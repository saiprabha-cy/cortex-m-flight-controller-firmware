#include "kalman.h"

void kalman1d_init(kalman1d_t *k, float q_angle, float q_bias, float r_meas)
{
    k->angle = 0.0f; k->bias = 0.0f;
    k->P[0][0] = 1.0f; k->P[0][1] = 0.0f; k->P[1][0] = 0.0f; k->P[1][1] = 1.0f;
    k->q_angle = q_angle; k->q_bias = q_bias; k->r_meas = r_meas;
}

float kalman1d_update(kalman1d_t *k, float gyro_rate, float accel_angle, float dt)
{
    /* Predict: angle += (rate - bias) * dt ; F = [[1,-dt],[0,1]] */
    k->angle += (gyro_rate - k->bias) * dt;
    k->P[0][0] += dt * (dt * k->P[1][1] - k->P[0][1] - k->P[1][0] + k->q_angle);
    k->P[0][1] -= dt * k->P[1][1];
    k->P[1][0] -= dt * k->P[1][1];
    k->P[1][1] += k->q_bias * dt;

    /* Update with the accelerometer angle: H = [1, 0] */
    float S  = k->P[0][0] + k->r_meas;
    float K0 = k->P[0][0] / S;
    float K1 = k->P[1][0] / S;
    float y  = accel_angle - k->angle;
    k->angle += K0 * y;
    k->bias  += K1 * y;
    float p00 = k->P[0][0], p01 = k->P[0][1];
    k->P[0][0] -= K0 * p00; k->P[0][1] -= K0 * p01;
    k->P[1][0] -= K1 * p00; k->P[1][1] -= K1 * p01;
    return k->angle;
}

float pd_ctrl_step(const pd_ctrl_t *c, float angle_err, float rate)
{
    float u = (c->kp * angle_err) - (c->kd * rate);
    if (u > c->out_limit) { u = c->out_limit; }
    if (u < -c->out_limit) { u = -c->out_limit; }
    return u;
}
