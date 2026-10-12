/* baro_bmp280.h - I2C barometer driver (Bosch BMP280). Uses the datasheet's integer
 * compensation formulas (32-bit temperature, 64-bit pressure) from the BMP280 datasheet sect. 3.11.3. */
#ifndef BARO_BMP280_H
#define BARO_BMP280_H
#include "bus_if.h"
typedef struct {
    const bus_t *bus; uint8_t addr;
    uint16_t T1; int16_t T2, T3; uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9; int32_t t_fine;
} baro_t;
int baro_init(baro_t *d, const bus_t *bus, uint8_t addr7);       /* 0 ok; <0 bus err; >0 wrong chip id */
int baro_read(baro_t *d, float *pressure_pa, float *temp_c);
float baro_altitude_m(float pressure_pa, float sea_level_pa);
#endif
