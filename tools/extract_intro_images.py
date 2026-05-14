#!/usr/bin/env python3
"""
Extract every intro-overlay slide from FF8 Disc 1 as a PNG.

The intro overlay (originally "display_init") streams 36 LZSS-compressed
640x400 / 580x406 BGR555 frames off the CD. Reading sectors from the
user-supplied disc image, this script:

  1. Pulls the Mode 2 Form 1 data area for each entry in g_introAssetTable
     (sector + size from src/intro_assets.c).
  2. LZSS-decodes the payload using the same algorithm as func_80039520
     (4-byte size prefix, LSB-first control byte, 12-bit offset + 4-bit
     length back-references).
  3. Parses the 8-byte raw-bitmap header (width @ +4, height @ +6 LE).
  4. Renders pixels as PSX BGR555 (5b/5g/5r in a u16, STP bit ignored)
     into a PIL Image and writes a PNG per asset.

Usage:
  ./tools/extract_intro_images.py path/to/disc.bin [out_dir]

The output is *proprietary game data* — do not commit the PNGs to a
public repository.
"""

import argparse
import os
import struct
import sys
from pathlib import Path

# (sector LBA, size in bytes) — mirrors g_introAssetTable in src/intro_assets.c.
ASSETS = [
    (0x000246D9, 0x0002E5F2, "disc-swap prompt for Disc 1"),
    (0x00024736, 0x0002CE46, "disc-swap prompt for Disc 2"),
    (0x00024790, 0x0002D772, "disc-swap prompt for Disc 3"),
    (0x000247EB, 0x0002C834, "disc-swap prompt for Disc 4"),
    (0x00024845, 0x0000F199, "Final Fantasy VIII logo"),
    (0x00024864, 0x0000FCBC, "intro slide 5"),
    (0x00024884, 0x0000F96C, "intro slide 6"),
    (0x000248A4, 0x00024404, "intro slide 7 (full image)"),
    (0x000248ED, 0x00029BC7, "intro slide 8 (full image)"),
    (0x00024941, 0x0000FC47, "intro slide 9"),
    (0x00024961, 0x00010186, "intro slide 10"),
    (0x00024982, 0x00023D6F, "intro slide 11 (full image)"),
    (0x000249CA, 0x000221CF, "intro slide 12 (full image)"),
    (0x00024A0F, 0x0000FCA1, "intro slide 13"),
    (0x00024A2F, 0x0000FF14, "intro slide 14"),
    (0x00024A4F, 0x00020597, "intro slide 15 (full image)"),
    (0x00024A90, 0x00021378, "intro slide 16 (full image)"),
    (0x00024AD3, 0x00010261, "intro slide 17"),
    (0x00024AF4, 0x0000FF48, "intro slide 18"),
    (0x00024B14, 0x0002144C, "intro slide 19 (full image)"),
    (0x00024B57, 0x00026CAD, "intro slide 20 (full image)"),
    (0x00024BA5, 0x00010236, "intro slide 21"),
    (0x00024BC6, 0x000107D9, "intro slide 22"),
    (0x00024BE7, 0x00029B94, "intro slide 23 (full image)"),
    (0x00024C3B, 0x000279E1, "intro slide 24 (full image)"),
    (0x00024C8B, 0x00010054, "intro slide 25"),
    (0x00024CAC, 0x0000FFB0, "intro slide 26"),
    (0x00024CCC, 0x0001FE02, "intro slide 27 (full image)"),
    (0x00024D0C, 0x00026F37, "intro slide 28 (full image)"),
    (0x00024D5A, 0x0001007E, "intro slide 29"),
    (0x00024D7B, 0x0000FBD7, "intro slide 30"),
    (0x00024D9B, 0x00026F1F, "intro slide 31 (full image)"),
    (0x00024DE9, 0x0002D5DA, '"The End" title card'),
    (0x00024E44, 0x00015485, "final fade frame"),
    (0x00024E6F, 0x0000FF09, '"CAUTION WRONG DISC" warning'),
    (0x00024E8F, 0x0000F131, "Squaresoft logo"),
]

