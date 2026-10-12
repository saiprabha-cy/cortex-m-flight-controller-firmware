#include "gpio.h"

void gpio_clock_enable(GPIO_TypeDef *port)
{
    uint32_t idx = ((uint32_t)port - (uint32_t)GPIOA) / 0x400u;
    RCC->AHB1ENR |= (1u << idx);
    (void)RCC->AHB1ENR;                                 /* read-back: let the clock settle (errata-safe) */
}

void gpio_config(GPIO_TypeDef *port, uint8_t pin, gpio_mode_t mode, gpio_otype_t ot, gpio_speed_t sp, gpio_pull_t pu, uint8_t af)
{
    gpio_clock_enable(port);
    uint32_t s2 = 2u * pin;
    port->MODER   = (port->MODER   & ~(3u << s2)) | ((uint32_t)mode << s2);
    port->OTYPER  = (port->OTYPER  & ~(1u << pin)) | ((uint32_t)ot << pin);
    port->OSPEEDR = (port->OSPEEDR & ~(3u << s2)) | ((uint32_t)sp << s2);
    port->PUPDR   = (port->PUPDR   & ~(3u << s2)) | ((uint32_t)pu << s2);
    if (mode == GPIO_AF) {
        uint32_t reg = pin >> 3, sh = 4u * (pin & 7u);
        port->AFR[reg] = (port->AFR[reg] & ~(0xFu << sh)) | ((uint32_t)af << sh);
    }
}
