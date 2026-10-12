#include "unity.h"
#include "kalman.h"
#include <math.h>
void setUp(void) {} void tearDown(void) {}
static uint32_t lcg = 12345u;
static float noise(float amp) { lcg = lcg * 1664525u + 1013904223u; return amp * (((float)(lcg >> 8) / 8388608.0f) - 1.0f); }

void test_filter_tracks_angle_and_estimates_gyro_bias(void)
{
    kalman1d_t k; kalman1d_init(&k, 0.001f, 0.003f, 0.03f);
    const float dt = 0.01f, true_bias = 0.05f;              /* 0.05 rad/s gyro bias */
    float err_sum = 0.0f; int n = 0;
    for (int i = 0; i < 6000; i++) {
        float t = (float)i * dt, th = 0.5f * sinf(0.8f * t), w = 0.4f * cosf(0.8f * t);
        float est = kalman1d_update(&k, w + true_bias + noise(0.01f), th + noise(0.15f), dt);
        if (i > 2000) { err_sum += fabsf(est - th); n++; }
    }
    TEST_ASSERT_TRUE_MESSAGE((err_sum / (float)n) < 0.03f, "mean tracking error > 0.03 rad");
    TEST_ASSERT_FLOAT_WITHIN(0.015f, true_bias, k.bias);
}
void test_filter_beats_raw_accelerometer(void)
{
    kalman1d_t k; kalman1d_init(&k, 0.001f, 0.003f, 0.03f);
    float e_raw = 0, e_kf = 0; int n = 0;
    for (int i = 0; i < 5000; i++) {
        float t = (float)i * 0.01f, th = 0.3f * sinf(t), w = 0.3f * cosf(t), meas = th + noise(0.2f);
        float est = kalman1d_update(&k, w + noise(0.01f), meas, 0.01f);
        if (i > 1000) { e_raw += fabsf(meas - th); e_kf += fabsf(est - th); n++; }
    }
    TEST_ASSERT_TRUE(e_kf < 0.5f * e_raw);
}
void test_covariance_stays_finite_and_positive(void)
{
    kalman1d_t k; kalman1d_init(&k, 0.001f, 0.003f, 0.03f);
    for (int i = 0; i < 100000; i++) { kalman1d_update(&k, noise(1.0f), noise(1.0f), 0.01f); }
    TEST_ASSERT_TRUE(isfinite(k.P[0][0]) && isfinite(k.P[1][1])); TEST_ASSERT_TRUE(k.P[0][0] > 0.0f && k.P[1][1] > 0.0f);
}
void test_pd_saturation_and_sign(void)
{
    pd_ctrl_t c = { 4.0f, 1.0f, 0.5f };
    TEST_ASSERT_EQUAL_FLOAT(0.5f, pd_ctrl_step(&c, 10.0f, 0.0f)); TEST_ASSERT_EQUAL_FLOAT(-0.5f, pd_ctrl_step(&c, -10.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.2f, pd_ctrl_step(&c, 0.1f, 0.2f));
}
void test_closed_loop_settles_double_integrator(void)
{
    pd_ctrl_t c = { 6.0f, 3.0f, 2.0f }; float th = 0.6f, w = 0.0f;
    for (int i = 0; i < 1000; i++) { float u = pd_ctrl_step(&c, 0.0f - th, w); w += u * 0.01f; th += w * 0.01f; }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, th);
}
int main(void) { UNITY_BEGIN(); RUN_TEST(test_filter_tracks_angle_and_estimates_gyro_bias); RUN_TEST(test_filter_beats_raw_accelerometer);
    RUN_TEST(test_covariance_stays_finite_and_positive); RUN_TEST(test_pd_saturation_and_sign); RUN_TEST(test_closed_loop_settles_double_integrator); return UNITY_END(); }
