/* flash.h - STM32F407 internal flash driver (RM0090 section 3): sector erase + 32-bit programming. */
#ifndef FLASH_H
#define FLASH_H
#include "../common/flash_if.h"
extern const flash_ops_t g_flash_hw_ops;
int flash_hw_sector_of(uint32_t addr, uint32_t *base, uint32_t *size);  /* returns sector number or -1 */
#endif
