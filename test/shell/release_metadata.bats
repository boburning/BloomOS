#!/usr/bin/env bats

setup() {
    TEST_ROOT="$(mktemp -d)"
    RELEASE_DIR="$TEST_ROOT/release/1.2.3"
    mkdir -p "$RELEASE_DIR" "$TEST_ROOT/payload/miyoo/app/.tmp_update" "$TEST_ROOT/payload/RetroArch"
    printf 'core\n' >"$TEST_ROOT/payload/miyoo/app/.tmp_update/onion.pak"
    printf 'retroarch\n' >"$TEST_ROOT/payload/RetroArch/retroarch.pak"
    printf 'locked\n' >"$TEST_ROOT/dependencies.lock"
    python3 -c 'import pathlib, zipfile; root=pathlib.Path(__import__("sys").argv[1]); archive=pathlib.Path(__import__("sys").argv[2]); z=zipfile.ZipFile(archive, "w"); [z.write(p, p.relative_to(root).as_posix()) for p in sorted(root.rglob("*")) if p.is_file()]; z.close()' \
        "$TEST_ROOT/payload" "$RELEASE_DIR/BloomOS-v1.2.3.zip"
}

teardown() { rm -rf "$TEST_ROOT"; }

@test "release metadata is deterministic and validates its archive" {
    run python3 /workspace/tools/release_metadata.py create \
        --output-dir "$RELEASE_DIR" \
        --archive "$RELEASE_DIR/BloomOS-v1.2.3.zip" \
        --version 1.2.3 \
        --channel beta \
        --commit 0123456789012345678901234567890123456789 \
        --source-date-epoch 1700000000 \
        --dependency-lock "$TEST_ROOT/dependencies.lock" \
        --toolchain example.invalid/toolchain@sha256:abc
    [ "$status" -eq 0 ]

    run python3 /workspace/tools/release_metadata.py validate --output-dir "$RELEASE_DIR"
    [ "$status" -eq 0 ]
    grep -F '"build_date": "2023-11-14T22:13:20Z"' "$RELEASE_DIR/build-info.json"
    grep -F '"product": "BloomOS"' "$RELEASE_DIR/manifest.json"
    grep -F 'BloomOS-v1.2.3.zip' "$RELEASE_DIR/SHA256SUMS"
}

@test "release validation rejects a modified archive" {
    python3 /workspace/tools/release_metadata.py create \
        --output-dir "$RELEASE_DIR" \
        --archive "$RELEASE_DIR/BloomOS-v1.2.3.zip" \
        --version 1.2.3 \
        --channel stable \
        --commit 0123456789012345678901234567890123456789 \
        --source-date-epoch 1700000000 \
        --dependency-lock "$TEST_ROOT/dependencies.lock" \
        --toolchain example.invalid/toolchain@sha256:abc
    printf 'tampered\n' >>"$RELEASE_DIR/BloomOS-v1.2.3.zip"

    run python3 /workspace/tools/release_metadata.py validate --output-dir "$RELEASE_DIR"
    [ "$status" -ne 0 ]
    [[ "$output" == *"digest or size does not match"* ]]
}
