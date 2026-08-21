#!/usr/bin/env bats

@test "signed system catalog exactly covers packaged Onion emulator identities" {
    run python3 - /workspace <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
catalog = json.loads((root / "build/system-catalog.json").read_text(encoding="utf-8"))
assert catalog == {"schema": 1, "entries": catalog["entries"]}
entries = catalog["entries"]
assert entries
folders = [entry["folder"] for entry in entries]
ids = [entry["system_id"] for entry in entries]
assert len(folders) == len(set(folders))
assert len(ids) == len(set(ids))
assert folders == sorted(folders)

configs = list((root / "static/packages/Emu").glob("*/Emu/*/config.json"))
packaged = {path.parent.name: path for path in configs}
assert len(packaged) == len(configs)
assert set(folders) == set(packaged)

for entry in entries:
    assert set(entry) <= {"folder", "rom_folder", "system_id"}
    config = json.loads(packaged[entry["folder"]].read_text(encoding="utf-8"))
    prefix = "../../Roms/"
    assert config["rompath"].startswith(prefix)
    actual = config["rompath"][len(prefix):].split("/", 1)[0]
    assert actual == entry.get("rom_folder", entry["folder"])
PY
    [ "$status" -eq 0 ]
}
