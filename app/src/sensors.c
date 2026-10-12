/* sensors.c - sensor front-end: hardware drivers (SPI IMU, I2C baro), onboard simulation, or host-injected HIL data. */
#include "app.h"
#include "spi.h"
#include "i2c.h"
#include "bsp.h"
#include "dbg.h"
#include "sim_sensors.h"
#include <string.h>

static spi_t s_spi; static i2c_t s_i2c; static imu_t s_imu; static baro_t s_baro;
static bus_t s_imu_bus, s_baro_bus;
static volatile sensor_src_t s_src = SRC_SIM;
static volatile bool s_freeze;
static imu_sample_t s_frozen_imu; static float s_frozen_p, s_frozen_t;
static sim_t s_sim; static uint32_t s_sim_t0;
static uint32_t s_imu_fail, s_baro_fail;
bool g_hw_sensors_present;

typedef struct { imu_sample_t imu; float p, t; uint32_t rx_us; bool valid; } hil_t;
static volatile hil_t s_hil;
#define HIL_MAX_AGE_US 100000u

int sensors_init(void)
{
    spi_init(&s_spi, SPI1, 3u, 7u, GPIOE, 3u);                     /* mode 3, PCLK2/128 = 656 kHz */
    s_imu_bus = (bus_t){ &s_spi, spi_xfer, NULL };
    i2c_init(&s_i2c, I2C1, 400000u);
    s_baro_bus = (bus_t){ &s_i2c, NULL, i2c_wr_rd };
    int ri = imu_init(&s_imu, &s_imu_bus), rb = baro_init(&s_baro, &s_baro_bus, 0x76u);
    g_hw_sensors_present = (ri == 0) && (rb == 0);
    sim_init(&s_sim, 0xC0FFEEu); s_sim_t0 = time_us();
    dbg_printf("SENS: imu_init=%d baro_init=%d -> hardware %s\r\n", ri, rb, g_hw_sensors_present ? "PRESENT" : "ABSENT");
    sensor_src_t want = (sensor_src_t)FC_SENSOR_DEFAULT;
    if ((want == SRC_HW) && !g_hw_sensors_present) {
#if FC_ALLOW_SIM_FALLBACK
        want = SRC_SIM; dbg_printf("SENS: falling back to onboard simulator\r\n");
        g_fc.flags |= TFLAG_SIM_FALLBACK;
#else
        g_fc.imu_ok = g_fc.baro_ok = false;
#endif
    }
    s_src = want; g_fc.src = want;
    return g_hw_sensors_present ? 0 : -1;
}

void sensors_set_source(sensor_src_t s) { s_src = s; g_fc.src = s; if (s == SRC_HIL) { g_fc.flags |= TFLAG_HIL_SOURCE; } else { g_fc.flags &= (uint16_t)~TFLAG_HIL_SOURCE; } }
void sensors_freeze(bool on) { s_freeze = on; }
uint32_t sensors_imu_fail_count(void) { return s_imu_fail; }
uint32_t sensors_baro_fail_count(void) { return s_baro_fail; }

void sensors_hil_push(const uint8_t *pl, uint8_t len)
{
    if (len < (9u * 4u)) { return; }
    float v[9]; memcpy(v, pl, sizeof v);
    hil_t h = { { v[0], v[1], v[2], v[3], v[4], v[5], 25.0f }, v[7], v[8], time_us(), true };
    (void)v[6];
    taskENTER_CRITICAL(); memcpy((void *)&s_hil, &h, sizeof h); taskEXIT_CRITICAL();
}

int sensors_read_imu(imu_sample_t *o)
{
    if (s_freeze) { *o = s_frozen_imu; return 0; }                  /* injected stuck-at fault */
    int rc = -1;
    switch (s_src) {
    case SRC_HW: rc = imu_read(&s_imu, o); break;
    case SRC_SIM: { float p, t, r, pi; sim_step(&s_sim, (float)(uint32_t)(time_us() - s_sim_t0) * 1e-6f, o, &p, &t, &r, &pi); rc = 0; break; }
    case SRC_HIL: {
        hil_t h; taskENTER_CRITICAL(); memcpy(&h, (const void *)&s_hil, sizeof h); taskEXIT_CRITICAL();
        if (h.valid && ((uint32_t)(time_us() - h.rx_us) < HIL_MAX_AGE_US)) { *o = h.imu; rc = 0; }
        break; }
    }
    if (rc != 0) { s_imu_fail++; } else { s_frozen_imu = *o; }
    return rc;
}

int sensors_read_baro(float *p, float *t)
{
    if (s_freeze) { *p = s_frozen_p; *t = s_frozen_t; return 0; }
    int rc = -1;
    switch (s_src) {
    case SRC_HW: rc = baro_read(&s_baro, p, t); break;
    case SRC_SIM: { imu_sample_t i; float r, pi; sim_step(&s_sim, (float)(uint32_t)(time_us() - s_sim_t0) * 1e-6f, &i, p, t, &r, &pi); rc = 0; break; }
    case SRC_HIL: {
        hil_t h; taskENTER_CRITICAL(); memcpy(&h, (const void *)&s_hil, sizeof h); taskEXIT_CRITICAL();
        if (h.valid && ((uint32_t)(time_us() - h.rx_us) < HIL_MAX_AGE_US)) { *p = h.p; *t = h.t; rc = 0; }
        break; }
    }
    if (rc != 0) { s_baro_fail++; } else { s_frozen_p = *p; s_frozen_t = *t; }
    return rc;
}
