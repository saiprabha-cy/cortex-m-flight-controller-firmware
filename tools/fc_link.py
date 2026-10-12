#!/usr/bin/env python3
"""fc_link.py - host-side client for the flight-controller framed link (see common/frame.h).

Transport: a TCP socket (Renode socket terminal, default localhost:4001) or a pyserial URL/port.
    from fc_link import FcLink
    with FcLink("tcp://localhost:4001") as fc:
        print(fc.version()); print(fc.telemetry())

CLI quick tools:   python3 tools/fc_link.py --port tcp://localhost:4001 telem | version | stats | latency | faults | ping
"""
import argparse, socket, struct, sys, threading, time
from dataclasses import dataclass

SOF = b"\xA5\x5A"
MSG = dict(PING=0x01, GET_VERSION=0x02, GET_TELEM=0x03, SET_MODE=0x04, REBOOT=0x05, REQ_UPDATE=0x06, DUMP_FAULT=0x07,
           GET_STATS=0x08, CLEAR_FAULTLOG=0x09, SET_SENSOR_SRC=0x0A, GET_LATENCY=0x0B, CAN_NODE_CMD=0x0C,
           BL_HELLO=0x10, BL_ERASE=0x11, BL_WRITE=0x12, BL_FINISH=0x13, BL_BOOT=0x14,
           TELEM=0x40, EVENT=0x41, HIL_SENSOR=0x50, INJECT_FAULT=0x7E, NAK=0xFF)
ACK = 0x80
MODES = ["BOOT", "NOMINAL", "DEGRADED", "SAFE"]
FAULT_TYPES = ["NONE", "HARDFAULT", "MEMMANAGE", "BUSFAULT", "USAGEFAULT", "WDG_RESET", "BROWNOUT", "TASK_WDG", "STACK_OVF",
               "MALLOC_FAIL", "ASSERT", "NODE_LOST", "SENSOR", "COLD_BOOT", "SOFT_RESET", "BOOT_ROLLBACK"]
IMG_STATUS = ["OK", "BAD_MAGIC", "BAD_HEADER_CRC", "WRONG_SLOT", "BAD_SIZE", "BAD_LOAD_ADDR", "BAD_PAYLOAD_CRC", "BAD_SHA256",
              "BAD_SIGNATURE", "ROLLBACK_BLOCKED", "BAD_VECTOR_TABLE"]
SLOT_STATE = ["EMPTY", "TRIAL", "CONFIRMED", "BAD"]
INJECT = dict(HANG_SENSOR=1, NULL_CALL=2, STACK_OVERFLOW=3, SENSOR_STUCK=4, MALLOC_FAIL=5, DIV_ZERO=6, UNALIGNED=7,
              IRQ_LOCKUP=8, BUS_FAULT=9, HANG_CONTROL=10, HANG_SUPERVISOR=11, ASSERT=12)


def crc16_ccitt(data: bytes, crc: int = 0xFFFF) -> int:
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def encode(mtype: int, seq: int, payload: bytes = b"") -> bytes:
    if len(payload) > 240:
        raise ValueError("payload too long")
    body = bytes([len(payload), mtype, seq & 0xFF]) + payload
    return SOF + body + struct.pack(">H", crc16_ccitt(body))


class Parser:
    """Byte-stream parser mirroring common/frame.c (resynchronises on SOF)."""
    def __init__(self):
        self.buf = bytearray(); self.crc_errors = 0

    def feed(self, data: bytes):
        self.buf += data; out = []
        while True:
            i = self.buf.find(SOF)
            if i < 0:
                self.buf = self.buf[-1:] if self.buf[-1:] == SOF[:1] else bytearray(); break
            if i > 0:
                del self.buf[:i]
            if len(self.buf) < 5:
                break
            ln = self.buf[2]
            if ln > 240:
                del self.buf[:2]; continue
            total = 2 + 3 + ln + 2
            if len(self.buf) < total:
                break
            body = bytes(self.buf[2:5 + ln]); crc = struct.unpack(">H", self.buf[5 + ln:total])[0]
            if crc == crc16_ccitt(body):
                out.append((body[1], body[2], body[3:])); del self.buf[:total]
            else:
                self.crc_errors += 1; del self.buf[:2]
        return out


@dataclass
class Telem:
    mode: str; sensor_src: int; flags: int; uptime_ms: int
    roll: float; pitch: float; roll_rate: float; pitch_rate: float
    ax: float; ay: float; az: float; pressure_pa: float; altitude_m: float
    vbat_v: float; mcu_temp_c: float; node_vbat_v: float; node_current_a: float
    act_us: tuple; cpu_load_pct: int; slot: int

