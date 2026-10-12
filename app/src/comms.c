/* comms.c - command link task (USART2 + RS-485) and command dispatcher with fault-injection hooks. */
#include "app.h"
#include "bsp.h"
#include "dbg.h"
#include "fault.h"
#include "pwm.h"
#include "flash.h"
#include "iwdg.h"
#include "FreeRTOS.h"
#include <string.h>

frame_parser_t g_link_parser, g_rs485_parser;
volatile uint32_t g_isr_stamp_cyc;
static uint32_t s_isr_to_task_max_cyc;

void send_frame(uart_t *u, uint8_t type, uint8_t seq, const uint8_t *p, uint8_t n)
{
    uint8_t buf[FRAME_MAX_SIZE]; size_t len = frame_encode(type, seq, p, n, buf);
    if (len > 0u) { (void)uart_write(u, buf, len); }
}

static void ack(uart_t *u, const frame_t *f, const uint8_t *p, uint8_t n) { send_frame(u, (uint8_t)(f->type | MSG_ACK_FLAG), f->seq, p, n); }
static void nak(uart_t *u, const frame_t *f, uint8_t code) { send_frame(u, MSG_NAK, f->seq, &code, 1u); }

#if FC_ENABLE_FAULT_INJECTION
static volatile bool s_hang_sensor;
/* deliberate unbounded recursion (stack-overflow fault injection) */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winfinite-recursion"
static int recurse(int n) { volatile char pad[200]; pad[0] = (char)n; return recurse(n + 1) + pad[0]; }
#pragma GCC diagnostic pop
static void inject(uart_t *u, const frame_t *f)
{
    if (f->len < 1u) { nak(u, f, ERR_BAD_PARAM); return; }
    uint8_t code = f->payload[0]; uint32_t arg = 0u; if (f->len >= 5u) { memcpy(&arg, &f->payload[1], 4u); }
    dbg_printf("INJECT: code=%u arg=%lu\r\n", (unsigned)code, (unsigned long)arg);
    ack(u, f, NULL, 0u); uart_flush(u, 20000u);
    switch (code) {
    case 1: vTaskSuspend(g_task[TID_SENSOR]); break;                                  /* task stops checking in -> SW watchdog -> IWDG reset */
    case 2: { void (*fp)(void) = (void (*)(void))0; fp(); break; }                    /* HardFault: jump to null */
    case 3: (void)recurse(0); break;                                                  /* stack overflow in the comms task */
    case 4: sensors_freeze(arg != 0u); break;                                         /* stuck-at sensor */
    case 5: { volatile void *p = pvPortMalloc(0x7FFFFFFFu); (void)p; break; }         /* heap exhaustion -> malloc-failed hook */
    case 6: { volatile int z = (int)arg; volatile int r = 100 / z; (void)r; break; }  /* divide by zero (trap enabled) -> UsageFault */
    case 7: { volatile uint32_t *p = (volatile uint32_t *)0x20000001u; volatile uint32_t v = *p; (void)v; break; }   /* unaligned access trap */
    case 8: taskDISABLE_INTERRUPTS(); for (;;) { }                                    /* lock-up with IRQs off: only the IWDG can save us */
    case 9: { volatile uint32_t *p = (volatile uint32_t *)0xA0000000u; *p = 1u; break; }                         /* bus fault (write to unmapped region) */
    case 10: vTaskSuspend(g_task[TID_CONTROL]); break;
    case 11: vTaskSuspend(g_task[TID_HK]); break;                                     /* supervisor itself dies -> IWDG is the last line */
    case 12: configASSERT(0); break;
    default: break;
    }
}
#endif

