#!/usr/bin/env python3
"""Validate Bloom design tokens and original mark color usage."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys


HEX = re.compile(r"^#[0-9A-F]{6}$")


def luminance(color: str) -> float:
    values = [int(color[index : index + 2], 16) / 255 for index in (1, 3, 5)]
    linear = [value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4 for value in values]
    return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]


def contrast(first: str, second: str) -> float:
    high, low = sorted((luminance(first), luminance(second)), reverse=True)
    return (high + 0.05) / (low + 0.05)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tokens", type=pathlib.Path, required=True)
    parser.add_argument("--mark", type=pathlib.Path, required=True)
    args = parser.parse_args()
    failures: list[str] = []
    try:
        data = json.loads(args.tokens.read_text(encoding="utf-8"))
        mark = args.mark.read_text(encoding="utf-8")
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        print(f"Bloom design: unreadable input: {error}", file=sys.stderr)
        return 1
    palette = data.get("palette", {})
    if data.get("schema") != 1 or not isinstance(palette, dict):
        failures.append("schema-1 palette is required")
    for name, color in palette.items():
        if not isinstance(name, str) or not isinstance(color, str) or not HEX.fullmatch(color):
            failures.append(f"invalid palette entry: {name}")
    for role, token in data.get("roles", {}).items():
        if token not in palette:
            failures.append(f"role {role} references unknown token")
    for foreground, background, minimum in data.get("required_contrast", []):
        if foreground not in palette or background not in palette:
            failures.append("contrast pair references unknown token")
            continue
        ratio = contrast(palette[foreground], palette[background])
        if ratio < minimum:
            failures.append(f"contrast {foreground}/{background} is {ratio:.2f}, below {minimum}")
    mark_colors = set(re.findall(r"#[0-9A-Fa-f]{6}", mark))
    unknown = sorted(color.upper() for color in mark_colors if color.upper() not in set(palette.values()))
    if unknown:
        failures.append(f"mark uses colors outside the palette: {', '.join(unknown)}")
    if "<title" not in mark or "<desc" not in mark or mark.count("<path") < 9:
        failures.append("mark lacks accessible metadata or radial geometry")
    if failures:
        for failure in failures:
            print(f"Bloom design: {failure}", file=sys.stderr)
        return 1
    print(f"Bloom design validate: {len(palette)} tokens, {len(mark_colors)} mark colors")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
