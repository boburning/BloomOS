#!/usr/bin/env python3
"""Validate the immutable RAOfflineProxy package input lock."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from urllib.parse import urlparse


REQUIRED = {"raofflineproxy", "rcheevos", "libchdr", "python-runtime"}


def load(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema") != 1:
        raise ValueError("unsupported source-lock schema")
    sources = data.get("sources")
    if not isinstance(sources, list):
        raise ValueError("sources must be an array")
    names = {item.get("name") for item in sources if isinstance(item, dict)}
    if names != REQUIRED:
        raise ValueError("source-lock component set is incomplete")
    for item in sources:
        if urlparse(item.get("url", "")).scheme != "https":
            raise ValueError(f"{item.get('name')}: source URL must use HTTPS")
        digest = item.get("sha256", "")
        if len(digest) != 64 or any(ch not in "0123456789abcdef" for ch in digest):
            raise ValueError(f"{item['name']}: invalid SHA-256")
        if not isinstance(item.get("bytes"), int) or item["bytes"] <= 0:
            raise ValueError(f"{item['name']}: invalid byte count")
        if not item.get("revision") or not item.get("license"):
            raise ValueError(f"{item['name']}: incomplete provenance")
    return data


def verify(data: dict, source_dir: Path) -> None:
    for item in data["sources"]:
        path = source_dir / f"{item['name']}.tar.gz"
        if path.stat().st_size != item["bytes"]:
            raise ValueError(f"{item['name']}: byte count mismatch")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != item["sha256"]:
            raise ValueError(f"{item['name']}: SHA-256 mismatch")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("lock", type=Path)
    parser.add_argument("--source-dir", type=Path)
    args = parser.parse_args()
    data = load(args.lock)
    if args.source_dir:
        verify(data, args.source_dir)
    print(f"RAOfflineProxy source lock: {len(data['sources'])} inputs")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}")
        raise SystemExit(1)
