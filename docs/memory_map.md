# Memory map (STM32F407VG, 1 MB flash, 128 KB SRAM)

| Region | Address | Size | Sectors | Purpose |
|---|---|---|---|---|
| Bootloader | `0x0800_0000` | 32 KB | 0-1 | A/B selector, verifier, UART updater (19.8 KB used) |
| Fault log | `0x0800_8000` | 16 KB | 2 | 512 x 32-byte records, single bank (wrap = erase) |
| Boot metadata bank 0 | `0x0800_C000` | 16 KB | 3 | ping-pong log of `boot_meta_t` |
| Boot metadata bank 1 | `0x0801_0000` | 16 KB used of 64 KB | 4 | second ping-pong bank |
| Slot A | `0x0802_0000` | 384 KB | 5-7 | 512-byte image header + application |
| Slot B | `0x0808_0000` | 384 KB | 8-10 | same |
| Reserved | `0x080E_0000` | 128 KB | 11 | |
| SRAM | `0x2000_0000` | 128 KB - 32 B | | .data/.bss/heap; MSP stack at the top |
| Shared RAM | `0x2001_FFE0` | 32 B | | `shared_ram_t` (update request) + `boot_info_t` (reset-cause latch, clean-reset marker) |

* The vector table starts at slot base + 512 (512-byte aligned, as required by VTOR for 98 vectors).
* The slot linker scripts (`bsp/app_slot_a.ld`, `app_slot_b.ld`) differ only in the flash origin.
* Image header layout: `common/image_header.h` (mirrored by `tools/fw_packager.py`).
* STM32F4 has no read-while-write: while a sector is being erased the CPU stalls. Erases therefore happen only in the bootloader, during fault-log clear/wrap and during the one-time image confirmation.
