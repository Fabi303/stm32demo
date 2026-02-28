#!/usr/bin/env python3
"""Extract and verify the .build_metadata section from a firmware ELF.

Uses arm-none-eabi-objcopy to pull out the section as raw binary, then
parses the packed struct and verifies the CRC-32 — no Python dependencies
beyond the standard library.

Usage:
  python tools/read_build_meta.py build/debug/stm32f429i_demo

Exit code:
  0  metadata OK (CRC passed)
  1  error (bad magic, section missing, CRC mismatch)

Struct layout (71 bytes, little-endian, no padding):
  Offset  Size  Field
  ------  ----  -----
   0       4    magic    "META"  (no null terminator)
   4       9    commit   8-char git hash + NUL
  13       1    dirty    0 = clean, 1 = modified working tree
  14      32    branch   branch name + NUL
  46      12    date     __DATE__  "Mmm DD YYYY" + NUL
  58       9    time     __TIME__  "HH:MM:SS" + NUL
  67       4    crc32    CRC-32/ISO-HDLC of commit+dirty+branch+date+time (LE)

CRC algorithm: IEEE 802.3 (zlib polynomial), same as Python's zlib.crc32().
"""

import os
import struct
import subprocess
import sys
import tempfile
import zlib

# Must match BuildMetadata in src/build_metadata.cc
MAGIC       = b"META"
STRUCT_FMT  = "<4s9sB32s12s9sI"
STRUCT_SIZE = struct.calcsize(STRUCT_FMT)  # 71

assert STRUCT_SIZE == 71, f"Unexpected struct size {STRUCT_SIZE}"

def extract_section(elf_path: str) -> bytes:
    """Use arm-none-eabi-objcopy to dump .build_metadata as a raw binary."""
    with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as f:
        tmp = f.name
    try:
        result = subprocess.run(
            [
                "C:\\Arm\\arm-none-eabi\\bin\\arm-none-eabi-objcopy",
                "-O", "binary",
                "--only-section=.build_metadata",
                elf_path,
                tmp,
            ],
            capture_output=True,
        )
        if result.returncode != 0:
            print(f"ERROR: objcopy failed:\n{result.stderr.decode()}", file=sys.stderr)
            sys.exit(1)
        with open(tmp, "rb") as f:
            return f.read()
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def verify_crc(commit: bytes, dirty: int, branch: bytes, date: bytes, time_b: bytes) -> int:
    """Compute the expected CRC-32 over the metadata content fields.

    Byte sequence fed to zlib.crc32():
      commit_chars + [dirty_byte] + branch_chars + date_chars + time_chars
    All strings without their null terminator (mirrors the firmware's crc32_str).
    """
    payload = commit + bytes([dirty]) + branch + date + time_b
    return zlib.crc32(payload) & 0xFFFF_FFFF


def main() -> int:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <elf>", file=sys.stderr)
        return 1

    elf_path = sys.argv[1]
    if not os.path.isfile(elf_path):
        print(f"ERROR: file not found: {elf_path}", file=sys.stderr)
        return 1

    data = extract_section(elf_path)

    if len(data) < STRUCT_SIZE:
        print(
            f"ERROR: .build_metadata section is {len(data)} bytes "
            f"(expected >= {STRUCT_SIZE}).  Was the section added to the linker script?",
            file=sys.stderr,
        )
        return 1

    magic, commit, dirty, branch, date_b, time_b, stored_crc = struct.unpack_from(
        STRUCT_FMT, data
    )

    if magic != MAGIC:
        print(f"ERROR: bad magic {magic!r} (expected {MAGIC!r})", file=sys.stderr)
        return 1

    # Strip embedded NUL bytes for display and CRC input.
    commit_s = commit.rstrip(b"\x00")
    branch_s = branch.rstrip(b"\x00")
    date_s   = date_b.rstrip(b"\x00")
    time_s   = time_b.rstrip(b"\x00")

    computed = verify_crc(commit_s, dirty, branch_s, date_s, time_s)
    ok = computed == stored_crc

    print(f"Commit : {commit_s.decode()}{'-dirty' if dirty else ''}")
    print(f"Branch : {branch_s.decode()}")
    print(f"Built  : {date_s.decode()} {time_s.decode()}")
    print(
        f"CRC32  : {stored_crc:#010x}  "
        + ("OK" if ok else f"MISMATCH (computed {computed:#010x})")
    )

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