void cmd_dispatch(uart_t *u, const frame_t *f)
{
    uint8_t out[FRAME_MAX_PAYLOAD];
    switch (f->type) {
    case MSG_PING: ack(u, f, NULL, 0u); break;
    case MSG_GET_VERSION: {
        version_info_t v = { FC_VERSION, FC_SECURITY_COUNTER, (uint8_t)app_fw_slot(), (uint8_t)g_fc.mode, 0u, 0u, app_image_confirmed() ? 1u : 0u };
        memcpy(out, &v, sizeof v); ack(u, f, out, (uint8_t)sizeof v); break; }
    case MSG_GET_TELEM: { telem_t t; telemetry_snapshot(&t); ack(u, f, (const uint8_t *)&t, (uint8_t)sizeof t); break; }
    case MSG_SET_MODE:
        if (f->len != 1u) { nak(u, f, ERR_BAD_PARAM); break; }
        if (f->payload[0] == 1u) { mode_post_event(EV_CMD_SAFE); } else if (f->payload[0] == 0u) { mode_post_event(EV_CMD_RECOVER); } else { nak(u, f, ERR_BAD_PARAM); break; }
        ack(u, f, NULL, 0u); break;
    case MSG_SET_SENSOR_SRC:
        if ((f->len != 1u) || (f->payload[0] > 2u)) { nak(u, f, ERR_BAD_PARAM); break; }
        sensors_set_source((sensor_src_t)f->payload[0]); ack(u, f, NULL, 0u); break;
    case MSG_CAN_NODE_CMD: {
        if (f->len != 1u) { nak(u, f, ERR_BAD_PARAM); break; }
        can_msg_t m = { CANID_OBC_CMD, false, false, 1u, { f->payload[0], 0u, 0u, 0u, 0u, 0u, 0u, 0u }, 0u };
        if (can_send(&g_can, &m, 5000u) == 0) { ack(u, f, NULL, 0u); } else { nak(u, f, ERR_NOT_SUPPORTED); }
        break; }
    case MSG_HIL_SENSOR: sensors_hil_push(f->payload, f->len); break;                  /* no response: 100 Hz stream */
    case MSG_REBOOT: ack(u, f, NULL, 0u); uart_flush(u, 50000u); bsp_system_reset(); break;
    case MSG_REQ_UPDATE: ack(u, f, NULL, 0u); uart_flush(u, 50000u); bsp_shared_set(SHARED_REQ_UPDATE, 0u); bsp_system_reset(); break;
    case MSG_DUMP_FAULT: {
        fault_record_t r; uint32_t cnt = fault_log_count();
        if (f->len == 0u) { memcpy(out, &cnt, 4u); ack(u, f, out, 4u); break; }      /* no index: return the record count */
        if (fault_log_read(f->payload[0], &r) != FLASH_OK) { nak(u, f, ERR_BAD_PARAM); break; }
        memcpy(out, &r, sizeof r); ack(u, f, out, (uint8_t)sizeof r); break; }
    case MSG_CLEAR_FAULTLOG: if (fault_log_clear() == FLASH_OK) { ack(u, f, NULL, 0u); } else { nak(u, f, ERR_FLASH); } break;
    case MSG_GET_STATS: { stats_t s; extern void stats_fill(stats_t *); stats_fill(&s); ack(u, f, (const uint8_t *)&s, (uint8_t)sizeof s); break; }
    case MSG_GET_LATENCY: { latency_t l; latency_get(&l); ack(u, f, (const uint8_t *)&l, (uint8_t)sizeof l); break; }
#if FC_ENABLE_FAULT_INJECTION
    case MSG_INJECT_FAULT: inject(u, f); break;
#endif
    default: nak(u, f, ERR_UNKNOWN_CMD); break;
    }
}

static void rx_wake_cb(void *ctx)
{
    (void)ctx;
    g_isr_stamp_cyc = dwt_cycles();
    BaseType_t hp = pdFALSE;
    if (g_task[TID_COMMS] != NULL) { vTaskNotifyGiveFromISR(g_task[TID_COMMS], &hp); }
    portYIELD_FROM_ISR(hp);
}

void comms_install_rx_callbacks(void)
{
    g_link.rx_cb = rx_wake_cb; g_rs485.rx_cb = rx_wake_cb;
}

void comms_task(void *arg)
{
    (void)arg;
    frame_parser_init(&g_link_parser); frame_parser_init(&g_rs485_parser);
    uint32_t last_rx_ms = 0u; uint8_t b;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
        uint32_t lat = dwt_cycles() - g_isr_stamp_cyc;                           /* ISR -> task wake-up latency (cycles) */
        if ((lat > s_isr_to_task_max_cyc) && (lat < 0x10000000u)) { s_isr_to_task_max_cyc = lat; }
        uint32_t now = app_now_ms();
        bool any = false;
        while (uart_read_byte(&g_link, &b)) { any = true; if (frame_parser_feed(&g_link_parser, b)) { cmd_dispatch(&g_link, &g_link_parser.f); } }
        while (uart_read_byte(&g_rs485, &b)) { any = true; if (frame_parser_feed(&g_rs485_parser, b)) { cmd_dispatch(&g_rs485, &g_rs485_parser.f); } }
        if (any) { last_rx_ms = now; }
        else if ((last_rx_ms != 0u) && ((uint32_t)(now - last_rx_ms) > 100u)) {   /* inter-byte timeout: drop a stalled partial frame */
            g_link_parser.state = 0u; g_rs485_parser.state = 0u;
        }
        WDG_CHECKIN(TID_COMMS);
    }
}
uint32_t comms_isr_to_task_max_cycles(void) { return s_isr_to_task_max_cyc; }
