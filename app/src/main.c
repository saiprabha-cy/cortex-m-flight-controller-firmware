/* main.c - application entry: board bring-up, self-test, task creation, RTOS hooks. */
#include "app.h"
#include "bsp.h"
#include "dbg.h"
#include "fault.h"
#include "flash.h"
#include "gpio.h"
#include "iwdg.h"
#include "pwm.h"
#include "../../common/boot_meta.h"
#include "../../common/fault_types.h"
#include "../../common/image_header.h"

fc_state_t g_fc; SemaphoreHandle_t g_fc_mx;
uart_t g_link, g_dbg, g_rs485; can_t g_can;
TaskHandle_t g_task[TID_COUNT];
bool g_selftest_ok; uint32_t g_boot_slot;
uint32_t g_faults_this_boot;

extern void mode_queue_init(void); extern void comms_install_rx_callbacks(void); extern void pvd_enable(void);
extern void can_task_set_ok(bool ok);

#define TASK_STACK_WORDS 512u

static void create_tasks(void)
{
    static const struct { TaskFunction_t fn; const char *name; uint16_t stack; UBaseType_t prio; uint32_t deadline_ms; } T[TID_COUNT] = {
        { sensor_task,       "sensor",  TASK_STACK_WORDS, PRIO_SENSOR,  4u * FC_SENSOR_PERIOD_MS + 40u },
        { control_task,      "control", TASK_STACK_WORDS, PRIO_CONTROL, 300u },
        { comms_task,        "comms",   TASK_STACK_WORDS, PRIO_COMMS,   300u },
        { telemetry_task,    "telem",   TASK_STACK_WORDS, PRIO_TELEM,   3u * FC_TELEM_PERIOD_MS },
        { can_task,          "can",     TASK_STACK_WORDS, PRIO_CAN,     200u },
        { housekeeping_task, "hk",      TASK_STACK_WORDS, PRIO_HK,      3u * FC_HK_PERIOD_MS },
    };
    wdg_init(&g_wdg, FC_WDG_GRACE_MS);
    for (int i = 0; i < TID_COUNT; i++) {
        g_wdg_id[i] = wdg_register(&g_wdg, T[i].name, T[i].deadline_ms, 0u);
        if (xTaskCreate(T[i].fn, T[i].name, T[i].stack, NULL, T[i].prio, &g_task[i]) != pdPASS) { app_assert_failed("task create", i); }
    }
}

static void log_boot_and_policy(reset_cause_t rc, clk_status_t clk)
{
    dbg_printf("\r\n=== FLIGHT CONTROLLER FW v%u.%u.%u  slot=%c  reset=%s  clk=%s ===\r\n", (unsigned)FW_VER_MAJOR(FC_VERSION), (unsigned)FW_VER_MINOR(FC_VERSION),
               (unsigned)FW_VER_PATCH(FC_VERSION), app_fw_slot() ? 'B' : 'A', bsp_reset_cause_name(rc), (clk == CLK_PLL_168MHZ) ? "168MHz" : "HSI-fallback");
    switch (rc) {
    case RST_IWDG: case RST_WWDG: (void)fault_log_record(FAULT_WDG_RESET, 0u, 0u, 0u, 0u, 0u); g_wdg_resets_seen++; break;
    case RST_BROWN_OUT: (void)fault_log_record(FAULT_BROWNOUT, 0u, 0u, 0u, 0u, 0u); break;
    case RST_POWER_ON: case RST_PIN: (void)fault_log_record(FAULT_COLD_BOOT, 0u, 0u, 0u, 0u, 0u); break;
    case RST_SOFTWARE: (void)fault_log_record(FAULT_SOFT_RESET, 0u, 0u, 0u, 0u, 0u); break;
    default: break;
    }
    uint32_t n = fault_log_count(), start = (n > 64u) ? (n - 64u) : 0u; uint8_t types[64]; uint32_t k = 0u;
    for (uint32_t i = start; i < n; i++) { fault_record_t r; if (fault_log_read(i, &r) == FLASH_OK) { types[k++] = r.type; } }
    uint32_t abnormal = fault_policy_trailing_abnormal(types, k);
    dbg_printf("BOOT: %lu fault records, %lu consecutive abnormal resets\r\n", (unsigned long)n, (unsigned long)abnormal);
    if (abnormal >= FC_RESET_LIMIT) { dbg_printf("BOOT: reset limit reached -> SAFE mode\r\n"); mode_post_event(EV_WDG_RESET_LIMIT); }
}

