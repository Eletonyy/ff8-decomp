#!/usr/bin/env python3
"""Decompress FF8 LZS files.

Format: a u32 byte count of the compressed payload that follows, then groups
of one flag byte and eight tokens, flag bits read LSB first. A set bit is a
literal byte; a clear bit is a two-byte back-reference into a 4096-byte ring
buffer: 12-bit position (low byte, then the high nibble of the second byte)
and 4-bit length + 3. The ring starts zeroed with the write position at 0xFEE.

Usage:
    python3 tools/extract/unlzs.py [--copy-raw] <in>... <out>

<out> is a file when there is one input, a directory otherwise. --copy-raw
copies an input that carries no LZS header instead of failing, so a mixed
directory of packed and stored files can go through one call.
"""

import os
import shutil
import struct
import sys

RING_SIZE = 4096
RING_START = 0xFEE


def is_lzs(data):
    return len(data) >= 4 and struct.unpack_from("<I", data)[0] == len(data) - 4


def decompress(data):
    (size,) = struct.unpack_from("<I", data)
    if size != len(data) - 4:
        raise ValueError(f"header says {size} bytes, file holds {len(data) - 4}")
    src = data[4:]
    ring = bytearray(RING_SIZE)
    pos = RING_START
    out = bytearray()
    i = 0
    while i < len(src):
        flags = src[i]
        i += 1
        for bit in range(8):
            if i >= len(src):
                break
            if flags & (1 << bit):
                byte = src[i]
                i += 1
                out.append(byte)
                ring[pos] = byte
                pos = (pos + 1) % RING_SIZE
            else:
                lo, hi = src[i], src[i + 1]
                i += 2
                ref = lo | ((hi & 0xF0) << 4)
                for k in range((hi & 0x0F) + 3):
                    byte = ring[(ref + k) % RING_SIZE]
                    out.append(byte)
                    ring[pos] = byte
                    pos = (pos + 1) % RING_SIZE
    return bytes(out)


def main():
    args = sys.argv[1:]
    copy_raw = "--copy-raw" in args
    if copy_raw:
        args.remove("--copy-raw")
    if len(args) < 2:
        lines = __doc__.strip().splitlines()
        print(lines[lines.index("Usage:") + 1].strip(), file=sys.stderr)
        return 2
    sources, dest = args[:-1], args[-1]
    if len(sources) > 1 and not os.path.isdir(dest):
        print(f"{dest}: not a directory", file=sys.stderr)
        return 2

    for src in sources:
        out = os.path.join(dest, os.path.basename(src)) if os.path.isdir(dest) else dest
        with open(src, "rb") as f:
            data = f.read()
        if not is_lzs(data):
            if not copy_raw:
                print(f"{src}: no LZS header", file=sys.stderr)
                return 1
            shutil.copyfile(src, out)
            continue
        with open(out, "wb") as f:
            f.write(decompress(data))
    return 0


if __name__ == "__main__":
    sys.exit(main())
