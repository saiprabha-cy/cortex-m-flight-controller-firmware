# 07 FMEA (software failure modes)

| ID | Failure mode | Effect | Detection | Mitigation / reaction | Test |
|---|---|---|---|---|---|
| FM-01 | Corrupted or malicious firmware image | wrong code executed | CRC32, SHA-256, Ed25519 | image never booted, slot marked BAD, previous image runs | UT image_verify, HIL corrupt tests |
| FM-02 | New image crashes / hangs on boot | loss of function | no confirmation after 3 boots | rollback to previous slot | HIL rollback |
| FM-03 | Power loss during update | half-written slot | slot is EMPTY until FINISH verified | old image untouched | UT powerfail sweeps |
| FM-04 | Power loss during metadata write | lost boot state | CRC per record, ping-pong banks | newest valid record wins | UT `test_powerfail_*` |
| FM-05 | Task stalls (deadlock, endless loop) | stale outputs | per-task check-in deadlines | DEGRADED, then IWDG reset. Note: LSI tolerance is wide (~+-30 %) so IWDG timeout is nominal | HIL HANG_* |
| FM-06 | CPU lock-up with IRQs off | none visible | IWDG only | hardware reset | HIL IRQ_LOCKUP |
| FM-07 | Fault log wrap erases the bank | oldest records lost; erase stalls CPU ~100 ms | n/a | single-bank by design (diagnostic data only) | UT `test_single_bank_wrap_keeps_going` |
| FM-08 | HardFault / bus / usage fault | undefined behaviour | fault handlers | registers logged, reset | HIL NULL_CALL etc. |
| FM-09 | Stack overflow | memory corruption | FreeRTOS check method 2 + monitored HWM | logged, reset | HIL STACK_OVERFLOW |
| FM-10 | Heap exhaustion | task creation / queue failure | malloc-failed hook | logged, reset | HIL MALLOC_FAIL |
| FM-11 | IMU stuck / NaN / spike / dropout | bad attitude | plausibility, stuck-at (50 identical samples), age check | DEGRADED, authority halved | HIL stream faults |
| FM-12 | CAN node silent | missing power telemetry | 1 s heartbeat timeout | DEGRADED + log | two-node demo |
| FM-13 | CAN bus-off / error passive | lost messages | ESR counters, ISR | hardware auto bus-off recovery, counters in stats | stats only (not injected) |
| FM-14 | Reset loop | repeated crash | trailing-abnormal counter in fault log | boots into SAFE after 3 | HIL repeated crashes |
| FM-15 | Link noise / truncated frame | lost command | CRC16, length check, 100 ms inter-byte timeout | resync, host retries same seq | UT frame, HIL garbage |
| FM-16 | Brown-out | undefined operation | PVD interrupt (2.9 V) | logged, SAFE | not testable in emulator |
| FM-17 | Clock source failure | wrong baud/timing | bounded waits in `bsp_clock_init` | fallback to 16 MHz HSI, flag in telemetry | INSP |
