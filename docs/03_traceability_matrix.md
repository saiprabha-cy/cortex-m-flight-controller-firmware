# 03 Traceability matrix (requirement -> design -> verification -> result)

Design references are sections of `02_firmware_design_document.md`. Results are from `05_test_report.md`.

| Requirement | Design | Verification | Result |
|---|---|---|---|
| FW-BOOT-001 | 3 | UT image_verify; INT packager interop | Pass |
| FW-BOOT-002 | 3 | UT, INT, HIL wrong-slot | Pass |
| FW-BOOT-003 | 2, 3 | UT rollback protection; INT | Pass |
| FW-BOOT-004 | 2 | UT boot_meta; HIL good update | Pass |
| FW-BOOT-005 | 2 | UT boot_meta; HIL crashing update | Pass |
| FW-BOOT-006 | 3 | UT; HIL corrupt payload/signature | Pass |
| FW-BOOT-007 | 4 | UT flashlog power-fail sweeps (model) | Pass (NOR model, not real flash) |
| FW-BOOT-008 | 2 | HIL update tests | Pass |
| FW-RTOS-001 | 5 | inspection; HIL telemetry | Pass |
| FW-RTOS-002 | 5, 8 | HIL latency report | Measured (emulated) |
| FW-RTOS-003 | 5 | HIL stack/heap headroom | Pass |
| FW-DRV-001 | - | inspection of `drivers/` | Pass |
| FW-DRV-002 | - | UT sensors | Pass |
| FW-COM-001 | 7 | UT frame; HIL garbage | Pass |
| FW-COM-002 | 7 | HIL unknown command | Pass |
| FW-COM-003 | 5 | HIL two-node CAN | Pass |
| FW-FLT-001 | 6 | HIL NULL_CALL (UsageFault captured with PC/CFSR); DIV_ZERO/UNALIGNED xfail in emulator | Partial: not verified for HardFault/DIV0/unaligned traps on silicon |
| FW-FLT-002 | 6 | UT wdg_policy; HIL HANG_*, IRQ_LOCKUP | Pass |
| FW-FLT-003 | 6 | HIL STACK_OVERFLOW (any detection), MALLOC_FAIL, ASSERT | Pass (see note) |
| FW-FLT-004 | 6 | UT policy; HIL repeated crashes | Pass |
| FW-FLT-005 | 6 | UT fsm; HIL safe mode | Pass |
| FW-FLT-006 | 5, 6 | HIL stuck sensor; HIL stream faults | Pass |
| FW-CTL-001 | 5 | UT kalman/pipeline; HIL accuracy | Pass |
| FW-QA-001 | - | build with -Werror; static report | Build pass; MISRA not compliant |
| FW-QA-002 | - | inspection of `FC_ENABLE_FAULT_INJECTION` | Pass |
