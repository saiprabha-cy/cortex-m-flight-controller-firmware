/* pwm.h - TIM4 4-channel PWM on PD12..PD15 (AF2): 50 Hz servo/ESC style, 1 us resolution. */
#ifndef PWM_H
#define PWM_H
#include <stdint.h>
void pwm_init_servo(void);                         /* 50 Hz, pulses 1000..2000 us, default 1500 */
void pwm_set_us(uint8_t channel, uint16_t pulse_us);   /* channel 0..3; clamped to [1000, 2000] */
uint16_t pwm_get_us(uint8_t channel);
void pwm_disable_outputs(void);                    /* safe-mode: outputs low / neutral */
#endif
