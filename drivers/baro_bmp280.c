#include "baro_bmp280.h"
#include <math.h>

#define REG_ID        0xD0u
#define REG_CALIB     0x88u
#define REG_CTRL_MEAS 0xF4u
#define REG_CONFIG    0xF5u
#define REG_PRESS_MSB 0xF7u

static int rd(baro_t *d, uint8_t reg, uint8_t *dst, size_t n)
{ return d->bus->i2c_wr_rd(d->bus->ctx, d->addr, &reg, 1u, dst, n); }
static int wr(baro_t *d, uint8_t reg, uint8_t v)
{ uint8_t t[2] = { reg, v }; return d->bus->i2c_wr_rd(d->bus->ctx, d->addr, t, 2u, NULL, 0u); }

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static int16_t  s16(const uint8_t *p) { return (int16_t)u16(p); }

int baro_init(baro_t *d, const bus_t *bus, uint8_t addr7)
{
    uint8_t id, c[24];
    d->bus = bus; d->addr = addr7; d->t_fine = 0;
    if (rd(d, REG_ID, &id, 1u) != BUS_OK) { return -1; }
    if (id != 0x58u) { return (int)id == 0 ? 0xFF : (int)id; }
    if (rd(d, REG_CALIB, c, 24u) != BUS_OK) { return -1; }
    d->T1 = u16(&c[0]); d->T2 = s16(&c[2]); d->T3 = s16(&c[4]);
    d->P1 = u16(&c[6]); d->P2 = s16(&c[8]); d->P3 = s16(&c[10]); d->P4 = s16(&c[12]); d->P5 = s16(&c[14]);
    d->P6 = s16(&c[16]); d->P7 = s16(&c[18]); d->P8 = s16(&c[20]); d->P9 = s16(&c[22]);
    if (wr(d, REG_CONFIG, 0x10u) != BUS_OK) { return -1; }            /* IIR filter x4 */
    if (wr(d, REG_CTRL_MEAS, 0x57u) != BUS_OK) { return -1; }         /* T x2, P x16, normal mode */
    return 0;
}

int baro_read(baro_t *d, float *pressure_pa, float *temp_c)
{
    uint8_t b[6];
    if (rd(d, REG_PRESS_MSB, b, 6u) != BUS_OK) { return -1; }
    int32_t adc_p = (int32_t)(((uint32_t)b[0] << 12) | ((uint32_t)b[1] << 4) | ((uint32_t)b[2] >> 4));
    int32_t adc_t = (int32_t)(((uint32_t)b[3] << 12) | ((uint32_t)b[4] << 4) | ((uint32_t)b[5] >> 4));

    int32_t v1 = ((((adc_t >> 3) - ((int32_t)d->T1 << 1))) * (int32_t)d->T2) >> 11;
    int32_t v2 = (((((adc_t >> 4) - (int32_t)d->T1) * ((adc_t >> 4) - (int32_t)d->T1)) >> 12) * (int32_t)d->T3) >> 14;
    d->t_fine = v1 + v2;
    *temp_c = (float)((d->t_fine * 5 + 128) >> 8) / 100.0f;

    int64_t p1 = (int64_t)d->t_fine - 128000;
    int64_t p2 = p1 * p1 * (int64_t)d->P6;
    p2 += (p1 * (int64_t)d->P5) << 17;
    p2 += ((int64_t)d->P4) << 35;
    p1 = ((p1 * p1 * (int64_t)d->P3) >> 8) + ((p1 * (int64_t)d->P2) << 12);
    p1 = ((((int64_t)1 << 47) + p1) * (int64_t)d->P1) >> 33;
    if (p1 == 0) { return -2; }                                       /* avoid division by zero */
    int64_t p = 1048576 - adc_p;
    p = (((p << 31) - p2) * 3125) / p1;
    p1 = ((int64_t)d->P9 * (p >> 13) * (p >> 13)) >> 25;
    p2 = ((int64_t)d->P8 * p) >> 19;
    p = ((p + p1 + p2) >> 8) + ((int64_t)d->P7 << 4);
    *pressure_pa = (float)p / 256.0f;
    return 0;
}

float baro_altitude_m(float pressure_pa, float sea_level_pa)
{
    return 44330.0f * (1.0f - powf(pressure_pa / sea_level_pa, 0.190295f));
}
