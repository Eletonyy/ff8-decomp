#!/usr/bin/env python3
"""Unpack .IMG files into the files listed in its lock file.

The image SHA1 is checked against the lock before anything is written,
and each file is checked against its own SHA1 as it is unpacked.

Usage:
    python3 tools/extract/unpackimg.py <image> <lock> [-o out_dir] [--check]
"""

import argparse
import hashlib
import os
import sys

import yaml


def sha1_of_file(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        while chunk := f.read(1 << 20):
            h.update(chunk)
    return h.hexdigest()


def unpack(image, lock_path, out_dir=None):
    with open(lock_path) as f:
        lock = yaml.safe_load(f)

    image_sha1 = sha1_of_file(image)
    if image_sha1 != lock["image"]["sha1"]:
        raise ValueError(f"{image}: SHA1 {image_sha1}, expected {lock['image']['sha1']}")

    sector_size = lock["image"]["sector_size"]
    count = 0
    failed = 0
    with open(image, "rb") as img:
        for entry in lock["entries"]:
            img.seek(entry["sector"] * sector_size)
            data = img.read(entry["size"])
            if hashlib.sha1(data).hexdigest() != entry["sha1"]:
                print(f"{entry['path']}: SHA1 mismatch", file=sys.stderr)
                failed += 1
                continue
            count += 1
            if out_dir is None:
                continue
            path = os.path.join(out_dir, entry["path"])
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as f:
                f.write(data)

    if out_dir is None:
        print(f"{count} entries OK")
    else:
        print(f"unpacked {count} entries to {out_dir}")
    return failed


def main():
    parser = argparse.ArgumentParser(description="Unpack FF8DISC1.IMG using a lock file.")
    parser.add_argument("image")
    parser.add_argument("lock")
    parser.add_argument("-o", "--out-dir", default="extracted")
    parser.add_argument("--check", action="store_true", help="verify checksums, write nothing")
    args = parser.parse_args()

    try:
        failed = unpack(args.image, args.lock, None if args.check else args.out_dir)
    except ValueError as e:
        print(e, file=sys.stderr)
        return 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
