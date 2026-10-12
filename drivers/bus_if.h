/* bus_if.h - tiny bus abstraction between sensor drivers and the register-level SPI/I2C drivers.
 * Lets device drivers be unit-tested on the host against a software model of the sensor. */
#ifndef BUS_IF_H
#define BUS_IF_H
#include <stdint.h>
#include <stddef.h>
#define BUS_OK    0
#define BUS_ERR  (-1)
typedef struct {
    void *ctx;
    /* SPI: full-duplex transfer of len bytes with CS framing handled by the implementation.
     * I2C: write-then-read: tx (txlen bytes) then rx (rxlen bytes) with repeated start. */
    int (*spi_xfer)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);
    int (*i2c_wr_rd)(void *ctx, uint8_t addr7, const uint8_t *tx, size_t txlen, uint8_t *rx, size_t rxlen);
} bus_t;
#endif
