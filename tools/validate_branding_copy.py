#!/usr/bin/env python3
"""Reject accidental Onion product branding in Bloom-owned UI source."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


SCOPES = (
    "src/tweaks",
    "src/installUI",
    "src/prompt/test.sh",
    "static/configs/Saves/CurrentProfile/states/README.txt",
    "static/configs/Saves/CurrentProfile/saves/README.txt",
    "static/build/autorun.inf",
)
TEXT_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".inf", ".sh", ".txt"}
ONION = re.compile(r"onion", re.IGNORECASE)
LEGACY_CREDENTIALS = {
    '"Username: onion\\n"',
    '"Password: onion\\n"',
    '"Password: onion\\n");',
}
INTERNAL_IDENTIFIERS = ("onionVersion", "ONION_VERSION")


def validate(repository: pathlib.Path) -> tuple[list[str], int]:
    failures: list[str] = []
    classified = 0
    for scope in SCOPES:
        root = repository / scope
        if root.is_dir():
            paths = sorted(root.rglob("*"))
        elif root.is_file():
            paths = [root]
        else:
            failures.append(f"missing audit scope: {scope}")
            continue
        for path in paths:
            if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
                continue
            try:
                lines = path.read_text(encoding="utf-8").splitlines()
            except (OSError, UnicodeError) as error:
                failures.append(f"cannot read {path.relative_to(repository).as_posix()}: {error}")
                continue
            for line_number, line in enumerate(lines, 1):
                if not ONION.search(line):
                    continue
                stripped = line.strip()
                classified_line = stripped in LEGACY_CREDENTIALS or any(
                    identifier in line for identifier in INTERNAL_IDENTIFIERS
                )
                if classified_line:
                    classified += 1
                    continue
                relative = path.relative_to(repository).as_posix()
                failures.append(f"unclassified Onion copy: {relative}:{line_number}")
    return failures, classified


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", type=pathlib.Path, required=True)
    args = parser.parse_args()
    failures, classified = validate(args.repository.resolve())
    if failures:
        for failure in failures:
            print(f"branding copy: {failure}", file=sys.stderr)
        return 1
    print(f"branding copy validate: {classified} classified legacy literals")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
