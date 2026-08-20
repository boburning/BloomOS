#!/usr/bin/env python3
"""Fail-closed host gate for RetroAchievements release-sensitive contracts."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from validate_ra_core_policy import validate as validate_core_policy
from validate_raofflineproxy_sources import load as load_proxy_sources


REQUIRED_TESTS = {
    "test/test_bloom_launch.cpp",
    "test/test_bloom_ra.cpp",
    "test/test_bloom_ra_account.cpp",
    "test/test_bloom_ra_catalog.cpp",
    "test/test_bloom_ra_database.cpp",
    "test/test_bloom_ra_scanner.cpp",
    "test/shell/ra_cli.bats",
    "test/shell/ra_core_policy.bats",
    "test/shell/ra_proxy_adapter.bats",
    "test/shell/raofflineproxy_sources.bats",
    "test/shell/ra_certification_tool.bats",
}


def validate(root: Path) -> list[str]:
    errors: list[str] = []
    errors.extend(validate_core_policy(root / "build/ra-core-policy.json", root / "build/core-manifest.json"))
    try:
        source_lock = load_proxy_sources(root / "build/raofflineproxy/sources.json")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        errors.append(f"invalid RAOfflineProxy source lock: {error}")
        source_lock = {"sources": []}

    dependencies = (root / "build/dependencies.lock").read_text(encoding="utf-8")
    for item in source_lock["sources"]:
        if item["revision"] not in dependencies and item["name"] in {"raofflineproxy", "python-runtime"}:
            errors.append(f"dependency lock omits {item['name']} revision")

    bridge_files = (
        "build/rhash/build.sh",
        "build/rhash/fetch.sh",
        "build/rhash/raofflineproxy-console-hash.patch",
    )
    for path in bridge_files:
        if not (root / path).is_file():
            errors.append(f"missing Bloom disc hash bridge input: {path}")
    makefile = (root / "Makefile").read_text(encoding="utf-8")
    if "build/rhash/build.sh" not in makefile or "libbloom-rchash.so" not in makefile:
        errors.append("core build omits the Bloom disc hash bridge")
    for revision in ("2ad0b8672f68a48148620164510b963039e49eb1", "6cde5348eb118da3baf94f75a69577a005a484fd",
                     "be6898e6dc26338bf82421ff7602fd37932be449"):
        if revision not in dependencies:
            errors.append(f"dependency lock omits disc hash bridge revision {revision}")

    missing_tests = sorted(path for path in REQUIRED_TESTS if not (root / path).is_file())
    if missing_tests:
        errors.append("missing RA regression tests: " + ",".join(missing_tests))

    adapter = (root / "src/bloomRaProxy/bloom-ra-proxy").read_text(encoding="utf-8")
    for forbidden in ('run_upstream start-proxy', 'run_upstream stop-proxy', 'retroarch.cfg'):
        if forbidden in adapter:
            errors.append(f"proxy adapter contains forbidden permanent-config integration: {forbidden}")

    launch = (root / "src/bloomLaunch/bloom_launch.c").read_text(encoding="utf-8")
    for invariant in ("offline casual proxy is unavailable", "achievement session policy is immutable",
                      'strcmp(mode, "hardcore") == 0', "cheevos_custom_host"):
        if invariant not in launch:
            errors.append(f"launch policy invariant missing: {invariant}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", type=Path, default=Path("."))
    args = parser.parse_args()
    errors = validate(args.repository.resolve())
    if errors:
        for error in errors:
            print(f"error: {error}")
        return 1
    print("RetroAchievements release gate: ready")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
