# Latency & resource report

System clock 168 MHz. Cycle source: EMULATED: derived from a 1 us timer (no DWT unit in this emulator) - resolution 1 us.

| Metric | Value |
|---|---|
| ISR entry latency min / avg / max (cycles) | 0 / 10 / 168  (16 samples) |
| ISR entry latency max (us) | 1.00 |
| Sensor task period min / max (us), nominal 10000 | 9787 / 10002 |
| Sensor period jitter, worst case (us) | 213 |
| Control execution avg / max (cycles) | 1012 / 1176 |
| Control execution max (us) | 7.00 |
| Sample -> actuator latency, worst (us) | 22 |
| CPU load (%) | 1 |
| Heap free / min-ever free (bytes) | 34016 / 34016 |
| ISR/main stack free (bytes) | 74120 |
| Stack headroom sensor task (words) | 411 |
| Stack headroom control task (words) | 411 |
| Stack headroom comms task (words) | 340 |
| Stack headroom telem task (words) | 398 |
| Stack headroom can task (words) | 463 |
| Stack headroom hk task (words) | 394 |
| Stack headroom timer task (words) | 225 |
| Stack headroom idle task (words) | 101 |
