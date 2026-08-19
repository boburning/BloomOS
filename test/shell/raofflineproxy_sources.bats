#!/usr/bin/env bats

setup() {
    LOCK=/workspace/build/raofflineproxy/sources.json
    VALIDATOR=/workspace/tools/validate_raofflineproxy_sources.py
}

@test "RAOfflineProxy source lock is complete" {
    run python3 "$VALIDATOR" "$LOCK"
    [ "$status" -eq 0 ]
    [ "$output" = "RAOfflineProxy source lock: 4 inputs" ]
}

@test "RAOfflineProxy source lock rejects mutable URLs" {
    repository="$BATS_TEST_TMPDIR/repository"
    mkdir -p "$repository"
    cp "$LOCK" "$repository/sources.json"
    python3 - "$repository/sources.json" <<'PY'
import json, pathlib, sys
p = pathlib.Path(sys.argv[1])
d = json.loads(p.read_text())
d["sources"][0]["url"] = "http://example.invalid/latest.tar.gz"
p.write_text(json.dumps(d))
PY
    run python3 "$VALIDATOR" "$repository/sources.json"
    [ "$status" -eq 1 ]
    [[ "$output" == *"source URL must use HTTPS"* ]]
}

@test "RAOfflineProxy artifact verification fails closed" {
    source_dir="$BATS_TEST_TMPDIR/sources"
    mkdir -p "$source_dir"
    for name in raofflineproxy rcheevos libchdr python-runtime; do
        printf corrupt > "$source_dir/$name.tar.gz"
    done
    run python3 "$VALIDATOR" "$LOCK" --source-dir "$source_dir"
    [ "$status" -eq 1 ]
    [[ "$output" == *"byte count mismatch"* ]]
}
