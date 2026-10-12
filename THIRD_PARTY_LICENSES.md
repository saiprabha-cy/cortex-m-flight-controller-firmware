# Third-party code (vendored, unmodified unless stated)

| Component | Version | Licence | Path |
|---|---|---|---|
| FreeRTOS-Kernel | V11.1.0 (tasks, queue, list, timers, event groups, stream buffers, ARM_CM4F port, heap_4) | MIT | `third_party/freertos/` |
| CMSIS Core (Arm CMSIS_5) | 5.x headers | Apache-2.0 | `third_party/cmsis_core/` |
| CMSIS Device STM32F4 headers (register definitions only) | ST cmsis_device_f4 | Apache-2.0 | `third_party/cmsis_device_f4/` |
| Monocypher (Ed25519) | current master at vendoring time | BSD-2-Clause / CC0 | `third_party/monocypher/` |
| Unity test framework | current master at vendoring time | MIT | `third_party/unity/` |
| Renode platform description | derived from Renode `stm32f4.repl` (SVD line removed, clocks corrected) | MIT | `renode/stm32f407.repl` |

All driver, BSP, bootloader and application code in this repository is original.
Licence texts are in each directory. The project's own licence is not set: add one before publishing.
