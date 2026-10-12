#include "crc.h"

/* Nibble-table CRC32: 64 bytes of table instead of 1 KB - keeps the bootloader small. */
static const uint32_t k_crc32_nibble[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

uint32_t crc32_init(void) { return 0xFFFFFFFFu; }

uint32_t crc32_update(uint32_t state, const uint8_t *data, size_t len)
{
    for (size_t i = 0u; i < len; i++) {
        state ^= data[i];
        state = (state >> 4) ^ k_crc32_nibble[state & 0x0Fu];
        state = (state >> 4) ^ k_crc32_nibble[state & 0x0Fu];
    }
    return state;
}

uint32_t crc32_final(uint32_t state) { return state ^ 0xFFFFFFFFu; }

uint32_t crc32_calc(const uint8_t *data, size_t len)
{
    return crc32_final(crc32_update(crc32_init(), data, len));
}

uint16_t crc16_ccitt_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)((uint16_t)byte << 8);
    for (uint8_t b = 0u; b < 8u; b++) {
        crc = ((crc & 0x8000u) != 0u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0u; i < len; i++) { crc = crc16_ccitt_update(crc, data[i]); }
    return crc;
}
