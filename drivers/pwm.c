#include "pwm.h"
#include "gpio.h"
#include "bsp.h"

#define PWM_MIN_US 1000u
#define PWM_MAX_US 2000u
#define PWM_NEUTRAL_US 1500u

void pwm_init_servo(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN; (void)RCC->APB1ENR;
    for (uint8_t p = 12u; p <= 15u; p++) { gpio_config(GPIOD, p, GPIO_AF, GPIO_PUSHPULL, GPIO_FAST_SPEED, GPIO_NOPULL, 2); }
    uint32_t tim_clk = (g_pclk1_hz == g_sysclk_hz) ? g_pclk1_hz : (2u * g_pclk1_hz);
    TIM4->PSC = (tim_clk / 1000000u) - 1u;                       /* 1 MHz tick */
    TIM4->ARR = 19999u;                                          /* 20 ms period = 50 Hz */
    TIM4->CCMR1 = (6u << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE | (6u << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE;   /* PWM mode 1 */
    TIM4->CCMR2 = (6u << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE | (6u << TIM_CCMR2_OC4M_Pos) | TIM_CCMR2_OC4PE;
    TIM4->CCR1 = TIM4->CCR2 = TIM4->CCR3 = TIM4->CCR4 = PWM_NEUTRAL_US;
    TIM4->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;
    TIM4->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
    TIM4->EGR = TIM_EGR_UG;
}

void pwm_set_us(uint8_t ch, uint16_t us)
{
    if (us < PWM_MIN_US) { us = PWM_MIN_US; }
    if (us > PWM_MAX_US) { us = PWM_MAX_US; }
    switch (ch) { case 0: TIM4->CCR1 = us; break; case 1: TIM4->CCR2 = us; break; case 2: TIM4->CCR3 = us; break; case 3: TIM4->CCR4 = us; break; default: break; }
}
uint16_t pwm_get_us(uint8_t ch)
{
    switch (ch) { case 0: return (uint16_t)TIM4->CCR1; case 1: return (uint16_t)TIM4->CCR2; case 2: return (uint16_t)TIM4->CCR3; case 3: return (uint16_t)TIM4->CCR4; default: return 0u; }
}
void pwm_disable_outputs(void) { TIM4->CCER = 0u; for (uint8_t p = 12u; p <= 15u; p++) { gpio_config(GPIOD, p, GPIO_OUT, GPIO_PUSHPULL, GPIO_FAST_SPEED, GPIO_PULLDOWN, 0); gpio_clear(GPIOD, p); } }
