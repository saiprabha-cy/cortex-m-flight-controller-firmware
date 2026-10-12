#!/usr/bin/env python3
"""hil_sim.py - hardware-in-the-loop 'vehicle' simulator.

Plays the role of the airframe + sensors: it streams IMU/barometer samples to the firmware (MSG_HIL_SENSOR) and compares the
on-target Kalman estimate (from telemetry) with the simulated truth.

Time base: the emulator may run slower or faster than wall-clock, so the simulator paces itself on the FIRMWARE's own clock
(uptime reported in telemetry, interpolated with a measured speed ratio) - the truth is evaluated at exactly the virtual time
each telemetry frame was produced. Optional fault modes corrupt the stream to exercise the firmware's fault detection.

    python3 tools/hil_sim.py --port tcp://localhost:4001 --seconds 30 [--fault dropout|stuck|nan|spike]
"""
import argparse, math, pathlib, random, sys, time
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from fc_link import FcLink


class Truth:
    def __init__(self, bias=(0.04, -0.03)):
        self.bias = bias

    def at(self, t):
        roll, pitch = 0.3 * math.sin(0.5 * t), 0.2 * math.sin(0.35 * t + 0.7)
        rr, pr = 0.3 * 0.5 * math.cos(0.5 * t), 0.2 * 0.35 * math.cos(0.35 * t + 0.7)
        return roll, pitch, rr, pr


class HilRunner:
    def __init__(self, fc: FcLink, seed=3, noise=(0.02, 0.005, 5.0)):
        self.fc, self.rnd, self.noise, self.truth = fc, random.Random(seed), noise, Truth()
        self.speed = 0.5; self._anchor = None; self.samples_sent = 0; self.errors = []; self._last_n = -1

    def _vnow(self):
        t = self.fc.last_telem
        if t is None: return 0.0
        if self._anchor is None or self._anchor[0] != t.uptime_ms:
            now = time.time()
            if self._anchor is not None and now > self._anchor[1] + 0.2:
                sp = ((t.uptime_ms - self._anchor[0]) / 1000.0) / (now - self._anchor[1])
                self.speed = 0.8 * self.speed + 0.2 * min(2.0, max(0.05, sp))
            self._anchor = (t.uptime_ms, time.time())
        return self._anchor[0] / 1000.0 + (time.time() - self._anchor[1]) * self.speed

    def sample(self, t, fault=None):
        roll, pitch, rr, pr = self.truth.at(t); a, g, p = self.noise; n = self.rnd.uniform
        ax, ay, az = -math.sin(pitch) + n(-a, a), math.sin(roll) * math.cos(pitch) + n(-a, a), math.cos(roll) * math.cos(pitch) + n(-a, a)
        gx, gy, gz = rr + self.truth.bias[0] + n(-g, g), pr + self.truth.bias[1] + n(-g, g), n(-g, g)
        if fault == "nan": ax = float("nan")
        elif fault == "spike": ax = 40.0
        return (ax, ay, az, gx, gy, gz, 101325.0 + n(-p, p), 24.0)

    def run(self, seconds_virtual, fault_window=None, fault=None, settle=8.0, on_tick=None):
        """Stream until `seconds_virtual` of firmware time have passed. fault_window=(t0,t1) in virtual seconds."""
        self.fc.set_sensor_source(2)
        self.fc.wait_telem(2); start = self._vnow(); frozen = None
        while True:
            self.fc.pump(0.0); v = self._vnow(); rel = v - start
            if rel > seconds_virtual: break
            in_fault = fault_window is not None and fault_window[0] <= rel < fault_window[1]
            if in_fault and fault == "dropout": pass                                     # send nothing
            else:
                s = self.sample(v, fault if in_fault else None)
                if in_fault and fault == "stuck":
                    frozen = frozen or s; s = frozen
                self.fc.hil_send(*s); self.samples_sent += 1
            t = self.fc.last_telem
            if t is not None and t.uptime_ms != self._last_n:
                self._last_n = t.uptime_ms
                if rel > settle:
                    tr, tp, _, _ = self.truth.at(t.uptime_ms / 1000.0)
                    self.errors.append((t.uptime_ms / 1000.0, t.roll - tr, t.pitch - tp, t.mode))
                if on_tick: on_tick(rel, t)
            time.sleep(0.01 / max(self.speed, 0.05))

    def rms_deg(self, nominal_only=True):
        e = [x for x in self.errors if (x[3] == "NOMINAL" or not nominal_only)]
        if not e: return float("nan"), float("nan")
        r = math.sqrt(sum(x[1] ** 2 for x in e) / len(e)); p = math.sqrt(sum(x[2] ** 2 for x in e) / len(e))
        return math.degrees(r), math.degrees(p)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="tcp://localhost:4001"); ap.add_argument("--seconds", type=float, default=30)
    ap.add_argument("--fault", choices=["dropout", "stuck", "nan", "spike"]); ap.add_argument("--fault-at", type=float, default=15); ap.add_argument("--fault-len", type=float, default=5)
    a = ap.parse_args()
    with FcLink(a.port) as fc:
        fc.ensure_application(); h = HilRunner(fc)
        h.run(a.seconds, (a.fault_at, a.fault_at + a.fault_len) if a.fault else None, a.fault,
              on_tick=lambda rel, t: print(f"t={rel:6.1f}s mode={t.mode:8s} roll={math.degrees(t.roll):7.2f} pitch={math.degrees(t.pitch):7.2f}") if int(rel * 10) % 20 == 0 else None)
        r, p = h.rms_deg(); print(f"RMS error  roll={r:.2f} deg  pitch={p:.2f} deg  ({len(h.errors)} samples, {h.samples_sent} sensor frames sent)")


if __name__ == "__main__":
    main()
