/* flash_if.h - flash abstraction. The same upper layers (flash log, boot metadata, image
 * installer) run on the real STM32 flash driver, and on a RAM-backed NOR model in host tests. */
#ifndef FLASH_IF_H
#define FLASH_IF_H
#include <stdint.h>

#define FLASH_OK        0
#define FLASH_ERR      (-1)

typedef struct {
    int (*read)(uint32_t addr, void *dst, uint32_t len);
    /* addr and len must be multiples of 4; programming 0->1 is illegal (NOR semantics) */
    int (*program)(uint32_t addr, const void *src, uint32_t len);
    /* erases the whole sector that contains addr */
    int (*erase_sector)(uint32_t addr);
} flash_ops_t;

#endif
