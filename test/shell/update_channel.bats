#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export CHANNEL=/workspace/static/build/.tmp_update/bin/bloom-update-channel
    export BLOOM_SD_ROOT="$SDCARD"
    export BLOOM_UPDATE_ROOT="$BLOOM_TEST_ROOT/update"
}

teardown() { teardown_bloom_fixture; }

@test "channel defaults to stable without creating update state" {
    run "$CHANNEL" get
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"channel":"stable"'
    printf '%s' "$output" | grep -F '"source":"default"'
    [ ! -e "$BLOOM_UPDATE_ROOT" ]
}

@test "channel selection is atomic and restricted to release channels" {
    run "$CHANNEL" set nightly
    [ "$status" -eq 0 ]
    [ "$(cat "$BLOOM_UPDATE_ROOT/channel")" = nightly ]
    [ -z "$(find "$BLOOM_UPDATE_ROOT" -name '*.tmp-*' -print)" ]
    run "$CHANNEL" get --raw
    [ "$status" -eq 0 ]
    [ "$output" = nightly ]
    run "$CHANNEL" set development
    [ "$status" -eq 1 ]
    [ "$(cat "$BLOOM_UPDATE_ROOT/channel")" = nightly ]
}

@test "channel refuses symlinked state" {
    mkdir -p "$BLOOM_TEST_ROOT/elsewhere" "$BLOOM_UPDATE_ROOT"
    printf 'stable\n' >"$BLOOM_TEST_ROOT/elsewhere/channel"
    ln -s "$BLOOM_TEST_ROOT/elsewhere/channel" "$BLOOM_UPDATE_ROOT/channel"
    run "$CHANNEL" set beta
    [ "$status" -eq 1 ]
    [ "$(cat "$BLOOM_TEST_ROOT/elsewhere/channel")" = stable ]
}

@test "bloomctl exposes channel inspection and selection only" {
    export BLOOM_UPDATE_CHANNEL_BIN="$CHANNEL"
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update channel beta
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"channel":"beta"'

    run sh /workspace/static/build/.tmp_update/bin/bloomctl update channel
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"channel":"beta"'

    run sh /workspace/static/build/.tmp_update/bin/bloomctl update channel development
    [ "$status" -eq 1 ]
}
