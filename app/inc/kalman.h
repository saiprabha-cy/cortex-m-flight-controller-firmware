/* kalman.h - 2-state (angle, gyro bias) Kalman filter per axis + PD attitude controller.
 * Pure float C, no hardware dependency: runs identically on host (unit tests) and on the Cortex-M4F. */
#ifndef KALMAN_H
#define KALMAN_H
typedef struct {
    float angle, bias;          /* state */
    float P[2][2];              /* covariance */
    float q_angle, q_bias, r_meas;
} kalman1d_t;

void  kalman1d_init(kalman1d_t *k, float q_angle, float q_bias, float r_meas);
/* gyro_rate [rad/s], accel_angle [rad] (measurement), dt [s]. Returns the filtered angle. */
float kalman1d_update(kalman1d_t *k, float gyro_rate, float accel_angle, float dt);

typedef struct { float kp, kd, out_limit; } pd_ctrl_t;
float pd_ctrl_step(const pd_ctrl_t *c, float angle_err, float rate);   /* saturated output */
#endif
