/* spi.h - SPI1 master driver (RM0090 section 28), polled with timeouts, software chip-select. */
#ifndef SPI_H
#define SPI_H
#include "gpio.h"
#include "bus_if.h"
typedef struct { SPI_TypeDef *reg; GPIO_TypeDef *cs_port; uint8_t cs_pin; uint32_t timeout_us; uint32_t errors; } spi_t;
/* prescaler_log2: 1 -> /2 ... 8 -> /256. mode 0..3 (CPOL/CPHA). PA5=SCK PA6=MISO PA7=MOSI, CS=PE3 by default board wiring. */
void spi_init(spi_t *s, SPI_TypeDef *reg, uint8_t mode, uint8_t prescaler_log2, GPIO_TypeDef *cs_port, uint8_t cs_pin);
int  spi_xfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);       /* matches bus_t.spi_xfer */
#endif
