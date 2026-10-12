#include "imu_mpu6500.h"
#include <math.h>

#define REG_WHO_AM_I      0x75u
#define REG_PWR_MGMT_1    0x6Bu
#define REG_GYRO_CONFIG   0x1Bu
#define REG_ACCEL_CONFIG  0x1Cu
#define REG_ACCEL_XOUT_H  0x3Bu
#define WHO_AM_I_VALUE    0x70u
#define SPI_READ_FLAG     0x80u
#define DEG2RAD           0.017453292519943295f

static int wr(imu_t *d, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { (uint8_t)(reg & 0x7Fu), val }, rx[2];
    return d->bus->spi_xfer(d->bus->ctx, tx, rx, 2u);
}

static int rd(imu_t *d, uint8_t reg, uint8_t *dst, size_t n)
{
    uint8_t tx[15] = { (uint8_t)(reg | SPI_READ_FLAG) }, rx[15];
    if (n > 14u) { return BUS_ERR; }
    if (d->bus->spi_xfer(d->bus->ctx, tx, rx, n + 1u) != BUS_OK) { return BUS_ERR; }
    for (size_t i = 0u; i < n; i++) { dst[i] = rx[i + 1u]; }
    return BUS_OK;
}

int imu_init(imu_t *d, const bus_t *bus)
{
    uint8_t id;
    d->bus = bus;
    if (rd(d, REG_WHO_AM_I, &id, 1u) != BUS_OK) { return -1; }
    if (id != WHO_AM_I_VALUE) { return (int)id == 0 ? 0xFF : (int)id; }
    if (wr(d, REG_PWR_MGMT_1, 0x01u) != BUS_OK) { return -1; }        /* wake, PLL clock */
    if (wr(d, REG_GYRO_CONFIG, 0x08u) != BUS_OK) { return -1; }       /* +-500 dps */
    if (wr(d, REG_ACCEL_CONFIG, 0x08u) != BUS_OK) { return -1; }      /* +-4 g */
    d->gyro_lsb_per_dps = 65.5f;
    d->accel_lsb_per_g = 8192.0f;
    return 0;
}

int imu_read(imu_t *d, imu_sample_t *s)
{
    uint8_t b[14];
    if (rd(d, REG_ACCEL_XOUT_H, b, 14u) != BUS_OK) { return -1; }
    int16_t v[7];
    for (size_t i = 0u; i < 7u; i++) { v[i] = (int16_t)(((uint16_t)b[2u*i] << 8) | b[2u*i + 1u]); }
    s->ax = (float)v[0] / d->accel_lsb_per_g; s->ay = (float)v[1] / d->accel_lsb_per_g; s->az = (float)v[2] / d->accel_lsb_per_g;
    s->temp_c = ((float)v[3] / 333.87f) + 21.0f;
    s->gx = ((float)v[4] / d->gyro_lsb_per_dps) * DEG2RAD;
    s->gy = ((float)v[5] / d->gyro_lsb_per_dps) * DEG2RAD;
    s->gz = ((float)v[6] / d->gyro_lsb_per_dps) * DEG2RAD;
    return 0;
}

bool imu_sample_plausible(const imu_sample_t *s)
{
    const float f[7] = { s->ax, s->ay, s->az, s->gx, s->gy, s->gz, s->temp_c };
    for (int i = 0; i < 7; i++) { if (!isfinite(f[i])) { return false; } }
    return (fabsf(s->ax) <= 4.1f) && (fabsf(s->ay) <= 4.1f) && (fabsf(s->az) <= 4.1f) &&
           (fabsf(s->gx) <= 9.0f) && (fabsf(s->gy) <= 9.0f) && (fabsf(s->gz) <= 9.0f) &&
           (s->temp_c > -60.0f) && (s->temp_c < 125.0f);
}
