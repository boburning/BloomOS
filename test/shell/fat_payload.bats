#!/usr/bin/env bats

@test "accepts a regular FAT-compatible payload" {
    payload="$BATS_TEST_TMPDIR/payload"
    mkdir -p "$payload/.tmp_update/bin"
    printf 'runtime\n' >"$payload/.tmp_update/bin/bloomctl"

    run python3 /workspace/tools/validate_fat_payload.py "$payload"

    [ "$status" -eq 0 ]
    [ "$output" = "FAT payload validation passed" ]
}

@test "rejects symlinks and a leaked PixelReader workspace" {
    payload="$BATS_TEST_TMPDIR/payload"
    mkdir -p "$payload/pixelreader/work/lib"
    printf 'library\n' >"$payload/pixelreader/work/lib/libxml2.so.2"
    ln -s libxml2.so.2 "$payload/pixelreader/work/lib/libxml2.so"

    run python3 /workspace/tools/validate_fat_payload.py "$payload"

    [ "$status" -eq 1 ]
    [[ "$output" == *"temporary PixelReader workspace: pixelreader/work"* ]]
    [[ "$output" == *"symlink: pixelreader/work/lib/libxml2.so"* ]]
}
