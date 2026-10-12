/* imu_mpu6500.h - SPI IMU driver (InvenSense MPU-6500 compatible register map).
 * Register addresses from the MPU-6500 register map: WHO_AM_I 0x75 (=0x70), PWR_MGMT_1 0x6B,
 * GYRO_CONFIG 0x1B, ACCEL_CONFIG 0x1C, ACCEL_XOUT_H 0x3B (14-byte burst: accel, temp, gyro). */
#ifndef IMU_MPU6500_H
#define IMU_MPU6500_H
#include "bus_if.h"
#include <stdbool.h>
typedef struct { float ax, ay, az; float gx, gy, gz; float temp_c; } imu_sample_t;   /* g, rad/s, degC */
typedef struct { const bus_t *bus; float accel_lsb_per_g, gyro_lsb_per_dps; } imu_t;
int  imu_init(imu_t *d, const bus_t *bus);                 /* 0 = ok, <0 = bus error, >0 = wrong WHO_AM_I */
int  imu_read(imu_t *d, imu_sample_t *s);
bool imu_sample_plausible(const imu_sample_t *s);          /* range + NaN check used by fault detection */
#endif
