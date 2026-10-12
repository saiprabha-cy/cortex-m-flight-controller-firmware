# 05 Test report (v1.0)

Environment: Ubuntu 24.04, arm-none-eabi-gcc 13.2.1, Renode 1.17.0 (portable), pytest 9, cppcheck 2.13, clang-tidy. **All hardware-in-the-loop results are from the Renode emulator, not silicon.**

## Summary

| Procedure | Result |
|---|---|
| TP-01 host unit tests (ASan+UBSan) | **10/10 suites pass** (crc_sha, frame, mode_fsm, flashlog, image_verify, boot_meta, kalman, sensors, wdg_policy, pipeline) |
| TP-02 packager <-> C verifier interop | **7/7 pass** |
| TP-03 static analysis | cppcheck: no error/warning after 3 documented suppressions; MISRA: **not compliant**, inventory in docs/06; clang-tidy: 8 warnings (linker symbols) |
| TP-04 boot/telemetry/link/modes | **7/7 pass** |
| TP-05 HIL attitude accuracy | **pass**: roll RMS 0.23 deg, pitch 0.18 deg (200 samples, firmware-clock paced) |
| TP-06 HIL sensor-stream faults | **4/4 pass** (dropout, NaN, spike, stuck -> DEGRADED, then NOMINAL) |
| TP-07 fault-injection campaign | **10 pass, 2 xfail**: NULL_CALL, STACK_OVERFLOW, MALLOC_FAIL, ASSERT, HANG_SENSOR, HANG_CONTROL, HANG_SUPERVISOR, IRQ_LOCKUP + repeated-crash->SAFE + clear log pass. DIV_ZERO and UNALIGNED are xfail because Renode 1.17 does not model `CCR.DIV_0_TRP` / `UNALIGN_TRP`; **not verified on silicon** |
| TP-08 secure update scenarios | **5/5 pass**: good update installed and confirmed after ~10 s healthy; corrupt payload rejected; corrupt signature rejected; wrong-slot image refused; crashing update rolled back to v1.0.0 with `USAGEFAULT` x2 + `BOOT_ROLLBACK` in the fault log |
| TP-09 CAN two-node | **pass**: power telemetry over CAN (7.4 V / 0.9 A), node silence -> NODE_LOST -> DEGRADED -> recovery |
| TP-10 latency report | see `docs/latency_report_emulated.md` |

## Notes and known behaviour
* STACK_OVERFLOW is accepted as detected if *any* fault (RTOS overflow hook, UsageFault, HardFault, BusFault) is logged and the MCU resets: deep recursion corrupts memory before the RTOS stack check (made at context switch) runs. An MPU guard region would close this (future work).
* NULL function call produces a UsageFault (INVSTATE, Thumb bit clear), not a HardFault - this is the real Cortex-M behaviour.
* When a host talks to the bootloader during its boot window, `BL_BOOT` runs the boot decision a second time, so a host-assisted boot consumes one extra trial attempt (rollback occurs one reset earlier).
* Renode limitations: no ADC model (vbat = 0), no DWT (cycles from a 1 us timer), RCC reset flags not set (RAM marker used), no CCR trap emulation.
* Timing numbers (docs/latency_report_emulated.md) are **emulated** and have 1 us resolution; they demonstrate the measurement method, not silicon performance.
* Not tested: brown-out (PVD), CAN bus-off, DMA error paths, real peripherals (SPI IMU / I2C barometer drivers are verified against software models only), power-fail of the real flash controller (verified against a NOR model).
* Run time of the complete HIL suite: roughly 30 minutes on a 2-core cloud VM.
