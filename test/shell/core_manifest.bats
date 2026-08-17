#!/usr/bin/env bats

setup() {
    CORE_DIR="/workspace/static/build/RetroArch/.retroarch/cores"
    MANIFEST="/workspace/build/core-manifest.json"
    TEST_ROOT="$(mktemp -d)"
}

teardown() { rm -rf "$TEST_ROOT"; }

@test "core manifest exactly matches every shipped libretro binary" {
    run python3 /workspace/tools/core_manifest.py validate \
        --core-dir "$CORE_DIR" --manifest "$MANIFEST"

    [ "$status" -eq 0 ]
    [[ "$output" == "core manifest validate: 110 cores" ]]
}

@test "core manifest rejects stale binary provenance" {
    cp "$MANIFEST" "$TEST_ROOT/core-manifest.json"
    sed -i 's/"sha256": "/"sha256": "0/' "$TEST_ROOT/core-manifest.json"

    run python3 /workspace/tools/core_manifest.py validate \
        --core-dir "$CORE_DIR" --manifest "$TEST_ROOT/core-manifest.json"

    [ "$status" -ne 0 ]
    [[ "$output" == *"stale or non-canonical"* ]]
}
