#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

RESULTS = {"pass", "fail", "not_applicable", "pending"}


def validate(path: Path, policy_path: Path) -> list[str]:
    record = json.loads(path.read_text(encoding="utf-8"))
    policy = json.loads(policy_path.read_text(encoding="utf-8"))
    errors: list[str] = []
    if record.get("schema") != 1 or record.get("state") not in {"pending", "complete"}:
        errors.append("invalid evidence schema or state")
    matching = [entry for entry in policy.get("entries", []) if entry.get("core") == record.get("core")]
    if not matching or any(entry.get("binary_sha256") != record.get("core_sha256") for entry in matching):
        errors.append("evidence core SHA does not match policy")
    results = record.get("results")
    if not isinstance(results, dict) or not results or any(value not in RESULTS for value in results.values()):
        errors.append("invalid evidence result")
    if record.get("state") == "complete":
        for field in ("bloom_commit", "device", "operator", "tested_at"):
            if not record.get(field):
                errors.append(f"complete evidence lacks {field}")
        if not isinstance(record.get("fixtures"), list) or not record["fixtures"]:
            errors.append("complete evidence lacks fixtures")
        if isinstance(results, dict) and "pending" in results.values():
            errors.append("complete evidence contains pending results")
    serialized = json.dumps(record).lower()
    if re.search(r'"(password|token|rom_path|content_hash)"\s*:', serialized):
        errors.append("evidence contains a prohibited secret or ROM identity field")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("record", type=Path)
    parser.add_argument("--policy", type=Path, default=Path("build/ra-core-policy.json"))
    args = parser.parse_args()
    errors = validate(args.record, args.policy)
    if errors:
        print("\n".join(errors))
        return 1
    print("RetroAchievements evidence record is structurally valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
