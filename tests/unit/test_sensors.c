#include "unity.h"
#include "imu_mpu6500.h"
#include "baro_bmp280.h"
#include <string.h>
#include <math.h>
void setUp(void) {} void tearDown(void) {}

/* ---- software model of an MPU-6500 on SPI ---- */
typedef struct { uint8_t regs[256]; int fail; } mpu_model_t;
static int mpu_xfer(void *c, const uint8_t *tx, uint8_t *rx, size_t n)
{
    mpu_model_t *m = (mpu_model_t *)c; if (m->fail) { return BUS_ERR; }
    uint8_t reg = tx[0] & 0x7F; int rd = (tx[0] & 0x80) != 0; rx[0] = 0;
    for (size_t i = 1; i < n; i++) { if (rd) { rx[i] = m->regs[(reg + i - 1) & 0xFF]; } else { m->regs[reg] = tx[i]; rx[i] = 0; } }
    return BUS_OK;
}
static void put16(mpu_model_t *m, int reg, int16_t v) { m->regs[reg] = (uint8_t)((uint16_t)v >> 8); m->regs[reg + 1] = (uint8_t)v; }

void test_imu_init_and_read_scaling(void)
{
    mpu_model_t m; memset(&m, 0, sizeof m); m.regs[0x75] = 0x70; bus_t b = { &m, mpu_xfer, NULL }; imu_t d; imu_sample_t s;
    TEST_ASSERT_EQUAL(0, imu_init(&d, &b)); TEST_ASSERT_EQUAL_HEX8(0x01, m.regs[0x6B]); TEST_ASSERT_EQUAL_HEX8(0x08, m.regs[0x1B]);
    put16(&m, 0x3B, 0); put16(&m, 0x3D, 0); put16(&m, 0x3F, 8192); put16(&m, 0x41, 0);   /* az = 1 g (+-4g scale) */
    put16(&m, 0x43, 655); put16(&m, 0x45, -655); put16(&m, 0x47, 0);                    /* gx = +10 dps, gy = -10 dps */
    TEST_ASSERT_EQUAL(0, imu_read(&d, &s));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, s.az); TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.17453f, s.gx); TEST_ASSERT_FLOAT_WITHIN(1e-3f, -0.17453f, s.gy);
    TEST_ASSERT_TRUE(imu_sample_plausible(&s));
}
void test_imu_wrong_id_and_bus_error(void)
{
    mpu_model_t m; memset(&m, 0, sizeof m); m.regs[0x75] = 0x68; bus_t b = { &m, mpu_xfer, NULL }; imu_t d;
    TEST_ASSERT_EQUAL(0x68, imu_init(&d, &b)); m.regs[0x75] = 0x70; m.fail = 1; TEST_ASSERT_EQUAL(-1, imu_init(&d, &b));
    m.fail = 0; TEST_ASSERT_EQUAL(0, imu_init(&d, &b)); m.fail = 1; imu_sample_t s; TEST_ASSERT_EQUAL(-1, imu_read(&d, &s));
}
void test_imu_plausibility_rejects_stuck_garbage(void)
{
    imu_sample_t s = { 0, 0, 1, 0, 0, 0, 25 }; TEST_ASSERT_TRUE(imu_sample_plausible(&s));
    s.gx = NAN; TEST_ASSERT_FALSE(imu_sample_plausible(&s)); s.gx = 0; s.az = 7.9f; TEST_ASSERT_FALSE(imu_sample_plausible(&s));
    s.az = 1; s.temp_c = 200; TEST_ASSERT_FALSE(imu_sample_plausible(&s));
}

/* ---- BMP280 model loaded with the datasheet's worked example (Bosch BMP280 DS, sect. 3.12) ---- */
typedef struct { uint8_t regs[256]; } bmp_model_t;
static int bmp_i2c(void *c, uint8_t a, const uint8_t *tx, size_t txn, uint8_t *rx, size_t rxn)
{
    bmp_model_t *m = (bmp_model_t *)c; if (a != 0x76) { return BUS_ERR; }
    if (rxn == 0) { m->regs[tx[0]] = tx[1]; return BUS_OK; }
    for (size_t i = 0; i < rxn; i++) { rx[i] = m->regs[(tx[0] + i) & 0xFF]; } (void)txn; return BUS_OK;
}
static void p16(bmp_model_t *m, int r, int v) { m->regs[r] = (uint8_t)v; m->regs[r + 1] = (uint8_t)((unsigned)v >> 8); }
void test_baro_datasheet_example(void)
{
    bmp_model_t m; memset(&m, 0, sizeof m); m.regs[0xD0] = 0x58; bus_t b = { &m, NULL, bmp_i2c }; baro_t d; float p, t;
    p16(&m, 0x88, 27504); p16(&m, 0x8A, 26435); p16(&m, 0x8C, -1000); p16(&m, 0x8E, 36477); p16(&m, 0x90, -10685);
    p16(&m, 0x92, 3024); p16(&m, 0x94, 2855); p16(&m, 0x96, 140); p16(&m, 0x98, -7); p16(&m, 0x9A, 15500); p16(&m, 0x9C, -14600); p16(&m, 0x9E, 6000);
    int32_t adc_p = 415148, adc_t = 519888;
    m.regs[0xF7] = (uint8_t)(adc_p >> 12); m.regs[0xF8] = (uint8_t)(adc_p >> 4); m.regs[0xF9] = (uint8_t)(adc_p << 4);
    m.regs[0xFA] = (uint8_t)(adc_t >> 12); m.regs[0xFB] = (uint8_t)(adc_t >> 4); m.regs[0xFC] = (uint8_t)(adc_t << 4);
    TEST_ASSERT_EQUAL(0, baro_init(&d, &b, 0x76)); TEST_ASSERT_EQUAL_HEX8(0x57, m.regs[0xF4]);
    TEST_ASSERT_EQUAL(0, baro_read(&d, &p, &t));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.08f, t); TEST_ASSERT_FLOAT_WITHIN(1.0f, 100653.27f, p);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 0.0f, baro_altitude_m(101325.0f, 101325.0f));
}
void test_baro_wrong_id_and_wrong_address(void)
{
    bmp_model_t m; memset(&m, 0, sizeof m); m.regs[0xD0] = 0x60; bus_t b = { &m, NULL, bmp_i2c }; baro_t d;
    TEST_ASSERT_EQUAL(0x60, baro_init(&d, &b, 0x76)); TEST_ASSERT_EQUAL(-1, baro_init(&d, &b, 0x77));
}
int main(void) { UNITY_BEGIN(); RUN_TEST(test_imu_init_and_read_scaling); RUN_TEST(test_imu_wrong_id_and_bus_error);
    RUN_TEST(test_imu_plausibility_rejects_stuck_garbage); RUN_TEST(test_baro_datasheet_example); RUN_TEST(test_baro_wrong_id_and_wrong_address); return UNITY_END(); }
