#!/usr/bin/env python3
"""Validate the implementation-derived MainUI responsibility inventory."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys


CLASSIFICATIONS = {
    "replace",
    "import-then-replace",
    "compatibility-adapter",
    "firmware-adapter",
}
REQUIRED_FIELDS = {
    "id",
    "summary",
    "classification",
    "current_paths",
    "target_owner",
    "stable_gate",
}


def validate(repository: pathlib.Path, manifest: pathlib.Path) -> list[str]:
    failures: list[str] = []
    try:
        data = json.loads(manifest.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        return [f"manifest is unreadable: {error}"]

    if data.get("schema") != 1:
        failures.append("schema must be 1")
    responsibilities = data.get("responsibilities")
    if not isinstance(responsibilities, list) or not responsibilities:
        return failures + ["responsibilities must be a non-empty list"]

    seen: set[str] = set()
    for index, item in enumerate(responsibilities):
        label = f"responsibilities[{index}]"
        if not isinstance(item, dict):
            failures.append(f"{label} must be an object")
            continue
        missing = sorted(REQUIRED_FIELDS - item.keys())
        if missing:
            failures.append(f"{label} missing fields: {', '.join(missing)}")
            continue
        item_id = item["id"]
        if not isinstance(item_id, str) or not item_id or item_id in seen:
            failures.append(f"{label} id must be a unique non-empty string")
        else:
            seen.add(item_id)
        if item["classification"] not in CLASSIFICATIONS:
            failures.append(f"{label} has unknown classification")
        for field in ("summary", "target_owner", "stable_gate"):
            if not isinstance(item[field], str) or not item[field].strip():
                failures.append(f"{label}.{field} must be a non-empty string")
        paths = item["current_paths"]
        if not isinstance(paths, list) or not paths:
            failures.append(f"{label}.current_paths must be a non-empty list")
            continue
        for raw_path in paths:
            if not isinstance(raw_path, str) or not raw_path or "\\" in raw_path:
                failures.append(f"{label} contains an invalid repository path")
                continue
            path = pathlib.PurePosixPath(raw_path)
            if path.is_absolute() or ".." in path.parts or any(char in raw_path for char in "*?["):
                failures.append(f"{label} contains an unsafe repository path: {raw_path}")
                continue
            if not (repository / pathlib.Path(*path.parts)).exists():
                failures.append(f"{label} references a missing path: {raw_path}")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", type=pathlib.Path, required=True)
    parser.add_argument("--manifest", type=pathlib.Path, required=True)
    args = parser.parse_args()
    failures = validate(args.repository.resolve(), args.manifest.resolve())
    if failures:
        for failure in failures:
            print(f"mainui inventory: {failure}", file=sys.stderr)
        return 1
    count = len(json.loads(args.manifest.read_text(encoding="utf-8"))["responsibilities"])
    print(f"mainui inventory validate: {count} responsibilities")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
