/* housekeeping.c - supervisor: mode manager, task-watchdog supervision + IWDG kick policy, stats, image confirmation. */
#include "app.h"
#include "bsp.h"
#include "dbg.h"
#include "fault.h"
#include "iwdg.h"
#include "pwm.h"
#include "flash.h"
#include "../../common/boot_meta.h"
#include "../../common/image_header.h"
#include "timers.h"
#include <string.h>

extern bool irq_brownout_pending(void);
extern uint32_t comms_isr_to_task_max_cycles(void);
extern bool g_selftest_ok;

wdg_t g_wdg; int g_wdg_id[TID_COUNT];
uint32_t g_wdg_soft_miss_total, g_wdg_resets_seen;
extern uint32_t g_faults_this_boot;
static QueueHandle_t s_evq;
static bool s_confirmed;

void mode_queue_init(void) { s_evq = xQueueCreate(16, sizeof(sys_event_t)); }
void mode_post_event(sys_event_t ev) { if (s_evq != NULL) { (void)xQueueSend(s_evq, &ev, 0); } }
void mode_post_event_from_isr(sys_event_t ev) { BaseType_t hp = pdFALSE; if (s_evq != NULL) { (void)xQueueSendFromISR(s_evq, &ev, &hp); portYIELD_FROM_ISR(hp); } }
bool mode_event_receive(sys_event_t *ev) { return (xQueueReceive(s_evq, ev, 0) == pdTRUE); }

/* ---- which slot are we running from, and image confirmation ---- */
extern const void *const g_vector_table[];
uint32_t app_fw_slot(void)
{
    uint32_t a = (uint32_t)(uintptr_t)g_vector_table;
    return ((a >= SLOT_B_BASE) && (a < (SLOT_B_BASE + SLOT_SIZE))) ? SLOT_B : SLOT_A;
}
static bool running_from_slot(void)
{
    uint32_t a = (uint32_t)(uintptr_t)g_vector_table;
    return (a >= SLOT_A_BASE) && (a < (SLOT_B_BASE + SLOT_SIZE));
}
bool app_image_confirmed(void) { return s_confirmed; }

void app_confirm_image(void)
{
    if (!running_from_slot()) { s_confirmed = true; return; }                 /* started without bootloader (debug) */
    flashlog_t log; boot_meta_t m; uint32_t slot = app_fw_slot();
    if (flashlog_init(&log, &g_flash_hw_ops, META_BANK0_BASE, META_BANK_SIZE, META_BANK1_BASE, META_BANK_SIZE) != FLASH_OK) { return; }
    (void)boot_meta_load(&log, &m);
    const image_header_t *h = (const image_header_t *)(uintptr_t)(slot ? SLOT_B_BASE : SLOT_A_BASE);
    if (m.slot_state[slot] == SLOT_STATE_TRIAL) {
        boot_meta_mark_confirmed(&m, slot, h->security_counter);
        if (boot_meta_save(&log, &m) == FLASH_OK) { dbg_printf("HK: image in slot %c CONFIRMED (sec counter %lu)\r\n", slot ? 'B' : 'A', (unsigned long)h->security_counter); s_confirmed = true; }
    } else { s_confirmed = (m.slot_state[slot] == SLOT_STATE_CONFIRMED); }
    (void)fault_log_record(FAULT_NONE, 0xC0DEu, 0u, 0u, 0u, 0u);              /* "healthy" marker: resets the abnormal-reset counter */
}

/* ---- statistics ---- */
static uint32_t s_prev_idle, s_prev_total;
static void update_cpu_load(void)
{
    uint32_t idle = (uint32_t)ulTaskGetIdleRunTimeCounter(), total = time_us();
    uint32_t di = idle - s_prev_idle, dt = total - s_prev_total;
    s_prev_idle = idle; s_prev_total = total;
    uint32_t load = (dt != 0u) ? (100u - ((100u * ((di > dt) ? dt : di)) / dt)) : 0u;
    g_fc.cpu_load_pct = (uint8_t)load;
}

void stats_fill(stats_t *s)
{
    memset(s, 0, sizeof *s);
    s->uptime_ms = app_now_ms();
    for (int i = 0; i < TID_COUNT; i++) { s->stack_hwm_words[i] = (uint16_t)uxTaskGetStackHighWaterMark(g_task[i]); }
    s->stack_hwm_words[6] = (uint16_t)uxTaskGetStackHighWaterMark(xTimerGetTimerDaemonTaskHandle());
    s->stack_hwm_words[7] = (uint16_t)uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle());
    s->isr_stack_free_bytes = bsp_stack_free_bytes();
    s->heap_free = (uint32_t)xPortGetFreeHeapSize(); s->heap_min_free = (uint32_t)xPortGetMinimumEverFreeHeapSize();
    s->cpu_load_pct = g_fc.cpu_load_pct;
    s->frame_ok = g_link_parser.frames_ok; s->frame_crc_err = g_link_parser.crc_errors; s->frame_len_err = g_link_parser.len_errors; s->frame_resync = g_link_parser.resyncs;
    s->link_rx_overflow = g_link.rx_overflow; s->link_tx_overflow = g_link.tx_overflow; s->link_err_overrun = g_link.err_overrun; s->link_err_framing = g_link.err_framing;
    s->can_rx = g_can.rx_count; s->can_tx = g_can.tx_count; s->can_tx_fail = g_can.tx_fail; s->can_rx_overflow = g_can.rx_overflow;
    s->can_bus_off = g_can.bus_off_events; s->can_err_passive = g_can.error_passive_events;
    s->can_tec = (uint8_t)can_tec(&g_can); s->can_rec = (uint8_t)can_rec(&g_can);
    s->wdg_soft_miss = g_wdg_soft_miss_total; s->wdg_resets_seen = g_wdg_resets_seen; s->faults_logged = fault_log_count();
    s->sensor_imu_fail = sensors_imu_fail_count(); s->sensor_baro_fail = sensors_baro_fail_count();
}
void stats_isr_cycles_hook(void) { (void)comms_isr_to_task_max_cycles(); }

