#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export LOGGER=/workspace/static/build/.tmp_update/bin/bloom-ra-log
    export BLOOM_RA_LOG_DIR="$BLOOM_TEST_ROOT/logs"
    export BLOOM_RA_LOG_MAX_BYTES=1024
    export SHA=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
}

teardown() { teardown_bloom_fixture; }

@test "logger writes only allowlisted launch and finish fields" {
    "$LOGGER" launch softcore direct gpsp_libretro.so "$SHA" best_effort
    "$LOGGER" finish stopped 60

    log="$BLOOM_RA_LOG_DIR/retroachievements.log"
    [ "$(stat -c %a "$log")" = 600 ]
    [ "$(wc -l <"$log")" -eq 2 ]
    grep -F '"core":"gpsp_libretro.so"' "$log"
    grep -F '"detail":"60"' "$log"
    ! grep -E 'token|username|rom|game_id|path' "$log"
}

@test "logger rejects free-form data and unsafe paths" {
    run "$LOGGER" launch softcore direct 'private/path_libretro.so' "$SHA" best_effort
    [ "$status" -eq 1 ]
    run "$LOGGER" finish failed 'private reason'
    [ "$status" -eq 1 ]

    mkdir -p "$BLOOM_TEST_ROOT/outside"
    ln -s "$BLOOM_TEST_ROOT/outside" "$BLOOM_RA_LOG_DIR"
    run "$LOGGER" finish failed launch_failed
    [ "$status" -eq 1 ]
}

@test "logger retains one bounded rotated generation" {
    mkdir -p "$BLOOM_RA_LOG_DIR"
    dd if=/dev/zero of="$BLOOM_RA_LOG_DIR/retroachievements.log" bs=1024 count=1 2>/dev/null
    "$LOGGER" finish failed launch_failed

    [ "$(stat -c %s "$BLOOM_RA_LOG_DIR/retroachievements.log.1")" -eq 1024 ]
    [ "$(wc -l <"$BLOOM_RA_LOG_DIR/retroachievements.log")" -eq 1 ]
}
