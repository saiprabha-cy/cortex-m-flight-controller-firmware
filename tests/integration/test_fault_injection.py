"""Fault-injection campaign: each case provokes a failure on the running firmware and checks that the system
(1) detects it, (2) records it in the flash fault log, (3) recovers or fails safe. See docs/07_fmea_fault_analysis.md."""
import time
import pytest
from fc_link import FcLink, LinkTimeout

# name -> (record types that must appear in the persistent fault log afterwards)
# Renode 1.17 does not model the Cortex-M DIV_0_TRP / UNALIGN_TRP traps, so those two cases are expected to fail
# in the emulator (xfail, non-strict) and are meant to pass on silicon / QEMU.
EMULATOR_LIMITED = {"DIV_ZERO": "Renode does not implement CCR.DIV_0_TRP", "UNALIGNED": "Renode does not implement CCR.UNALIGN_TRP"}

CASES = {
    "NULL_CALL":       ["USAGEFAULT"],                  # blx to address 0: Thumb bit clear -> INVSTATE UsageFault (CFSR.INVSTATE)
    "DIV_ZERO":        ["USAGEFAULT"],
    "UNALIGNED":       ["USAGEFAULT"],
    "STACK_OVERFLOW":  ["STACK_OVF|USAGEFAULT|HARDFAULT|BUSFAULT|MEMMANAGE"],   # see FMEA FM-09: deep recursion may corrupt before the RTOS check runs; any detection + reset is a pass
    "MALLOC_FAIL":     ["MALLOC_FAIL"],
    "ASSERT":          ["ASSERT"],
    "HANG_SENSOR":     ["TASK_WDG", "WDG_RESET"],      # SW watchdog notices, withholds the IWDG kick, MCU resets
    "HANG_CONTROL":    ["TASK_WDG", "WDG_RESET"],
    "IRQ_LOCKUP":      ["WDG_RESET"],                   # nothing can run: only the hardware watchdog helps
    "HANG_SUPERVISOR": ["WDG_RESET"],                   # the supervisor itself dies: IWDG stops being kicked
}


@pytest.mark.parametrize("fault", list(CASES))
def test_fault_is_logged_and_system_recovers(board, fault):
    if fault in EMULATOR_LIMITED:
        pytest.xfail(EMULATOR_LIMITED[fault])
    fc = board.fc
    before = len(fc.faults())
    boots = board.boot_count()
    fc.inject(fault, 0)
    board.wait_boot_count(boots + 1, timeout=120)          # the hardware watchdog needs ~1 s of firmware time to fire
    board.wait_reboot_to_app(timeout=90)
    recs = fc.faults()
    new_types = [r["type"] for r in recs[before:]]
    for need in CASES[fault]:
        assert any(n in new_types for n in need.split("|")), f"{fault}: expected {need} in fault log, got {new_types}"
    assert fc.version()["mode"] in ("NOMINAL", "DEGRADED", "BOOT")
    if fault == "NULL_CALL":
        r = [x for x in recs if x["type"] == "USAGEFAULT"][-1]
        assert r["cfsr"] & (1 << 17), "CFSR.INVSTATE expected for a call through a NULL function pointer"
        assert 0x08020200 <= r["pc"] < 0x08080000, "stacked PC must point into slot A"
    log = board.debug_log()
    assert "BOOTLOADER" in log


def test_repeated_crashes_force_safe_mode_then_ground_recovery(board):
    fc = board.fc
    for i in range(3):
        boots = board.boot_count(); fc.inject("NULL_CALL", 0); board.wait_boot_count(boots + 1, timeout=120); board.wait_reboot_to_app(timeout=90)
    t = fc.wait_telem(3)
    assert t.mode == "SAFE", "after 3 consecutive abnormal resets the firmware must boot into SAFE"
    assert t.act_us == (1500, 1500, 1500, 1500)
    assert "reset limit" in board.debug_log()
    fc.set_mode(safe=False); fc.wait_telem(3)
    assert fc.last_telem.mode == "NOMINAL", "ground command can leave SAFE"


def test_clear_fault_log(board):
    fc = board.fc
    assert fc.fault_count() >= 1
    fc.clear_faultlog(); assert fc.fault_count() == 0
