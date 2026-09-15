#!/usr/bin/env python3
"""Unpack FF8DISC1.IMG into the files listed in its lock file.

The image SHA1 is checked against the lock before anything is written,
and each file is checked against its own SHA1 as it is unpacked.

Usage:
    python3 tools/unpackimg.py <image> <lock> [-o out_dir] [--check]
"""

import argparse
import hashlib
import os
import sys

import yaml

SECTOR_SIZE = 2048


def sha1_of_file(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        while chunk := f.read(1 << 20):
            h.update(chunk)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description="Unpack FF8DISC1.IMG using a lock file.")
    parser.add_argument("image")
    parser.add_argument("lock")
    parser.add_argument("-o", "--out-dir", default="extracted")
    parser.add_argument("--check", action="store_true", help="verify checksums, write nothing")
    args = parser.parse_args()

    with open(args.lock) as f:
        lock = yaml.safe_load(f)

    image_sha1 = sha1_of_file(args.image)
    if image_sha1 != lock["image"]["sha1"]:
        print(f"{args.image}: SHA1 {image_sha1}, expected {lock['image']['sha1']}", file=sys.stderr)
        return 1

    count = 0
    failed = 0
    with open(args.image, "rb") as img:
        for entry in lock["entries"]:
            img.seek(entry["sector"] * SECTOR_SIZE)
            data = img.read(entry["size"])
            if hashlib.sha1(data).hexdigest() != entry["sha1"]:
                print(f"{entry['path']}: SHA1 mismatch", file=sys.stderr)
                failed += 1
                continue
            count += 1
            if args.check:
                continue
            path = os.path.join(args.out_dir, entry["path"])
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as f:
                f.write(data)

    if args.check:
        print(f"{count} entries OK")
    else:
        print(f"unpacked {count} entries to {args.out_dir}/")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
