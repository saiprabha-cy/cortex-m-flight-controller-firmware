/* sim_sensors.h - deterministic synthetic vehicle motion + sensor noise (used for emulation without
 * sensor models, and in host tests of the full estimation pipeline). */
#ifndef SIM_SENSORS_H
#define SIM_SENSORS_H
#include "imu_mpu6500.h"
typedef struct { uint32_t rng; float gyro_bias_x, gyro_bias_y; } sim_t;
void sim_init(sim_t *s, uint32_t seed);
/* t_s: time since start. Fills noisy IMU + baro; true roll/pitch (rad) returned for evaluation. */
void sim_step(sim_t *s, float t_s, imu_sample_t *imu, float *pressure_pa, float *temp_c, float *true_roll, float *true_pitch);
#endif
