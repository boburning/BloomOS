#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOCK="${ROOT}/build/raofflineproxy/sources.json"
DEST="${1:-${ROOT}/.build/raofflineproxy/sources}"
mkdir -p "${DEST}"

python3 - "${LOCK}" "${DEST}" <<'PY'
from __future__ import annotations
import json, pathlib, subprocess, sys
lock = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
dest = pathlib.Path(sys.argv[2])
for item in lock["sources"]:
    target = dest / (item["name"] + ".tar.gz")
    partial = target.with_suffix(target.suffix + ".part")
    subprocess.run(["curl", "--fail", "--location", "--retry", "3", "--output", str(partial), item["url"]], check=True)
    partial.replace(target)
PY

python3 "${ROOT}/tools/validate_raofflineproxy_sources.py" "${LOCK}" --source-dir "${DEST}"
