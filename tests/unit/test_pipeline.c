/* End-to-end estimation pipeline test on the host: simulated sensors -> IMU register encoding (MPU model)
 * -> driver -> attitude math -> Kalman. Same C files that run on the Cortex-M4F. */
#include "unity.h"
#include "sim_sensors.h"
#include "kalman.h"
#include <math.h>
void setUp(void) {} void tearDown(void) {}
void test_sim_plus_kalman_tracks_truth(void)
{
    sim_t s; sim_init(&s, 42); kalman1d_t kr, kp; kalman1d_init(&kr, 0.001f, 0.003f, 0.03f); kalman1d_init(&kp, 0.001f, 0.003f, 0.03f);
    float er = 0, ep = 0; int n = 0;
    for (int i = 0; i < 4000; i++) {
        float t = (float)i * 0.01f, P, T, tr, tp; imu_sample_t m; sim_step(&s, t, &m, &P, &T, &tr, &tp);
        float ra = atan2f(m.ay, m.az), pa = atan2f(-m.ax, sqrtf(m.ay * m.ay + m.az * m.az));
        float r = kalman1d_update(&kr, m.gx, ra, 0.01f), p = kalman1d_update(&kp, m.gy, pa, 0.01f);
        if (i > 1500) { er += fabsf(r - tr); ep += fabsf(p - tp); n++; }
    }
    TEST_ASSERT_TRUE(er / (float)n < 0.02f); TEST_ASSERT_TRUE(ep / (float)n < 0.02f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.03f, kr.bias); TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.02f, kp.bias);
}
int main(void) { UNITY_BEGIN(); RUN_TEST(test_sim_plus_kalman_tracks_truth); return UNITY_END(); }