int main(void)
{
    clk_status_t clk = bsp_clock_init();
    bsp_timebase_init(); dwt_init(); bsp_enable_fault_handlers();
    reset_cause_t rc = bsp_reset_cause();
    bsp_mark_running();                                    /* from here on, a reset we did not ask for counts as abnormal */

    gpio_config(GPIOA, 9, GPIO_AF, GPIO_PUSHPULL, GPIO_FAST_SPEED, GPIO_PULLUP, 7);           /* USART1_TX  debug console */
    gpio_config(GPIOA, 2, GPIO_AF, GPIO_PUSHPULL, GPIO_FAST_SPEED, GPIO_PULLUP, 7);           /* USART2_TX  command/telemetry link */
    gpio_config(GPIOA, 3, GPIO_AF, GPIO_PUSHPULL, GPIO_FAST_SPEED, GPIO_PULLUP, 7);           /* USART2_RX */
    gpio_config(GPIOB, 10, GPIO_AF, GPIO_PUSHPULL, GPIO_FAST_SPEED, GPIO_PULLUP, 7);          /* USART3_TX  RS-485 */
    gpio_config(GPIOB, 11, GPIO_AF, GPIO_PUSHPULL, GPIO_FAST_SPEED, GPIO_PULLUP, 7);          /* USART3_RX */
    uart_init(&g_dbg, USART1, FC_DBG_BAUD, g_pclk2_hz); dbg_init(&g_dbg);
    uart_init(&g_link, USART2, FC_LINK_BAUD, g_pclk1_hz);
#if FC_USE_DMA
    uart_enable_dma_tx(&g_link);
#endif
    uart_init(&g_rs485, USART3, FC_RS485_BAUD, g_pclk1_hz); uart_set_rs485(&g_rs485, GPIOB, 12u);
    comms_install_rx_callbacks();

    g_fc_mx = xSemaphoreCreateMutex(); mode_queue_init();
    g_fc.mode = MODE_BOOT; g_fc.act_us[0] = g_fc.act_us[1] = g_fc.act_us[2] = g_fc.act_us[3] = 1500u;
    if (clk != CLK_PLL_168MHZ) { g_fc.flags |= TFLAG_CLOCK_FALLBACK; }

    int fl = fault_log_init(&g_flash_hw_ops);
    log_boot_and_policy(rc, clk);

    bool ok = (fl == FLASH_OK);
    adc_init(); pvd_enable();
    (void)sensors_init();                                    /* absent hardware is tolerated when the simulator fallback is allowed */
    int cr = can_init(&g_can, CAN1, FC_CAN_BITRATE, false);
    can_task_set_ok(cr == 0); dbg_printf("CAN: init %s\r\n", (cr == 0) ? "ok" : "FAILED (CAN disabled)");
    g_selftest_ok = ok;

    create_tasks();
    iwdg_start(FC_IWDG_TIMEOUT_MS);                          /* from here on the hardware watchdog is armed */
    dbg_printf("BOOT: init done, starting scheduler (IWDG %u ms)\r\n", (unsigned)FC_IWDG_TIMEOUT_MS);
    vTaskStartScheduler();
    for (;;) { }
}

/* ------------- RTOS hooks and assertions: log, then fail safe by resetting ------------- */
void vApplicationStackOverflowHook(TaskHandle_t t, char *name)
{
    (void)t; __disable_irq();
    dbg_polled_printf(USART1, "\r\n*** STACK OVERFLOW in task %s ***\r\n", name);
    (void)fault_log_record(FAULT_STACK_OVF, 0u, 0u, 0u, 0u, 0u);
    bsp_system_reset();
}
void vApplicationMallocFailedHook(void)
{
    __disable_irq();
    dbg_polled_printf(USART1, "\r\n*** MALLOC FAILED ***\r\n");
    (void)fault_log_record(FAULT_MALLOC, 0u, 0u, 0u, 0u, 0u);
    bsp_system_reset();
}
void app_assert_failed(const char *file, int line)
{
    __disable_irq();
    dbg_polled_printf(USART1, "\r\n*** ASSERT %s:%d ***\r\n", file, line);
    (void)fault_log_record(FAULT_ASSERT, (uint16_t)line, 0u, 0u, 0u, 0u);
    bsp_system_reset();
}
