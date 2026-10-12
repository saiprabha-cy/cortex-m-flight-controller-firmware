"""pytest fixtures for the Renode hardware-in-the-loop (HIL) suite.

Requirements: renode on PATH (or RENODE=...), `pip install pytest cryptography`, firmware built into ./build.
Run:  pytest tests/integration -v            (add -m "not slow" to skip the long update/rollback tests)
Each test boots a fresh emulated board, so tests are independent."""
import itertools, os, pathlib, shutil, sys
import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from fc_link import FcLink          # noqa: E402
from renode_ctl import Renode       # noqa: E402

BUILD = ROOT / "build"
_ports = itertools.count(0)


def pytest_configure(config):
    config.addinivalue_line("markers", "slow: long-running scenario (firmware upload, rollback)")


def _require_env():
    if not (os.environ.get("RENODE") or shutil.which("renode")):
        pytest.skip("renode not installed (set RENODE=/path/to/renode)")
    if not (BUILD / "bootloader.elf").exists():
        pytest.skip("firmware not built: run scripts/build.sh first")


class Board:
    """One emulated STM32F407 board with the bootloader + (optionally) images preloaded."""
    def __init__(self, slot_a="app_a.signed.bin", slot_b=None, app_running=True, link_timeout=8.0):
        n = next(_ports)
        self.renode = Renode(monitor_port=12400 + n)
        self.log_path = BUILD / "logs" / f"uart1_{n}.log"; self.log_path.parent.mkdir(exist_ok=True)
        if self.log_path.exists(): self.log_path.unlink()
        self.renode.cmd(f"$slotA=@{BUILD / slot_a}")
        self.renode.cmd(f"$linkport={4100 + n}")
        self.renode.cmd(f"$uart1log=@{self.log_path}")
        self.renode.cmd(f"$bootloader=@{BUILD / 'bootloader.elf'}")
        self.renode.include("renode/flight_controller.resc")
        if slot_b:
            self.renode.cmd(f"sysbus LoadBinary @{BUILD / slot_b} 0x08080000")
        self.renode.start()
        self.fc = FcLink(f"tcp://localhost:{4100 + n}", timeout=link_timeout)
        if app_running:
            self.fc.ensure_application()

    def debug_log(self):
        return self.log_path.read_text(errors="replace").replace("\r", "") if self.log_path.exists() else ""

    def boot_count(self):
        return self.debug_log().count("=== BOOTLOADER")

    def wait_boot_count(self, n, timeout=120):
        """Block until the bootloader banner has been printed n times (i.e. the MCU really reset)."""
        import time
        t0 = time.time()
        while time.time() - t0 < timeout:
            if self.boot_count() >= n: return
            try: self.fc.pump(0.2)
            except Exception: time.sleep(0.2)
        raise TimeoutError(f"MCU did not reset (boot count {self.boot_count()} < {n})")

    def wait_reboot_to_app(self, timeout=60):
        """After a fault the board resets; wait until the application sends telemetry again."""
        import time
        t0 = time.time()
        while time.time() - t0 < timeout:
            try:
                self.fc.ensure_application(timeout=10); return
            except Exception:
                self.fc.pump(0.2)
        raise TimeoutError("board did not come back")

    def close(self):
        try: self.fc.close()
        finally: self.renode.close()


@pytest.fixture
def board():
    _require_env()
    b = Board()
    yield b
    b.close()


@pytest.fixture
def make_board():
    _require_env()
    made = []
    def factory(**kw):
        b = Board(**kw); made.append(b); return b
    yield factory
    for b in made: b.close()
