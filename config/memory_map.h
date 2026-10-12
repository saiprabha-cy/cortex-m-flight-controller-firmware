/* memory_map.h - single source of truth for the flash layout (see docs/memory_map.md).
 * STM32F407: sectors 0-3 = 16 KB, sector 4 = 64 KB, sectors 5-11 = 128 KB. */
#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#define FLASH_BASE_ADDR        0x08000000u
#define FLASH_END_ADDR         0x08100000u

#define BOOT_BASE              0x08000000u   /* sectors 0-1 */
#define BOOT_SIZE              0x00008000u   /* 32 KB */

#define FAULTLOG_BASE          0x08008000u   /* sector 2 (16 KB) */
#define FAULTLOG_SIZE          0x00004000u

#define META_BANK0_BASE        0x0800C000u   /* sector 3 (16 KB) */
#define META_BANK1_BASE        0x08010000u   /* sector 4 (64 KB; only first 16 KB used) */
#define META_BANK_SIZE         0x00004000u

#define SLOT_A                 0u            /* slot ids */
#define SLOT_B                 1u
#define SLOT_A_BASE            0x08020000u   /* sectors 5-7 (384 KB) */
#define SLOT_B_BASE            0x08080000u   /* sectors 8-10 (384 KB) */
#define SLOT_SIZE              0x00060000u

#define IMG_HEADER_SIZE        512u          /* keeps the vector table 512-byte aligned */
#define SLOT_APP_OFFSET        IMG_HEADER_SIZE
#define SLOT_A_APP_ADDR        (SLOT_A_BASE + SLOT_APP_OFFSET)
#define SLOT_B_APP_ADDR        (SLOT_B_BASE + SLOT_APP_OFFSET)
#define SLOT_APP_MAX_SIZE      (SLOT_SIZE - SLOT_APP_OFFSET)

#define SRAM_BASE_ADDR         0x20000000u
#define SRAM_SIZE              0x00020000u   /* 128 KB */

/* Last 32 bytes of SRAM are excluded from every linker script and survive soft/watchdog resets:
 *   [SHARED_RAM_ADDR +  0]  shared_ram_t  app -> bootloader request word (e.g. "enter update mode")
 *   [SHARED_RAM_ADDR + 16]  boot_info_t   reset-cause latch + clean-shutdown marker */
#define SHARED_RAM_ADDR        (SRAM_BASE_ADDR + SRAM_SIZE - 32u)
#define BOOT_INFO_ADDR         (SHARED_RAM_ADDR + 16u)

#endif
