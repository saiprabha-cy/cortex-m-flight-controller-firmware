/* i2c.h - I2C1 master (RM0090 section 27), polled with timeouts and bus-recovery (9 SCL pulses + SWRST). */
#ifndef I2C_H
#define I2C_H
#include "gpio.h"
#include "bus_if.h"
typedef struct { I2C_TypeDef *reg; uint32_t timeout_us; uint32_t errors, recoveries, nacks; } i2c_t;
void i2c_init(i2c_t *d, I2C_TypeDef *reg, uint32_t scl_hz);       /* I2C1: PB6=SCL PB9=SDA (AF4, open-drain) */
int  i2c_wr_rd(void *ctx, uint8_t addr7, const uint8_t *tx, size_t txlen, uint8_t *rx, size_t rxlen);
void i2c_bus_recover(i2c_t *d);
#endif
