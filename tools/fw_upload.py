#!/usr/bin/env python3
"""fw_upload.py - firmware updater client for the bootloader's framed-UART protocol.

  python3 tools/fw_upload.py --port tcp://localhost:4001 build/app_v2_b.signed.bin [--no-boot]

Flow: (1) if the application is running, ask it to reboot into the bootloader (REQ_UPDATE),
      (2) BL_HELLO -> bootloader tells which slot is the safe target; the image's slot must match,
      (3) BL_ERASE, BL_WRITE chunks (stop-and-wait, retried with the same sequence number),
      (4) BL_FINISH -> bootloader verifies CRC32 + SHA-256 + Ed25519 + anti-rollback; image becomes TRIAL,
      (5) BL_BOOT -> run it. The application confirms itself after ~10 s of healthy NOMINAL operation;
          otherwise the bootloader rolls back after 3 unconfirmed boots."""
import argparse, pathlib, struct, sys, time
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from fc_link import FcLink, LinkTimeout, IMG_STATUS, SLOT_STATE

CHUNK = 224


def bl_hello(fc):
    p = fc.request("BL_HELLO")
    maj, mi, pa, active, target, sa, sb, last, minsec, boots = struct.unpack("<BBBBBBBBII", p[:16])
    return dict(bl=f"{maj}.{mi}.{pa}", active="AB"[active], target="AB"[target], target_idx=target, state_a=SLOT_STATE[sa], state_b=SLOT_STATE[sb],
                last_boot_result=IMG_STATUS[last] if last < len(IMG_STATUS) else last, min_sec_counter=minsec, boot_count=boots)


def enter_bootloader(fc, timeout=60):
    """Make sure we are talking to the bootloader. Returns after a successful HELLO."""
    if fc.in_bootloader():
        return
    print("application running -> requesting update mode (reboot into bootloader)")
    fc.request_update(); fc.sock and time.sleep(0.5)
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            fc.request("BL_HELLO", retries=0, timeout=0.6); return
        except (LinkTimeout, RuntimeError):
            pass
    raise LinkTimeout("bootloader did not answer after REQ_UPDATE")


def upload(fc, image: bytes, boot=True, log=print, erase_timeout=120):
    slot_in_image = struct.unpack_from("<H", image, 6)[0]
    enter_bootloader(fc)
    h = bl_hello(fc); log(f"bootloader {h['bl']}: active={h['active']} target={h['target']} A={h['state_a']} B={h['state_b']}")
    if slot_in_image != h["target_idx"]:
        raise SystemExit(f"image is built for slot {'AB'[slot_in_image]} but the bootloader's safe target is slot {h['target']}. Use the matching build (app_*_{h['target'].lower()}).")
    log(f"erasing slot {h['target']} ..."); fc.request("BL_ERASE", bytes([h["target_idx"]]), timeout=erase_timeout, retries=1)
    t0 = time.time(); n = len(image)
    for off in range(0, n, CHUNK):
        chunk = image[off:off + CHUNK]
        chunk += b"\xFF" * (-len(chunk) % 4)
        fc.request("BL_WRITE", struct.pack("<I", off) + chunk, timeout=3)
        if (off // CHUNK) % 40 == 0: log(f"  {off * 100 // n:3d}%  ({off}/{n} bytes)")
    log(f"  100%  {n} bytes in {time.time() - t0:.1f}s")
    p = fc.request("BL_FINISH", timeout=30)
    log(f"image verified by bootloader: version {p[4]}.{p[3]}.{struct.unpack('<H', p[1:3])[0]} -> state TRIAL")
    if boot:
        fc.request("BL_BOOT", timeout=5); log("booting new image ...")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="tcp://localhost:4001"); ap.add_argument("image"); ap.add_argument("--no-boot", action="store_true")
    a = ap.parse_args()
    with FcLink(a.port, timeout=3) as fc:
        upload(fc, pathlib.Path(a.image).read_bytes(), boot=not a.no_boot)


if __name__ == "__main__":
    main()
