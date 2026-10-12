/* latency.c - real-time performance instrumentation (DWT cycle counter).
 *  - ISR entry latency: pend a software interrupt and measure cycles until its handler runs.
 *  - Sensor period jitter, control-task execution time and sample->actuator latency.
 * NOTE: in an emulator these numbers reflect the emulated core, not silicon. Label them "emulated". */
#include "app.h"
#include "bsp.h"

static volatile uint32_t t_trig, lat_min = 0xFFFFFFFFu, lat_max, lat_n; static volatile uint64_t lat_sum;
static volatile uint32_t per_min = 0xFFFFFFFFu, per_max, ctrl_exec_max, ctrl_lat_max; static volatile uint64_t ctrl_sum; static volatile uint32_t ctrl_n;

void EXTI0_IRQHandler(void)
{
    uint32_t d = dwt_cycles() - t_trig;
    if (d < lat_min) { lat_min = d; }
    if (d > lat_max) { lat_max = d; }
    lat_sum += d; lat_n++;
}

void latency_probe_trigger(void)
{
    NVIC_SetPriority(EXTI0_IRQn, 6u); NVIC_EnableIRQ(EXTI0_IRQn);
    t_trig = dwt_cycles();
    NVIC_SetPendingIRQ(EXTI0_IRQn);
}

void latency_note_sensor_period(uint32_t us)
{
    if (us < per_min) { per_min = us; }
    if (us > per_max) { per_max = us; }
}
void latency_note_control(uint32_t exec_cycles, uint32_t latency_us)
{
    if (exec_cycles > ctrl_exec_max) { ctrl_exec_max = exec_cycles; }
    ctrl_sum += exec_cycles; ctrl_n++;
    if (latency_us > ctrl_lat_max) { ctrl_lat_max = latency_us; }
}
void latency_get(latency_t *l)
{
    uint32_t nominal = FC_SENSOR_PERIOD_MS * 1000u;
    l->isr_lat_min_cyc = (lat_n != 0u) ? lat_min : 0u; l->isr_lat_max_cyc = lat_max;
    l->isr_lat_avg_cyc = (lat_n != 0u) ? (uint32_t)(lat_sum / lat_n) : 0u; l->isr_lat_samples = lat_n;
    l->sensor_period_min_us = per_min; l->sensor_period_max_us = per_max;
    uint32_t dev_hi = (per_max > nominal) ? (per_max - nominal) : 0u, dev_lo = (nominal > per_min) ? (nominal - per_min) : 0u;
    l->sensor_jitter_max_us = (dev_hi > dev_lo) ? dev_hi : dev_lo;
    l->ctrl_exec_max_cyc = ctrl_exec_max; l->ctrl_exec_avg_cyc = (ctrl_n != 0u) ? (uint32_t)(ctrl_sum / ctrl_n) : 0u;
    l->ctrl_latency_max_us = ctrl_lat_max; l->sysclk_hz = g_sysclk_hz; l->dwt_is_hardware = g_dwt_hw ? 1u : 0u;
}
