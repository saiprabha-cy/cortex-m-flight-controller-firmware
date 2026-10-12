# 04 Test procedures

All procedures are automated. Pre-conditions: `./scripts/setup_ubuntu.sh`, `export RENODE=...`, `./scripts/build.sh`.

| ID | Procedure | Command | Pass criteria |
|---|---|---|---|
| TP-01 | Host unit tests (ASan+UBSan) | `./scripts/run_unit_tests.sh` | 10/10 suites pass; no sanitizer report |
| TP-02 | Packager <-> bootloader verifier interop | `pytest tests/integration/test_packager_interop.py` | all pass |
| TP-03 | Static analysis | `./scripts/run_static.sh` | cppcheck exit 0; MISRA/clang-tidy findings reviewed in docs/06 |
| TP-04 | Boot, telemetry, link robustness, modes | `pytest tests/integration/test_hil_basic.py -k "not hil_"` | all pass |
| TP-05 | HIL attitude accuracy | `pytest tests/integration/test_hil_basic.py -k estimation` | roll/pitch RMS < 2 deg |
| TP-06 | HIL sensor-stream faults (dropout, NaN, spike, stuck) | `pytest ... -k sensor_stream_faults` | DEGRADED during fault, NOMINAL after |
| TP-07 | Fault-injection campaign | `pytest tests/integration/test_fault_injection.py` | each fault logged to flash and system recovers; DIV_ZERO/UNALIGNED are xfail in Renode (not modelled) |
| TP-08 | Secure update scenarios | `pytest tests/integration/test_update_rollback.py` | good image installed + confirmed; corrupt images rejected; crashing image rolled back |
| TP-09 | CAN two-node demo | `renode renode/two_nodes.resc` (manual) | node telemetry visible in OBC telemetry; node-lost detected when the node machine is paused |
| TP-10 | Latency report | `python3 tools/latency_report.py` | report written to docs/05 (emulated numbers) |