TELEM_FMT = "<BBHI" + "f" * 13 + "4HBB"      # 70 bytes == sizeof(telem_t)
FLAG_NAMES = ["SIM_FALLBACK", "IMU_FAULT", "BARO_FAULT", "NODE_LOST", "BROWNOUT_SEEN", "CAN_ERR_PASSIVE", "CLOCK_FALLBACK", "HIL_SOURCE", "IMAGE_CONFIRMED"]


def parse_telem(p: bytes) -> Telem:
    v = struct.unpack(TELEM_FMT, p[:struct.calcsize(TELEM_FMT)])
    return Telem(MODES[v[0]] if v[0] < 4 else str(v[0]), v[1], v[2], v[3], *v[4:17], tuple(v[17:21]), v[21], v[22])


def flag_list(flags: int):
    return [n for i, n in enumerate(FLAG_NAMES) if flags & (1 << i)]


class LinkTimeout(Exception):
    pass


class FcLink:
    def __init__(self, url="tcp://localhost:4001", timeout=3.0, retries=4, connect_timeout=30.0):
        self.url, self.timeout, self.retries = url, timeout, retries
        self.seq = 0; self.parser = Parser(); self.telem_cb = None; self.last_telem = None
        self.telem_count = 0; self.events = []; self.lock = threading.Lock()
        self.rawlog = None
        if url.startswith("tcp://"):
            host, port = url[6:].rsplit(":", 1); t0 = time.time()
            while True:
                try:
                    self.sock = socket.create_connection((host, int(port)), timeout=2); break
                except OSError:
                    if time.time() - t0 > connect_timeout:
                        raise
                    time.sleep(0.2)
            self.sock.settimeout(0.05); self._ser = None
        else:
            import serial   # pyserial
            self._ser = serial.serial_for_url(url if "://" in url else url, baudrate=115200, timeout=0.05); self.sock = None

    def close(self):
        (self.sock or self._ser).close()

    def __enter__(self): return self
    def __exit__(self, *a): self.close()

    # ---- raw I/O
    def _write(self, b: bytes):
        (self.sock.sendall(b) if self.sock else self._ser.write(b))

    def _read(self) -> bytes:
        try:
            return self.sock.recv(4096) if self.sock else self._ser.read(4096)
        except (socket.timeout, TimeoutError):
            return b""

    def pump(self, duration=0.0):
        """Read for `duration` seconds, dispatching unsolicited telemetry. Returns all frames seen."""
        end = time.time() + duration; seen = []
        while True:
            data = self._read()
            if data and self.rawlog is not None: self.rawlog.write(data)
            for t, s, p in self.parser.feed(data):
                seen.append((t, s, p)); self._dispatch(t, p)
            if time.time() >= end:
                return seen

    def _dispatch(self, t, p):
        if t == MSG["TELEM"]:
            self.last_telem = parse_telem(p); self.telem_count += 1
            if self.telem_cb: self.telem_cb(self.last_telem)
        elif t == MSG["EVENT"]:
            self.events.append(p)

    # ---- request/response with retry (same seq on retry: commands are idempotent)
    def request(self, name_or_type, payload=b"", expect_ack=True, timeout=None, retries=None):
        mt = MSG[name_or_type] if isinstance(name_or_type, str) else name_or_type
        self.seq = (self.seq + 1) & 0xFF; seq = self.seq
        frame = encode(mt, seq, payload); timeout = timeout or self.timeout
        for attempt in range((retries if retries is not None else self.retries) + 1):
            self._write(frame); end = time.time() + timeout
            while time.time() < end:
                for t, s, p in self.pump(0.0):
                    if s == seq and t == (mt | ACK):
                        return p
                    if s == seq and t == MSG["NAK"]:
                        raise RuntimeError(f"NAK for {name_or_type}: code {p[0] if p else '?'} {p[1:].hex() if len(p) > 1 else ''}")
        raise LinkTimeout(f"no response to {name_or_type} after {retries if retries is not None else self.retries} retries")

    # ---- bootloader / application handover
    def in_bootloader(self):
        """True if the peer is the bootloader (it answers PING but NAKs application commands)."""
        self.request("PING")
        try:
            self.request("GET_VERSION", retries=1, timeout=1.5); return False
        except RuntimeError:
            return True

    def ensure_application(self, timeout=60.0):
        """If the bootloader is waiting for a host (it does so when it sees any frame), tell it to boot, then wait for telemetry."""
        if self.in_bootloader():
            self.request("BL_BOOT", timeout=5)
        self.wait_telem(2, timeout=timeout)

    # ---- convenience
    def ping(self): self.request("PING"); return True
    def version(self):
        p = self.request("GET_VERSION"); fw, sec, slot, mode, rc, bl, conf = struct.unpack("<IIBBBBI", p)
        return dict(fw=f"{fw >> 24}.{(fw >> 16) & 255}.{fw & 0xFFFF}", fw_raw=fw, security_counter=sec, slot="AB"[slot], mode=MODES[mode], confirmed=bool(conf))
    def telemetry(self): return parse_telem(self.request("GET_TELEM"))
    def set_mode(self, safe: bool): self.request("SET_MODE", bytes([1 if safe else 0]))
    def set_sensor_source(self, n: int): self.request("SET_SENSOR_SRC", bytes([n]))
    def can_node_silence(self, on: bool): self.request("CAN_NODE_CMD", bytes([1 if on else 2]))
    def reboot(self): self.request("REBOOT")
    def request_update(self): self.request("REQ_UPDATE")
    def clear_faultlog(self): self.request("CLEAR_FAULTLOG", timeout=15)
    def fault_count(self): return struct.unpack("<I", self.request("DUMP_FAULT"))[0]
    def fault_record(self, idx):
        p = self.request("DUMP_FAULT", bytes([idx])); t, mode, info, pc, lr, cfsr, addr, up = struct.unpack("<BBHIIIII", p)
        return dict(type=FAULT_TYPES[t] if t < len(FAULT_TYPES) else t, mode=mode, info=info, pc=pc, lr=lr, cfsr=cfsr, addr=addr, uptime_ms=up)
    def faults(self): return [self.fault_record(i) for i in range(self.fault_count())]
    def inject(self, name, arg=0):
        try: self.request("INJECT_FAULT", struct.pack("<BI", INJECT[name], arg), retries=0, timeout=2)
        except LinkTimeout: pass

    def stats(self):
        p = self.request("GET_STATS")
        f = "<I8HIII" + "B3x" + "4I" + "4I" + "6IBBH" + "3I2I"
        v = struct.unpack(f, p[:struct.calcsize(f)])
        keys = ["uptime_ms"] + [f"stack_hwm_words_{n}" for n in ("sensor", "control", "comms", "telem", "can", "hk", "timer", "idle")] + \
               ["isr_stack_free_bytes", "heap_free", "heap_min_free", "cpu_load_pct", "frame_ok", "frame_crc_err", "frame_len_err", "frame_resync",
                "link_rx_overflow", "link_tx_overflow", "link_err_overrun", "link_err_framing", "can_rx", "can_tx", "can_tx_fail", "can_rx_overflow",
                "can_bus_off", "can_err_passive", "can_tec", "can_rec", "pad2", "wdg_soft_miss", "wdg_resets_seen", "faults_logged", "sensor_imu_fail", "sensor_baro_fail"]
        return dict(zip(keys, v))

    def latency(self):
        p = self.request("GET_LATENCY"); f = "<11I" + ("I" if len(p) >= 48 else "")
        v = struct.unpack(f, p[:struct.calcsize(f)])
        keys = ["isr_lat_min_cyc", "isr_lat_max_cyc", "isr_lat_avg_cyc", "isr_lat_samples", "sensor_jitter_max_us", "sensor_period_min_us",
                "sensor_period_max_us", "ctrl_exec_max_cyc", "ctrl_exec_avg_cyc", "ctrl_latency_max_us", "sysclk_hz", "dwt_is_hardware"]
        return dict(zip(keys, v))

    def hil_send(self, ax, ay, az, gx, gy, gz, pressure_pa=101325.0, temp_c=25.0):
        self.seq = (self.seq + 1) & 0xFF
        self._write(encode(MSG["HIL_SENSOR"], self.seq, struct.pack("<9f", ax, ay, az, gx, gy, gz, 0.0, pressure_pa, temp_c)))

    def wait_telem(self, n=1, timeout=10.0):
        target = self.telem_count + n; end = time.time() + timeout
        while self.telem_count < target:
            if time.time() > end: raise LinkTimeout("no telemetry")
            self.pump(0.05)
        return self.last_telem


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--port", default="tcp://localhost:4001")
    ap.add_argument("cmd", choices=["ping", "version", "telem", "stats", "latency", "faults"])
    a = ap.parse_args()
    with FcLink(a.port) as fc:
        r = dict(ping=fc.ping, version=fc.version, telem=fc.telemetry, stats=fc.stats, latency=fc.latency, faults=fc.faults)[a.cmd]()
        print(r)

if __name__ == "__main__":
    main()
