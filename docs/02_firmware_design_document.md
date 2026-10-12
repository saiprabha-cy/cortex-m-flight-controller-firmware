# 02 Firmware Design Document

## 1. Architecture
```
 host tools (Python)  <--USART2 framed link / RS-485-->  application (FreeRTOS)  <--CAN-->  power node
                                                          |  tasks: sensor 100Hz | control | comms | telemetry | can | supervisor
 bootloader  --verifies & jumps--> slot A | slot B        |  common/: crc sha256 frame flashlog image_verify boot_meta mode_fsm wdg_mgr
 BSP: startup, clocks, timebase, fault capture, reset cause|  drivers/: register-level peripherals
```

## 2. Boot sequence
1. Reset -> bootloader: 168 MHz clock (bounded waits, HSI fallback), reset cause read and **latched in RAM** (RCC flags are cleared once read).
2. Load boot metadata (`boot_meta_t`) from the two-bank flash log; honour a pending update request (RAM word from the app).
3. `boot_decide()` (pure logic, unit-tested): candidate order = active slot, then the other; skip BAD slots; a TRIAL slot with >= 3 attempts becomes BAD; otherwise `image_verify()`; failure marks the slot BAD; success increments the attempt counter for TRIAL images.
4. If a good slot exists: 400 ms window for a host HELLO, then set VTOR/MSP and jump. If none: stay in the updater (recovery).
5. The application runs; after 10 s of healthy NOMINAL operation it confirms itself (`boot_meta_mark_confirmed`), raising the anti-rollback floor.

## 3. Image format & trust chain
512-byte header (`common/image_header.h`): magic, slot, size, load address, version, security counter, payload CRC32, payload SHA-256, header CRC32, Ed25519 signature over bytes [0,448).
Check order is cheapest-first: magic, header CRC, slot/size/address, payload CRC32, SHA-256, signature, anti-rollback, vector-table sanity.
CRC32 catches accidents; SHA-256 + Ed25519 provide authenticity (CRC is not a MAC - `test_crc_forged_but_sha_wrong` demonstrates this).

## 4. Flash log (`common/flashlog.c`)
32-byte records `[seq][24 B payload][CRC32]`, append-only, erased = 0xFF. Two banks ping-pong for boot metadata: the newest record is written to the other (erased) bank *before* the old bank is erased, so a power cut at any point leaves a valid newest record (swept at every flash event in `test_flashlog.c`). The fault log uses one bank.

## 5. Task model
| Task | Prio | Period / trigger | Notes |
|---|---|---|---|
| control | 6 | notified by sensor task | Kalman + PD, writes TIM4 PWM; authority halved in DEGRADED, outputs off in SAFE |
| sensor | 5 | 10 ms `vTaskDelayUntil` | IMU every cycle, baro+ADC at 10 Hz, plausibility + stuck-at detector |
| comms | 4 | USART RX ISR notification | parses frames, dispatches commands |
| can | 4 | 10 ms | RX queue, heartbeat 10 Hz, node-lost detection |
| telemetry | 3 | 100 ms | 70-byte frame on USART2, every 5th on RS-485 |
| supervisor (hk) | 2 | 100 ms | watchdog supervision + IWDG kick policy, mode FSM, stats, image confirmation |

Shared state `g_fc` is protected by one mutex held only for copies. ISRs (priority >= 6 numerically, below `configMAX_SYSCALL_INTERRUPT_PRIORITY`) only move bytes and give notifications.

## 6. Fault management
* **Software watchdog** (`common/wdg_mgr.c`): each task checks in every loop; late by > deadline -> soft miss (DEGRADED, logged); late by > deadline + 300 ms -> supervisor stops kicking the **IWDG (1 s)** -> reset.
* **Mode FSM** (`common/mode_fsm.c`): table-driven, exhaustively tested; SAFE is exited only by a ground command.
* **Fault capture** (`bsp/fault.c`): naked trampolines select MSP/PSP, log PC, LR, CFSR, MMFAR/BFAR to flash, print on the debug UART, reset.
* **Reset policy** (`common/fault_policy.c`): >= 3 consecutive abnormal resets since the last healthy marker/power-on/rollback -> boot into SAFE.

## 7. Link protocol (`common/frame.h`)
`A5 5A | len | type | seq | payload | CRC16-CCITT (BE)`. Host retries with the same sequence number; commands are idempotent (bootloader WRITE de-duplicates by offset).

## 8. Real-time measurements
`GET_LATENCY` reports ISR entry latency (pend a software IRQ, count cycles), sensor period min/max/jitter, control execution time and sample-to-actuator latency. In Renode there is no DWT: cycles derive from a 1 us timer (reported flag `dwt_is_hardware = 0`).
