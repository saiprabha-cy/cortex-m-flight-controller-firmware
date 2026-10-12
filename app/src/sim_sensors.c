#include "sim_sensors.h"
#include <math.h>

void sim_init(sim_t *s, uint32_t seed) { s->rng = seed; s->gyro_bias_x = 0.03f; s->gyro_bias_y = -0.02f; }

static float noise(sim_t *s, float amp)
{
    s->rng = (s->rng * 1664525u) + 1013904223u;
    return amp * ((((float)(s->rng >> 8)) / 8388608.0f) - 1.0f);
}

void sim_step(sim_t *s, float t, imu_sample_t *imu, float *p, float *temp, float *tr, float *tp)
{
    float roll = 0.35f * sinf(0.6f * t), pitch = 0.25f * sinf(0.4f * t + 1.0f);
    float roll_rate = 0.35f * 0.6f * cosf(0.6f * t), pitch_rate = 0.25f * 0.4f * cosf(0.4f * t + 1.0f);
    imu->ax = -sinf(pitch) + noise(s, 0.02f);
    imu->ay = (sinf(roll) * cosf(pitch)) + noise(s, 0.02f);
    imu->az = (cosf(roll) * cosf(pitch)) + noise(s, 0.02f);
    imu->gx = roll_rate + s->gyro_bias_x + noise(s, 0.005f);
    imu->gy = pitch_rate + s->gyro_bias_y + noise(s, 0.005f);
    imu->gz = noise(s, 0.005f);
    imu->temp_c = 24.0f + noise(s, 0.2f);
    *p = 101325.0f + noise(s, 8.0f); *temp = 23.5f;
    *tr = roll; *tp = pitch;
}
