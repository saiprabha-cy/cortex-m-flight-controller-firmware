"""The Python packager and the C verifier used by the bootloader must agree byte for byte (header layout, CRC32, SHA-256, Ed25519)."""
import pathlib, subprocess, sys, tempfile
import pytest
ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import fw_packager

VERIFY = ROOT / "build-host" / "verify_cli"
KEY = ROOT / "keys" / "dev_private.key"; PUB = ROOT / "keys" / "dev_public.bin"


@pytest.fixture(scope="module", autouse=True)
def _build():
    subprocess.check_call(["cmake", "-S", str(ROOT / "tests/unit"), "-B", str(ROOT / "build-host"), "-G", "Ninja"], stdout=subprocess.DEVNULL)
    subprocess.check_call(["cmake", "--build", str(ROOT / "build-host"), "--target", "verify_cli"], stdout=subprocess.DEVNULL)
    if not KEY.exists(): subprocess.check_call([sys.executable, str(ROOT / "tools/keygen.py")])


def run(img: bytes, slot: int, min_sec=0):
    with tempfile.NamedTemporaryFile(suffix=".bin") as f:
        f.write(img); f.flush()
        r = subprocess.run([str(VERIFY), f.name, str(PUB), str(slot), str(min_sec)], capture_output=True, text=True)
    return r.stdout.strip()


def pkg(slot="A", sec=5, corrupt=None):
    payload = bytearray(range(256)) * 20; payload[0:8] = b"\xF0\xFF\x01\x20\xAD\x03\x02\x08"   # SP=0x2001FFF0, reset vector inside slot A
    return fw_packager.build(bytes(payload), slot, 0x01020003, sec, bytes.fromhex(KEY.read_text().strip()), corrupt=corrupt)


def test_good_image(): assert run(pkg(), 0) == "OK"
def test_slot_b_image_in_slot_b():
    payload = bytearray(range(256)) * 4; payload[0:8] = b"\xF0\xFF\x01\x20\x01\x03\x08\x08"
    assert run(fw_packager.build(bytes(payload), "B", 1, 1, bytes.fromhex(KEY.read_text().strip())), 1) == "OK"
def test_wrong_slot(): assert run(pkg("A"), 1) == "WRONG_SLOT"
def test_rollback_blocked(): assert run(pkg(sec=5), 0, min_sec=6) == "ROLLBACK_BLOCKED"
@pytest.mark.parametrize("kind,expect", [("payload", "BAD_PAYLOAD_CRC"), ("signature", "BAD_SIGNATURE"), ("header", "BAD_HEADER_CRC")])
def test_corruptions(kind, expect): assert run(pkg(corrupt=kind), 0) == expect
