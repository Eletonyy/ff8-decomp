#!/usr/bin/env python3
"""Dump the FF8 discs found in rom and lay out the build's targets.

Usage:
    python3 tools/extract/extractdisc.py [--iso-dir rom] [--bin-dir original/bin]
"""

import argparse
import glob
import os
import subprocess
import sys

import yaml

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from unlzs import decompress
from unpackimg import sha1_of_file, unpack

DUMPSXISO = "tools/mkpsxiso/build/Release/dumpsxiso"

IMAGE_SUFFIXES = (".bin", ".img", ".iso")

DISCS = [
    {"id": "SLUS-00892", "disc": 1, "sha1": "5028a4f815a50d6ffe16f9c3e44ec6235a81f3f5"},
    {"id": "SLUS-00908", "disc": 2, "sha1": "2af26ce916c1b5bbb74fbacb0a8ca1a5171dc510"},
    {"id": "SLUS-00909", "disc": 3, "sha1": "9417003499803d9a9c3d26783570a967b0e4fba2"},
    {"id": "SLUS-00910", "disc": 4, "sha1": "68a3e8e96eb63018f728014a8044959bcb1b2272"},
]

DISC1_IMG = "FF8DISC1.IMG"
DISC1_LOCK = "config/SLUS_008.92_FF8DISC1.IMG.lock.yaml"

BIN_CONFIG = "config/original_bin.yaml"

GREEN = "\033[0;32m"
LIGHT_BLUE = "\033[0;94m"
RESET = "\033[0m"


def find_images(iso_dir):
    """Map SHA1 to path for every disc image sitting in iso_dir."""
    found = {}
    for name in sorted(os.listdir(iso_dir)):
        path = os.path.join(iso_dir, name)
        if os.path.isfile(path) and name.lower().endswith(IMAGE_SUFFIXES):
            found[sha1_of_file(path)] = path
    return found


def report(found):
    print(f"{'DISC':<6} {'ID':<12} {'SHA1':<40} STATUS")
    print(f"{'-' * 6:<6} {'-' * 12:<12} {'-' * 40:<40} {'-' * 6}")
    for disc in DISCS:
        state = f"{GREEN}[FOUND]{RESET}" if disc["sha1"] in found else f"{LIGHT_BLUE}[MISSING]{RESET}"
        print(f"{disc['disc']:<6} {disc['id']:<12} {disc['sha1']:<40} {state}")
    print()


def dump_disc(image, out_dir):
    subprocess.run([DUMPSXISO, image, "-x", out_dir, "--noxml"], check=True)


def lay_out_binaries(disc_dir, bin_dir, entries):
    """Copy disc 1's overlays to bin_dir, decompressing the LZS-packed ones."""
    count = 0
    for entry in entries:
        dest_dir = os.path.join(bin_dir, entry["dst"])
        os.makedirs(dest_dir, exist_ok=True)
        sources = sorted(glob.glob(os.path.join(disc_dir, entry["src"])))
        if not sources:
            print(f"Error: no file matches {entry['src']}", file=sys.stderr)
            return None
        for src in sources:
            with open(src, "rb") as f:
                data = f.read()
            if entry["lzs"]:
                data = decompress(data)
            with open(os.path.join(dest_dir, os.path.basename(src)), "wb") as f:
                f.write(data)
            count += 1
    return count


def main():
    parser = argparse.ArgumentParser(description="Dump the FF8 discs and lay out the build's targets.")
    parser.add_argument("--iso-dir", default="rom", help="where the disc images live")
    parser.add_argument("--bin-dir", default="original/bin", help="where the build's targets go")
    args = parser.parse_args()

    with open(BIN_CONFIG) as f:
        entries = yaml.safe_load(f)["files"]

    print("Detecting ISO's...")
    found = find_images(args.iso_dir)
    report(found)
    if DISCS[0]["sha1"] not in found:
        print(f"Error: no disc 1 image in {args.iso_dir}", file=sys.stderr)
        return 1

    for disc in DISCS:
        image = found.get(disc["sha1"])
        if image is None:
            continue
        if disc["disc"] != 1:
            print(f"Skipping Disc {disc['id']}, the project is not there yet")
            continue

        disc_dir = os.path.join(args.iso_dir, disc["id"])
        print(f"Extracting Disc {disc['id']}...")
        os.makedirs(disc_dir, exist_ok=True)
        dump_disc(image, disc_dir)
        if unpack(os.path.join(disc_dir, DISC1_IMG), DISC1_LOCK, disc_dir):
            return 1

        print(f"Copying Disc {disc['id']} binaries to {args.bin_dir}...")
        count = lay_out_binaries(disc_dir, args.bin_dir, entries)
        if count is None:
            return 1
        print(f"laid out {count} binaries in {args.bin_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
