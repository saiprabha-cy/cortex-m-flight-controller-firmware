"""Basic HIL tests: boot chain, telemetry, link robustness, mode commands, sensor fault handling, HIL estimation accuracy."""
import math, random, struct, time
import pytest
from fc_link import encode, MSG, flag_list


def test_boot_chain_and_version(board):
    v = board.fc.version()
    assert v["slot"] == "A" and v["fw"] == "1.0.0" and v["mode"] == "NOMINAL"
    log = board.debug_log()
    assert "BOOTLOADER v1.0.0" in log and "BL: jumping to slot A" in log and "FLIGHT CONTROLLER FW" in log


def test_telemetry_rate_and_content(board):
    fc = board.fc; fc.pump(0.1); n0 = fc.telem_count; up0 = fc.last_telem.uptime_ms
    fc.pump(4.0)
    frames = fc.telem_count - n0; virt_ms = fc.last_telem.uptime_ms - up0
    assert frames >= 5
    assert 90 <= virt_ms / frames <= 110, "telemetry should be 10 Hz in virtual time"
    t = fc.last_telem
    assert t.mode == "NOMINAL" and all(1000 <= a <= 2000 for a in t.act_us), "actuator pulses must stay inside 1000..2000 us"
    assert 0.9 < math.sqrt(t.ax ** 2 + t.ay ** 2 + t.az ** 2) < 1.1, "accelerometer should read ~1 g"


def test_stack_and_heap_headroom(board):
    s = board.fc.stats()
    for k, v in s.items():
        if k.startswith("stack_hwm_words_"): assert v > 40, f"{k} has only {v} words free"
    assert s["heap_free"] > 10000 and s["isr_stack_free_bytes"] > 2048
    assert s["wdg_soft_miss"] == 0


def test_garbage_and_corrupt_frames_do_not_break_link(board):
    fc = board.fc; rnd = random.Random(7)
    s0 = fc.stats()
    fc._write(bytes(rnd.randrange(256) for _ in range(300)))                      # line noise
    good = bytearray(encode(MSG["PING"], 99)); good[-1] ^= 0xFF; fc._write(bytes(good))  # bad CRC
    fc._write(b"\xA5\x5A\xF0\x01\x01")                                             # oversize length
    time.sleep(0.5); fc.pump(0.3)
    assert fc.ping()
    s1 = fc.stats()
    assert s1["frame_crc_err"] > s0["frame_crc_err"] or s1["frame_len_err"] > s0["frame_len_err"]


def test_unknown_command_is_nakd(board):
    with pytest.raises(RuntimeError, match="NAK"):
        board.fc.request(0x6F)


def test_safe_mode_command_and_recovery(board):
    fc = board.fc
    fc.set_mode(safe=True); fc.wait_telem(3)
    t = fc.last_telem; assert t.mode == "SAFE" and t.act_us == (1500, 1500, 1500, 1500)
    fc.set_mode(safe=False); fc.wait_telem(3); assert fc.last_telem.mode == "NOMINAL"


def test_stuck_sensor_degrades_then_recovers(board):
    fc = board.fc
    fc.inject("SENSOR_STUCK", 1)
    deadline = time.time() + 30
    while time.time() < deadline and fc.last_telem.mode != "DEGRADED": fc.pump(0.2)
    assert fc.last_telem.mode == "DEGRADED" and "IMU_FAULT" in flag_list(fc.last_telem.flags)
    assert max(fc.last_telem.act_us) - 1500 <= 500, "authority limited"
    fc.inject("SENSOR_STUCK", 0)
    deadline = time.time() + 60
    while time.time() < deadline and fc.last_telem.mode != "NOMINAL": fc.pump(0.3)
    assert fc.last_telem.mode == "NOMINAL"
    types = [f["type"] for f in fc.faults()]; assert "SENSOR" in types


def test_hil_estimation_accuracy(board):
    """Python plays the vehicle (paced on the firmware clock); the on-target Kalman estimate is compared with the truth."""
    from hil_sim import HilRunner
    h = HilRunner(board.fc); h.run(30.0, settle=10.0)
    r, p = h.rms_deg(); print(f"HIL attitude RMS error: roll {r:.2f} deg, pitch {p:.2f} deg ({len(h.errors)} samples)")
    assert len(h.errors) > 50 and r < 2.0 and p < 2.0


@pytest.mark.parametrize("fault", ["dropout", "nan", "spike", "stuck"])
def test_hil_sensor_stream_faults_are_detected(board, fault):
    """Corrupt the injected sensor stream; firmware must declare an IMU fault (DEGRADED) and recover when the stream is clean again."""
    from hil_sim import HilRunner
    h = HilRunner(board.fc); modes = []
    h.run(26.0, fault_window=(8.0, 12.0), fault=fault, on_tick=lambda rel, t: modes.append((rel, t.mode)))
    during = {m for rel, m in modes if 9.5 <= rel < 12.0}; after = {m for rel, m in modes if rel > 22.0}
    assert "DEGRADED" in during, f"{fault}: not detected (modes during fault: {during})"
    assert after == {"NOMINAL"}, f"{fault}: did not recover (modes late: {after})"
