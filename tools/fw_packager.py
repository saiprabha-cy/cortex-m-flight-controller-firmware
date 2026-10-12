#!/usr/bin/env python3
"""fw_packager.py - wraps a raw application .bin into a signed, slot-specific image.

  header (512 B) = magic, versions, slot, size, load address, anti-rollback counter, CRC32 + SHA-256 of payload,
                   header CRC32, Ed25519 signature over header bytes [0,448)
  output         = header || payload

The layout MUST match common/image_header.h (checked by tests/integration/test_packager_interop.py, which feeds the
output to the very same C verifier the bootloader uses).

Negative-test switches (--corrupt) produce deliberately broken images for the rollback / security demos."""
import argparse, hashlib, pathlib, struct, sys, time, zlib
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

MAGIC = 0x31574346
SLOT_BASE = {"A": 0x08020000, "B": 0x08080000}
HDR = 512; SIG_OFF = 448; CRC_OFF = 444

def parse_version(v):
    a = [int(x) for x in v.split(".")]
    if len(a) != 3 or a[0] > 255 or a[1] > 255 or a[2] > 65535: raise SystemExit("version must be MAJOR.MINOR.PATCH")
    return (a[0] << 24) | (a[1] << 16) | a[2]

def build(payload, slot, version, sec, key_seed, flags=0, build_time=None, corrupt=None):
    pl = bytearray(payload)
    while len(pl) % 4: pl.append(0xFF)                       # flash is programmed in 32-bit words
    base = SLOT_BASE[slot]
    crc = zlib.crc32(bytes(pl)) & 0xFFFFFFFF
    sha = hashlib.sha256(bytes(pl)).digest()
    bt = int(time.time()) if build_time is None else build_time
    head = struct.pack("<IHHIIIIIII32s", MAGIC, 1, 0 if slot == "A" else 1, len(pl), base + HDR, version, sec, flags, bt, crc, sha)
    head += b"\x00" * (CRC_OFF - len(head))
    head += struct.pack("<I", zlib.crc32(head) & 0xFFFFFFFF)
    assert len(head) == SIG_OFF
    sig = Ed25519PrivateKey.from_private_bytes(key_seed).sign(head)
    img = bytearray(head + sig + bytes(pl)); assert len(img) == HDR + len(pl)
    if corrupt == "payload": img[HDR + len(pl) // 2] ^= 0x01
    elif corrupt == "signature": img[SIG_OFF + 3] ^= 0x01
    elif corrupt == "header": img[16] ^= 0x01
    elif corrupt == "truncate": img = img[: len(img) - 64]
    return bytes(img)

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", required=True); ap.add_argument("--out", required=True)
    ap.add_argument("--slot", choices=["A", "B"], required=True); ap.add_argument("--version", default="1.0.0")
    ap.add_argument("--sec-counter", type=int, default=1); ap.add_argument("--key", default=str(pathlib.Path(__file__).resolve().parent.parent / "keys/dev_private.key"))
    ap.add_argument("--corrupt", choices=["payload", "signature", "header", "truncate"])
    a = ap.parse_args()
    seed = bytes.fromhex(pathlib.Path(a.key).read_text().strip())
    img = build(pathlib.Path(a.bin).read_bytes(), a.slot, parse_version(a.version), a.sec_counter, seed, corrupt=a.corrupt)
    pathlib.Path(a.out).write_bytes(img)
    print(f"{a.out}: slot {a.slot} v{a.version} sec={a.sec_counter} payload={len(img)-HDR} B sha256={hashlib.sha256(img[HDR:]).hexdigest()[:16]}...{' CORRUPT=' + a.corrupt if a.corrupt else ''}")

if __name__ == "__main__":
    main()
