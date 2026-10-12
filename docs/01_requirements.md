# 01 Requirements (v1.0)

Verification methods: **UT** host unit test · **HIL** Renode integration test · **INT** interop test · **INSP** inspection/analysis · **DEMO** scripted demo.

| ID | Requirement | Verified by |
|---|---|---|
| FW-BOOT-001 | The bootloader shall verify CRC32, SHA-256 and an Ed25519 signature of an image before executing it. | UT `test_image_verify`, INT `test_packager_interop` |
| FW-BOOT-002 | An image built for one slot shall not be accepted in the other (slot id + load address check). | UT `test_wrong_slot_image`, INT `test_wrong_slot`, HIL `test_wrong_slot_image_refused_by_uploader` |
| FW-BOOT-003 | The bootloader shall refuse images whose anti-rollback counter is below the stored floor. | UT `test_rollback_protection`, INT `test_rollback_blocked` |
| FW-BOOT-004 | A newly installed image shall run in TRIAL state and become CONFIRMED only when the application confirms it. | UT `test_new_image_trial_then_confirm`, HIL `test_good_update_is_installed_and_confirmed...` |
| FW-BOOT-005 | An image that is not confirmed after 3 boots shall be marked BAD and the previous image restored. | UT `test_unconfirmed_image_rolls_back_after_max_attempts`, HIL `test_crashing_update_rolls_back...` |
| FW-BOOT-006 | A corrupt image shall never be executed; the previous image keeps running. | UT (all corruption cases), HIL `test_corrupt_image_is_rejected...` |
| FW-BOOT-007 | Boot metadata and fault records shall survive power loss at any point of a write. | UT `test_powerfail_*` (cut at every flash event) |
| FW-BOOT-008 | The bootloader shall accept firmware over the framed UART link into the inactive slot only. | HIL upload tests, INSP `pick_target()` |
| FW-RTOS-001 | Sensor, control, comms, telemetry, CAN and supervisor functions shall run as separate RTOS tasks with fixed priorities. | INSP `app/src/main.c`, HIL `test_telemetry_rate_and_content` |
| FW-RTOS-002 | Sensor acquisition shall run at 100 Hz with measured period jitter reported. | HIL latency report (docs/05) |
| FW-RTOS-003 | Stack high-water marks and heap headroom shall be monitored and reported. | HIL `test_stack_and_heap_headroom` |
| FW-DRV-001 | UART, SPI, I²C, CAN, ADC, PWM, GPIO, DMA, flash and IWDG drivers shall be register-level (no vendor HAL). | INSP `drivers/` |
| FW-DRV-002 | Sensor drivers shall be testable against software device models. | UT `test_sensors` (MPU-6500 model, BMP280 datasheet example) |
| FW-COM-001 | The link protocol shall reject any frame with a single-bit error and resynchronise after noise. | UT `test_frame`, HIL `test_garbage_and_corrupt_frames_do_not_break_link` |
| FW-COM-002 | Unknown commands shall be answered with a NAK. | HIL `test_unknown_command_is_nakd` |
| FW-COM-003 | The CAN node (OBC) shall detect loss of the power node within 1 s. | HIL two-node demo (renode/two_nodes.resc) |
| FW-FLT-001 | Every HardFault/MemManage/BusFault/UsageFault shall be captured (PC, LR, CFSR, fault address), logged to flash, and trigger a reset. | HIL `test_fault_is_logged_and_system_recovers[NULL_CALL,DIV_ZERO,UNALIGNED]` |
| FW-FLT-002 | A stalled task shall be detected by the software watchdog and the hardware watchdog shall reset the MCU. | HIL `[HANG_SENSOR,HANG_CONTROL,HANG_SUPERVISOR,IRQ_LOCKUP]`, UT `test_wdg_policy` |
| FW-FLT-003 | Stack overflow, heap exhaustion and failed assertions shall be logged and cause a safe reset. | HIL `[STACK_OVERFLOW,MALLOC_FAIL,ASSERT]` |
| FW-FLT-004 | Three consecutive abnormal resets shall make the next boot start in SAFE mode. | UT `test_policy_counts_trailing_abnormal_resets`, HIL `test_repeated_crashes_force_safe_mode...` |
| FW-FLT-005 | In SAFE mode actuator outputs shall be neutral/disabled; only a ground command shall leave SAFE. | UT `test_safe_is_sticky...`, HIL `test_safe_mode_command_and_recovery` |
| FW-FLT-006 | Implausible, stuck or missing IMU data shall cause DEGRADED mode with reduced control authority, and automatic recovery after 5 s of clean data. | HIL `test_stuck_sensor_degrades_then_recovers`, `test_hil_sensor_stream_faults_are_detected` |
| FW-CTL-001 | Attitude shall be estimated by a Kalman filter that also estimates gyro bias. | UT `test_kalman`, `test_pipeline`; HIL `test_hil_estimation_accuracy` |
| FW-QA-001 | First-party code shall build with `-Wall -Wextra -Wconversion -Werror` and pass cppcheck/clang-tidy in CI. | CI |
| FW-QA-002 | Debug fault injection shall be removable by a build switch (`FC_ENABLE_FAULT_INJECTION=OFF`). | INSP `app/inc/app_config.h` |
