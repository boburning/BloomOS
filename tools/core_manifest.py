#!/usr/bin/env python3
"""Generate and validate BloomOS's shipped libretro core inventory."""

import argparse
import hashlib
import json
import pathlib
import re
import sys


SCHEMA = 1
ONION_BASELINE = "07505ea58c7bba698d6b9220ff43946a43cac76b"
DEVICES = ["mini-v1", "mini-v2", "mini-v3", "mini-v4", "plus", "flip"]
INFO_LINE = re.compile(r'^([a-zA-Z0-9_]+)\s*=\s*"(.*)"\s*$')


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_info(path):
    values = {}
    if not path.is_file():
        return values
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = INFO_LINE.match(line.strip())
        if match:
            values[match.group(1)] = match.group(2)
    return values


def build_manifest(core_dir):
    cores = []
    for binary in sorted(core_dir.glob("*_libretro.so"), key=lambda item: item.name):
        stem = binary.name[:-3]
        info_path = core_dir / f"{stem}.info"
        info = parse_info(info_path)
        cores.append(
            {
                "id": stem[:-len("_libretro")],
                "binary": binary.name,
                "bytes": binary.stat().st_size,
                "sha256": sha256(binary),
                "metadata": info_path.name if info_path.is_file() else None,
                "display_name": info.get("display_name"),
                "display_version": info.get("display_version"),
                "license_declared": info.get("license"),
                "upstream": None,
                "source_revision": None,
                "build_flags": None,
                "bloom_patch_set": [],
                "supported_devices": DEVICES,
                "known_issues": [],
                "last_validated": None,
            }
        )
    if not cores:
        raise ValueError(f"no libretro cores found in {core_dir}")
    return {
        "schema": SCHEMA,
        "inventory_scope": "shipped-libretro-binaries",
        "inherited_tree_commit": ONION_BASELINE,
        "cores": cores,
    }


def encoded(manifest):
    return json.dumps(manifest, indent=2, sort_keys=True, ensure_ascii=True) + "\n"


def validate(core_dir, manifest_path):
    try:
        actual = manifest_path.read_text(encoding="utf-8")
        stored = json.loads(actual)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read core manifest: {error}") from error
    expected = build_manifest(core_dir)
    if stored != expected or actual != encoded(expected):
        raise ValueError("core manifest is stale or non-canonical; run generate")
    return len(expected["cores"])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("generate", "validate"))
    parser.add_argument("--core-dir", type=pathlib.Path, required=True)
    parser.add_argument("--manifest", type=pathlib.Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "generate":
            manifest = build_manifest(args.core_dir)
            args.manifest.parent.mkdir(parents=True, exist_ok=True)
            with args.manifest.open("w", encoding="utf-8", newline="\n") as stream:
                stream.write(encoded(manifest))
            count = len(manifest["cores"])
        else:
            count = validate(args.core_dir, args.manifest)
        print(f"core manifest {args.command}: {count} cores")
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
