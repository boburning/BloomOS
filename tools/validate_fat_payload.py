#!/usr/bin/env python3
"""Reject build-tree entries that cannot be extracted onto the FAT SD card."""

import argparse
import os
import pathlib
import sys


def incompatible_entries(root: pathlib.Path) -> list[str]:
    failures: list[str] = []
    for directory, directories, files in os.walk(root, followlinks=False):
        parent = pathlib.Path(directory)
        for name in directories + files:
            path = parent / name
            relative = path.relative_to(root).as_posix()
            if path.is_symlink():
                failures.append(f"symlink: {relative}")
            if relative == "pixelreader/work" or relative.startswith("pixelreader/work/"):
                failures.append(f"temporary PixelReader workspace: {relative}")
    return sorted(set(failures))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=pathlib.Path)
    args = parser.parse_args()
    root = args.root.resolve()
    if not root.is_dir():
        parser.error(f"payload root is not a directory: {root}")

    failures = incompatible_entries(root)
    if failures:
        print("FAT payload validation failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("FAT payload validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
