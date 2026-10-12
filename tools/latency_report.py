#!/usr/bin/env python3
"""latency_report.py - queries a running board and prints/writes a markdown latency + resource report.
    python3 tools/latency_report.py --port tcp://localhost:4001 [--out build/latency_report.md] [--seconds 20]"""
import argparse, pathlib, sys, time
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from fc_link import FcLink


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--port", default="tcp://localhost:4001"); ap.add_argument("--out", default="build/latency_report.md")
    ap.add_argument("--seconds", type=float, default=20)
    a = ap.parse_args()
    with FcLink(a.port) as fc:
        fc.ensure_application(); fc.pump(a.seconds)
        lat, st = fc.latency(), fc.stats(); mhz = lat["sysclk_hz"] / 1e6
        src = "real DWT cycle counter" if lat["dwt_is_hardware"] else "EMULATED: derived from a 1 us timer (no DWT unit in this emulator) - resolution 1 us"
        rows = [("ISR entry latency min / avg / max (cycles)", f'{lat["isr_lat_min_cyc"]} / {lat["isr_lat_avg_cyc"]} / {lat["isr_lat_max_cyc"]}  ({lat["isr_lat_samples"]} samples)'),
                ("ISR entry latency max (us)", f'{lat["isr_lat_max_cyc"] / mhz:.2f}'),
                ("Sensor task period min / max (us), nominal 10000", f'{lat["sensor_period_min_us"]} / {lat["sensor_period_max_us"]}'),
                ("Sensor period jitter, worst case (us)", lat["sensor_jitter_max_us"]),
                ("Control execution avg / max (cycles)", f'{lat["ctrl_exec_avg_cyc"]} / {lat["ctrl_exec_max_cyc"]}'),
                ("Control execution max (us)", f'{lat["ctrl_exec_max_cyc"] / mhz:.2f}'),
                ("Sample -> actuator latency, worst (us)", lat["ctrl_latency_max_us"]),
                ("CPU load (%)", st["cpu_load_pct"]), ("Heap free / min-ever free (bytes)", f'{st["heap_free"]} / {st["heap_min_free"]}'),
                ("ISR/main stack free (bytes)", st["isr_stack_free_bytes"])]
        rows += [(f"Stack headroom {k[16:]} task (words)", v) for k, v in st.items() if k.startswith("stack_hwm_words_")]
    md = f"# Latency & resource report\n\nSystem clock {mhz:.0f} MHz. Cycle source: {src}.\n\n| Metric | Value |\n|---|---|\n" + "\n".join(f"| {k} | {v} |" for k, v in rows) + "\n"
    print(md); pathlib.Path(a.out).parent.mkdir(exist_ok=True, parents=True); pathlib.Path(a.out).write_text(md)


if __name__ == "__main__":
    main()
