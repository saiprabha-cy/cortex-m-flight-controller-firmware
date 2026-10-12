/* gpio.h - register-level GPIO driver (RM0090 section 8). No HAL. */
#ifndef GPIO_H
#define GPIO_H
#include "stm32f4xx.h"
typedef enum { GPIO_IN = 0, GPIO_OUT = 1, GPIO_AF = 2, GPIO_ANALOG = 3 } gpio_mode_t;
typedef enum { GPIO_PUSHPULL = 0, GPIO_OPENDRAIN = 1 } gpio_otype_t;
typedef enum { GPIO_LOW_SPEED = 0, GPIO_MED_SPEED = 1, GPIO_FAST_SPEED = 2, GPIO_HIGH_SPEED = 3 } gpio_speed_t;
typedef enum { GPIO_NOPULL = 0, GPIO_PULLUP = 1, GPIO_PULLDOWN = 2 } gpio_pull_t;

void gpio_clock_enable(GPIO_TypeDef *port);
void gpio_config(GPIO_TypeDef *port, uint8_t pin, gpio_mode_t mode, gpio_otype_t ot, gpio_speed_t sp, gpio_pull_t pu, uint8_t af);
static inline void gpio_set(GPIO_TypeDef *p, uint8_t pin)    { p->BSRR = (1u << pin); }
static inline void gpio_clear(GPIO_TypeDef *p, uint8_t pin)  { p->BSRR = (1u << (pin + 16u)); }
static inline void gpio_toggle(GPIO_TypeDef *p, uint8_t pin) { p->ODR ^= (1u << pin); }
static inline int  gpio_read(GPIO_TypeDef *p, uint8_t pin)   { return (int)((p->IDR >> pin) & 1u); }
#endif
