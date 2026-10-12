"""Secure A/B update scenarios over the framed UART link (slow: each image upload takes ~30-40 s in the emulator)."""
import time
import pytest
from fc_link import FcLink, flag_list
from fw_upload import upload, bl_hello, enter_bootloader
from conftest import BUILD

pytestmark = pytest.mark.slow


def image(name): return (BUILD / name).read_bytes()


def test_good_update_is_installed_and_confirmed_after_healthy_run(board):
    fc = board.fc
    upload(fc, image("app_v2_b.signed.bin"))
    fc.wait_telem(2, timeout=60)
    v = fc.version(); assert v["fw"] == "1.1.0" and v["slot"] == "B" and not v["confirmed"]
    deadline = time.time() + 120
    while time.time() < deadline and "IMAGE_CONFIRMED" not in flag_list(fc.last_telem.flags): fc.pump(0.5)
    assert "IMAGE_CONFIRMED" in flag_list(fc.last_telem.flags), "image should confirm itself after ~10 s healthy NOMINAL"
    fc.reboot(); board.wait_reboot_to_app()
    v = fc.version(); assert v["fw"] == "1.1.0" and v["slot"] == "B", "confirmed image persists across reboot"


@pytest.mark.parametrize("kind", ["payload", "signature"])
def test_corrupt_image_is_rejected_and_old_firmware_keeps_running(board, kind):
    fc = board.fc
    with pytest.raises(RuntimeError, match="NAK"):
        upload(fc, image(f"bad_{kind}_b.signed.bin"), boot=False)
    h = bl_hello(fc); assert h["state_b"] == "BAD" and h["active"] == "A"
    fc.request("BL_BOOT"); fc.wait_telem(2, timeout=60)
    assert fc.version()["fw"] == "1.0.0" and fc.version()["slot"] == "A"


def test_wrong_slot_image_refused_by_uploader(board):
    with pytest.raises(SystemExit):
        upload(board.fc, image("app_v2_a.signed.bin"), boot=False)


def test_crashing_update_rolls_back_to_previous_image(board):
    fc = board.fc
    upload(fc, image("app_bad_b.signed.bin"))
    deadline = time.time() + 240; ok = False
    while time.time() < deadline:
        try:
            fc.ensure_application(timeout=20); v = fc.version()
            if v["slot"] == "A": ok = True; break
        except Exception: fc.pump(0.5)
    assert ok, "bootloader must fall back to slot A after 3 unconfirmed, crashing boots"
    assert v["fw"] == "1.0.0"
    types = [f["type"] for f in fc.faults()]
    assert "USAGEFAULT" in types and "BOOT_ROLLBACK" in types
    assert "marked BAD" in board.debug_log()
