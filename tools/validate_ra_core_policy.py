#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ALLOWED_CORE = {"verified", "best_effort", "incompatible", "untested", "not_applicable"}
ALLOWED_FEATURE = {"verified", "best_effort", "unsupported", "untested", "not_applicable"}
REQUIRED_SYSTEMS = {
    "gb", "gbc", "gba", "nes", "fds", "snes", "psx", "genesis", "segacd", "32x", "gamegear",
    "mastersystem", "sg1000", "arcade", "cps1", "cps2", "cps3", "neogeo", "atari2600", "atari7800",
    "lynx", "pce", "pcecd", "supergrafx", "virtualboy", "wonderswan", "ngpc", "coleco", "msx",
    "amstrad", "amiga",
}


def validate(policy_path: Path, manifest_path: Path) -> list[str]:
    policy = json.loads(policy_path.read_text(encoding="utf-8"))
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    errors: list[str] = []
    if policy.get("schema") != 1 or not isinstance(policy.get("entries"), list):
        return ["RA core policy must use schema 1 with an entries array"]
    cores = {core["binary"]: core["sha256"] for core in manifest.get("cores", [])}
    systems: set[str] = set()
    for entry in policy["entries"]:
        system = entry.get("system")
        core = entry.get("core")
        digest = entry.get("binary_sha256")
        if system in systems:
            errors.append(f"duplicate policy system: {system}")
        systems.add(system)
        if core not in cores:
            errors.append(f"unknown core for {system}: {core}")
        elif cores[core] != digest:
            errors.append(f"stale core SHA for {system}: {core}")
        if not isinstance(digest, str) or re.fullmatch(r"[0-9a-f]{64}", digest) is None:
            errors.append(f"invalid core SHA for {system}")
        if entry.get("bloom_ra_status") not in ALLOWED_CORE:
            errors.append(f"invalid bloom_ra_status for {system}")
        for field in ("hardcore_status", "leaderboards_status"):
            if entry.get(field) not in ALLOWED_FEATURE:
                errors.append(f"invalid {field} for {system}")
        if entry.get("bloom_ra_status") == "verified" and (
            not entry.get("tested_devices") or not entry.get("test_cases") or not entry.get("tested_at")
        ):
            errors.append(f"verified policy lacks physical evidence for {system}")
    missing = REQUIRED_SYSTEMS - systems
    extra = systems - REQUIRED_SYSTEMS
    if missing:
        errors.append("missing required default systems: " + ",".join(sorted(missing)))
    if extra:
        errors.append("unexpected policy systems: " + ",".join(sorted(extra)))
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--policy", default="build/ra-core-policy.json", type=Path)
    parser.add_argument("--core-manifest", default="build/core-manifest.json", type=Path)
    args = parser.parse_args()
    errors = validate(args.policy, args.core_manifest)
    if errors:
        for error in errors:
            print(error)
        return 1
    print("RA core policy matches exact shipped core identities")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