# PSX raw-mode disc image: 2352-byte sectors, Mode 2 Form 1 user data at +24.
SECTOR_SIZE = 2352
DATA_OFFSET = 24
DATA_BYTES  = 2048


def read_sectors(image_path: Path, lba: int, size: int) -> bytes:
    """Read `size` bytes starting at sector `lba`, stitching 2048-byte data areas."""
    sectors_needed = (size + DATA_BYTES - 1) // DATA_BYTES
    out = bytearray()
    with open(image_path, "rb") as f:
        for s in range(sectors_needed):
            f.seek((lba + s) * SECTOR_SIZE + DATA_OFFSET)
            out.extend(f.read(DATA_BYTES))
    return bytes(out[:size])


def lzss_decode(data: bytes) -> bytes:
    """Port of func_80039520 — FF8 LZSS variant (4K window, 4-bit length+3).

    Stream:
        u32 cmpr_size_LE
        [cmpr_size bytes of LZSS payload]

    Payload is groups of one control byte + up to 8 (literal | back-ref):
        bit set   = next byte is a literal
        bit clear = next 2 bytes are (lo8 offset, hi4 offset | length-3)
    """
    size = int.from_bytes(data[:4], "little")
    src  = data[4:4 + size]
    pos  = 0
    out  = bytearray()
    while pos < len(src):
        ctrl = src[pos]; pos += 1
        for _ in range(8):
            if pos >= len(src):
                return bytes(out)
            if ctrl & 1:
                out.append(src[pos]); pos += 1
            else:
                if pos + 1 >= len(src):
                    return bytes(out)
                b1, b2 = src[pos], src[pos + 1]; pos += 2
                off    = b1 | ((b2 & 0xF0) << 4)        # 12-bit
                length = (b2 & 0x0F) + 3
                cur    = len(out)
                disp   = (cur - (off - 0xFEE)) & 0xFFF
                sp     = cur - disp
                for _ in range(length):
                    out.append(out[sp] if 0 <= sp < len(out) else 0)
                    sp += 1
            ctrl >>= 1
    return bytes(out)


def bgr555_to_rgb(b16: int) -> tuple[int, int, int]:
    """PSX 16bpp: low-to-high in the u16 = 5R 5G 5B (1 STP). Ignore STP."""
    r = (b16 & 0x001F) << 3
    g = (b16 & 0x03E0) >> 2
    b = (b16 & 0x7C00) >> 7
    return r, g, b


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("disc_bin", type=Path, help="FF8 Disc 1 .bin (raw 2352-byte sectors)")
    ap.add_argument("out_dir",  type=Path, nargs="?", default=Path("intro_pngs"))
    args = ap.parse_args()

    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        sys.exit("error: Pillow is required.  pip install Pillow")
    from PIL import Image

    args.out_dir.mkdir(parents=True, exist_ok=True)
    print(f"Reading {args.disc_bin}")
    print(f"Writing to {args.out_dir}/")
    for idx, (lba, size, label) in enumerate(ASSETS):
        raw = read_sectors(args.disc_bin, lba, size)
        dec = lzss_decode(raw)
        if len(dec) < 8:
            print(f"  asset {idx:02d}: decode too short ({len(dec)}) — skipped")
            continue
        _hdr0, _hdr1, w, h = struct.unpack_from("<HHHH", dec, 0)
        pixels = dec[8:]
        if len(pixels) < w * h * 2:
            print(f"  asset {idx:02d}: pixel data short for {w}x{h} — skipped")
            continue
        img = Image.new("RGB", (w, h))
        pix = img.load()
        for y in range(h):
            row = struct.unpack_from(f"<{w}H", pixels, y * w * 2)
            for x, v in enumerate(row):
                pix[x, y] = bgr555_to_rgb(v)
        out_path = args.out_dir / f"intro_{idx:02d}.png"
        img.save(out_path)
        print(f"  asset {idx:02d}: {w}x{h}  cmpr={size:>6}  decmp={len(dec):>6}  -> {out_path.name}  ({label})")


if __name__ == "__main__":
    main()
