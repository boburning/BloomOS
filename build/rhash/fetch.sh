#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOCK="${ROOT}/build/raofflineproxy/sources.json"
DEST="${1:-${ROOT}/.build/rhash/sources}"
mkdir -p "${DEST}"

python3 - "${LOCK}" "${DEST}" <<'PY'
from __future__ import annotations
import hashlib, json, pathlib, shutil, subprocess, sys

lock = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
destination = pathlib.Path(sys.argv[2])
required = {"raofflineproxy", "rcheevos", "libchdr"}
sources = {item["name"]: item for item in lock["sources"] if item["name"] in required}
if set(sources) != required:
    raise SystemExit("hash bridge source lock is incomplete")
for name in sorted(required):
    item = sources[name]
    target = destination / f"{name}.tar.gz"
    if target.exists() and target.stat().st_size == item["bytes"] and hashlib.sha256(target.read_bytes()).hexdigest() == item["sha256"]:
        continue
    partial = target.with_name(target.name + ".part")
    if shutil.which("curl"):
        command = ["curl", "--fail", "--location", "--retry", "3", "--output", str(partial), item["url"]]
    elif shutil.which("wget"):
        command = ["wget", "--tries=4", "--output-document", str(partial), item["url"]]
    else:
        raise SystemExit("neither curl nor wget is available for locked source fetch")
    subprocess.run(command, check=True)
    if partial.stat().st_size != item["bytes"] or hashlib.sha256(partial.read_bytes()).hexdigest() != item["sha256"]:
        if partial.exists():
            partial.unlink()
        raise SystemExit(f"{name}: downloaded source does not match the lock")
    partial.replace(target)
print("Bloom rhash sources: 3 verified inputs")
PY