/* ---- the supervisor loop ---- */
void housekeeping_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    uint32_t n = 0u, nominal_since = 0u, last_fault_ms = 0u; bool first = true, hard_fail_reported = false;
    sys_mode_t mode = MODE_BOOT;

    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(FC_HK_PERIOD_MS));
        uint32_t now = app_now_ms();

        if (first) { first = false; mode_post_event(g_selftest_ok ? EV_SELF_TEST_OK : EV_SELF_TEST_FAIL); }

        /* 1. software task watchdog + hardware watchdog policy */
        wdg_result_t r = wdg_supervise(&g_wdg, now);
        if (r.newly_late_mask != 0u) {
            for (uint32_t i = 0u; i < TID_COUNT; i++) if ((r.newly_late_mask & (1u << i)) != 0u) {
                g_wdg_soft_miss_total++;
                (void)fault_log_record(FAULT_TASK_WDG, (uint16_t)i, 0u, 0u, 0u, 0u);
                dbg_printf("WDG: task %u missed its deadline (age %lu ms)\r\n", (unsigned)i, (unsigned long)r.worst_age_ms);
            }
            mode_post_event(EV_WDG_SOFT_MISS);
        }
        if (r.late_mask != 0u) { last_fault_ms = now; }
        if (!r.hard_fail) { iwdg_kick(); }
        else if (!hard_fail_reported) { hard_fail_reported = true; dbg_printf("WDG: HARD FAIL - withholding IWDG kick, reset imminent\r\n"); }

        /* 2. voltage monitor */
        if (irq_brownout_pending()) { (void)fault_log_record(FAULT_BROWNOUT, 0u, 0u, 0u, 0u, 0u); g_fc.flags |= TFLAG_BROWNOUT_SEEN; mode_post_event(EV_BROWNOUT); }

        /* 3. mode manager */
        sys_event_t ev;
        while (mode_event_receive(&ev)) {
            sys_mode_t nxt = mode_fsm_next(mode, ev);
            if (nxt != mode) {
                dbg_printf("MODE: %s --%s--> %s\r\n", mode_name(mode), event_name(ev), mode_name(nxt));
                mode = nxt; fault_set_mode((uint8_t)mode); fc_lock(); g_fc.mode = mode; fc_unlock();
                if (mode != MODE_NOMINAL) { nominal_since = 0u; }
            }
        }
        if ((g_fc.flags & (TFLAG_IMU_FAULT | TFLAG_BARO_FAULT | TFLAG_NODE_LOST)) != 0u) { last_fault_ms = now; }
        if ((mode == MODE_DEGRADED) && ((uint32_t)(now - last_fault_ms) > FC_RECOVER_AFTER_MS)) { mode_post_event(EV_RECOVERED); }

        /* 4. confirm the image after it has been healthy and NOMINAL long enough (rollback protection) */
        if (mode == MODE_NOMINAL) {
            if (nominal_since == 0u) { nominal_since = now; }
            if (!s_confirmed && ((uint32_t)(now - nominal_since) >= FC_CONFIRM_AFTER_MS)) {
#if FC_DEMO_CRASH_ON_BOOT
                (void)0;                                                      /* broken demo build never confirms */
#else
                app_confirm_image();
#endif
            }
        }
#if FC_DEMO_CRASH_ON_BOOT
        if (now > 3000u) { void (*fp)(void) = (void (*)(void))0; fp(); }       /* deliberately crash: exercises reset-limit + bootloader rollback */
#endif

        /* 5. 1 Hz housekeeping */
        if ((n % (1000u / FC_HK_PERIOD_MS)) == 0u) {
            update_cpu_load(); latency_probe_trigger();
            dbg_printf("HK: t=%lus mode=%s cpu=%u%% heap=%lu isr_stk_free=%lu roll=%d pitch=%d (centi-deg) src=%u\r\n",
                       (unsigned long)(now / 1000u), mode_name(mode), (unsigned)g_fc.cpu_load_pct, (unsigned long)xPortGetFreeHeapSize(),
                       (unsigned long)bsp_stack_free_bytes(), (int)(g_fc.roll * 5729.578f), (int)(g_fc.pitch * 5729.578f), (unsigned)g_fc.src);
        }
        n++;
        WDG_CHECKIN(TID_HK);
    }
}
