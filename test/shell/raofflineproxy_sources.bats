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

@test "RAOfflineProxy package recipe rejects FAT-incompatible symlinks" {
    run grep -F 'find "${WORK_DIR}/package" -type l' /workspace/build/raofflineproxy/build.sh
    [ "$status" -eq 0 ]
    [[ "$output" == *'RAOfflineProxy package is not FAT-compatible'* || "$output" == *'find "${WORK_DIR}/package" -type l'* ]]
}

@test "RAOfflineProxy launcher uses its pinned runtime CA bundle" {
    run grep -F 'RAOFFLINEPROXY_CA_FILE="${CA_FILE}"' /workspace/build/raofflineproxy/build.sh
    [ "$status" -eq 0 ]
    run grep -F 'pip/_vendor/certifi/cacert.pem' /workspace/build/raofflineproxy/build.sh
    [ "$status" -eq 0 ]
}

@test "RAOfflineProxy native bridge removes build-directory identity" {
    run grep -F -- '-ffile-prefix-map="${WORK_DIR}"=/build/raofflineproxy' /workspace/build/raofflineproxy/build.sh
    [ "$status" -eq 0 ]
    run grep -F -- '-Wl,--build-id=none' /workspace/build/raofflineproxy/build.sh
    [ "$status" -eq 0 ]
}
