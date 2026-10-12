/* crc.h - CRC32 (IEEE 802.3) and CRC16-CCITT-FALSE. Portable C, no hardware dependency. */
#ifndef CRC_H
#define CRC_H
#include <stdint.h>
#include <stddef.h>

/* CRC32: poly 0x04C11DB7 reflected (0xEDB88320), init 0xFFFFFFFF, xorout 0xFFFFFFFF.
 * Matches Python zlib.crc32(). Use crc32_update() to chain: start with crc32_init(). */
uint32_t crc32_init(void);
uint32_t crc32_update(uint32_t state, const uint8_t *data, size_t len);
uint32_t crc32_final(uint32_t state);
uint32_t crc32_calc(const uint8_t *data, size_t len);

/* CRC16-CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, xorout 0. Check("123456789") = 0x29B1 */
uint16_t crc16_ccitt(const uint8_t *data, size_t len);
uint16_t crc16_ccitt_update(uint16_t crc, uint8_t byte);
#endif
