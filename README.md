# Cortex-M Fault-Tolerant Flight Controller Firmware: RTOS, A/B Secure Bootloader, CAN/UART/SPI/I²C Drivers, Renode HIL, MISRA-checked CI

Bare-metal + FreeRTOS firmware for an ARM Cortex-M4F flight controller (STM32F407), written from scratch at register level (no vendor HAL),
with a signed A/B bootloader, fault management, and a hardware-in-the-loop test bench that runs **without physical hardware** in the
[Renode](https://renode.io) emulator.

> **Honesty note (read this first).** Everything runs on an *emulated* STM32F407. Code is portable to a real board, but timing numbers
> produced in the emulator (interrupt latency, jitter, CPU load) describe the emulated core, not silicon, and are labelled as such.
> The STM32F4 has classic CAN (bxCAN) only - there is no CAN-FD on this part. USB and Ethernet are not implemented.

## What is in here

| Area | What it does | Where |
|---|---|---|
| Bootloader | Verifies **CRC32 + SHA-256 + Ed25519** of an image, A/B slots, trial boot, **rollback after 3 unconfirmed boots**, anti-rollback counter, framed-UART updater | `bootloader/`, `common/` |
| BSP | Own startup + generated 98-entry vector table, linker scripts, 168 MHz PLL with timeouts + HSI fallback, 1 MHz timebase, DWT, reset-cause latch | `bsp/` |
| Drivers (register level) | GPIO, UART (IRQ + DMA TX + RS-485 DE), SPI, I²C (bus recovery), bxCAN, ADC+DMA, PWM, flash, IWDG, PVD | `drivers/` |
| Sensor drivers | MPU-6500 IMU (SPI), BMP280 barometer (I²C) over a bus abstraction, unit-tested against software device models | `drivers/imu_mpu6500.c`, `baro_bmp280.c` |
| RTOS app | 6 FreeRTOS tasks (sensor 100 Hz, control, comms, telemetry, CAN, supervisor), queues, mutex, task notifications, ISR-to-task wakeups, software timers | `app/` |
| Fault management | Per-task software watchdog -> hardware IWDG, HardFault capture to flash log, stack-overflow/malloc/assert hooks, mode FSM (BOOT/NOMINAL/DEGRADED/SAFE), reset-loop -> SAFE | `common/`, `bsp/fault.c`, `app/src/housekeeping.c` |
| Control | 2-state Kalman filter (angle + gyro bias) per axis + PD controller, float, deterministic 100 Hz | `app/src/kalman.c` |
| CAN network | OBC + a second firmware image (`power_node`) as another emulated machine | `node/`, `renode/two_nodes.resc` |
| Host tools (Python) | Link client, firmware uploader, HIL vehicle simulator, packager/signer, Renode controller | `tools/` |
| Tests | Host unit tests (ASan/UBSan), packager<->C interop, Renode HIL, fault-injection campaign, update/rollback scenarios | `tests/` |
| Process docs | Requirements, design, traceability, test procedures/report, MISRA report, FMEA | `docs/` |

## Quick start (Ubuntu 22.04/24.04 or WSL2)

```bash
./scripts/setup_ubuntu.sh        # toolchain, cmake/ninja, cppcheck, python packages, Renode (portable) in ~/renode
export PATH=$HOME/renode:$PATH RENODE=$HOME/renode/renode
./scripts/build.sh               # builds bootloader, app images, power node; generates a DEV signing key on first run
./scripts/run_unit_tests.sh      # 10 host test suites, ASan+UBSan, no hardware needed
```

Windows: use WSL2 (Ubuntu). Everything else is identical.

## Run it in the emulator

Terminal 1 - the board (bootloader verifies slot A, jumps, FreeRTOS starts):
```bash
./scripts/demo.sh run                # Renode console; USART2 link on tcp://localhost:4001
tail -f build/uart1_debug.log        # the board's debug console (USART1)
```
Terminal 2 - talk to it:
```bash
python3 tools/fc_link.py telem       # one telemetry frame      (also: version | stats | latency | faults | ping)
python3 tools/hil_sim.py --seconds 40                          # play the vehicle, report attitude RMS error
python3 tools/hil_sim.py --seconds 40 --fault stuck            # corrupt the sensor stream: DEGRADED then recovery
python3 tools/fw_upload.py build/app_v2_b.signed.bin           # secure OTA update into slot B (~35 s in the emulator)
```
Note: if you connect while the bootloader's 400 ms host window is open it waits for you; `FcLink.ensure_application()` (used by all tools) sends BL_BOOT for you.

## The five demos that sell this project (each is an automated test too)

1. **Reject a tampered image** - `bad_payload_b` / `bad_signature_b` / `bad_header_b` are uploaded, the bootloader refuses them, the old firmware keeps running.
2. **Rollback** - `app_bad_b` (v1.2.0) crashes 3 s after boot and never confirms itself; after 3 boots the bootloader marks it BAD and returns to v1.0.0.
3. **Watchdog -> safe mode** - inject a stalled task; software watchdog flags it, IWDG resets, fault log shows `TASK_WDG`, `WDG_RESET`; three crashes in a row boot into SAFE.
4. **Sensor faults** - NaN / spike / stuck / dropout in the injected IMU stream -> DEGRADED (halved control authority) -> automatic recovery.
5. **Fault log from flash** - `python3 tools/fc_link.py faults` after any of the above prints PC/LR/CFSR of the crash that happened before the reset.

Run the whole campaign: `./scripts/run_hil.sh` (about 30 min on a 2-core machine; add `-m "not slow"` to skip the long update scenarios).

## Repository layout

```
bootloader/   main.c (updater + boot logic), inc/pubkey.h (generated)
app/          src/{main,tasks,comms,housekeeping,sensors,latency,irq,kalman,sim_sensors}.c, inc/
bsp/          startup.c (generated), bsp.c, fault.c, dbg.c, linker scripts (bootloader, slot A/B, node)
drivers/      gpio uart spi i2c can adc pwm flash iwdg + imu_mpu6500, baro_bmp280
common/       crc sha256 frame flashlog image_verify boot_meta mode_fsm wdg_mgr fault_policy  (host-testable, no hardware deps)
node/         CAN power node firmware
renode/       stm32f407.repl, flight_controller.resc, two_nodes.resc
tools/        fc_link.py fw_upload.py fw_packager.py hil_sim.py renode_ctl.py keygen.py gen_startup.py
tests/        unit/ (Unity, host)   integration/ (pytest + Renode)
docs/         01_requirements ... 07_fmea, memory_map, devlog
third_party/  FreeRTOS-Kernel V11.1.0, CMSIS core + STM32F4 device headers (register definitions only), Monocypher (Ed25519), Unity - all vendored with licences
```

## Configuration switches (CMake / `app/inc/app_config.h`)

| Option | Default | Meaning |
|---|---|---|
| `-DFC_FW_VERSION=1.0.0` | 1.0.0 | version baked into app_a/app_b |
| `-DFC_ENABLE_FAULT_INJECTION=OFF` | ON | removes the debug fault-injection command (do this for a "flight" build) |
| `FC_SENSOR_DEFAULT` | 0 (hardware) | 0 hardware drivers, 1 onboard simulator, 2 host HIL. In the emulator the hardware sensors are absent so it falls back to the simulator (`FC_ALLOW_SIM_FALLBACK`) |
| `FC_USE_DMA` | 1 | DMA for USART2 TX / ADC |
| `FC_REQUIRE_CAN_NODE` | 0 | treat a never-seen power node as a fault |

## Using a real STM32F407 Discovery later

1. Build as above. Flash with OpenOCD/ST-Link: `bootloader.bin` at `0x08000000`, `app_a.signed.bin` at `0x08020000`.
2. Wire an MPU-6500 to SPI1 (PA5/PA6/PA7, CS = PE3) and a BMP280 to I2C1 (PB6/PB9) and the firmware uses them instead of the simulator.
3. Debug console on USART1 (PA9), command link on USART2 (PA2/PA3), RS-485 on USART3 (PB10/11, DE = PB12), CAN1 on PD0/PD1.

## Verified results
See `docs/05_test_report.md`: 10/10 host suites, 7/7 interop, all HIL scenarios pass in Renode except two emulator-limited cases (DIV_ZERO / UNALIGNED traps, marked xfail). Attitude RMS error in HIL: 0.23 deg (roll), 0.18 deg (pitch).

## Known limitations (also listed in docs/05_test_report.md)

* Renode 1.17 has no ADC model (battery voltage reads 0), no DWT unit (cycle counts derive from a 1 us timer) and its RCC never sets watchdog/software-reset flags (a RAM "clean reset" marker is used to infer the reset cause: `FC_EMU_RESET_INFERENCE`).
* The emulator runs at roughly half real time on a laptop; tests pace themselves on the firmware clock.
* The development signing key is generated locally by `tools/keygen.py`; never reuse it for anything real.
* MISRA-C: checked with cppcheck's addon; the deviation list is honest and incomplete rule coverage is stated in `docs/06_misra_compliance_report.md`.
