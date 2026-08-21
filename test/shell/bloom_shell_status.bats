#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export STATUS=/workspace/static/build/.tmp_update/bin/bloom-shell-status
    export BLOOM_SYSTEM_HEALTH_BIN="$BLOOM_TEST_ROOT/system-health"
    export BLOOM_UPDATE_STATE_BIN="$BLOOM_TEST_ROOT/update-state"
    export BLOOM_RA_BIN="$BLOOM_TEST_ROOT/ra-status"
    export BLOOM_LIBRARY_DIR="$BLOOM_TEST_ROOT/lib"
    mkdir -p "$BLOOM_LIBRARY_DIR"
    printf '%s\n' '#!/bin/sh' 'printf '\''%s\n'\'' '\''{"schema":1,"healthy":true}'\''' >"$BLOOM_SYSTEM_HEALTH_BIN"
    printf '%s\n' '#!/bin/sh' 'printf '\''%s\n'\'' '\''{"schema":1,"phase":"known_good"}'\''' >"$BLOOM_UPDATE_STATE_BIN"
    printf '%s\n' '#!/bin/sh' 'printf '\''%s\n'\'' '\''{"schema":1,"enabled":false,"state":"signed_out"}'\''' >"$BLOOM_RA_BIN"
    chmod +x "$BLOOM_SYSTEM_HEALTH_BIN" "$BLOOM_UPDATE_STATE_BIN" "$BLOOM_RA_BIN"
}

teardown() { teardown_bloom_fixture; }

@test "returns the bounded shell health subset" {
    run "$STATUS"
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"healthy":true'
    printf '%s' "$output" | grep -F '"phase":"known_good"'
    printf '%s' "$output" | grep -F '"enabled":false'
    printf '%s' "$output" | grep -F '"state":"signed_out"'
    [[ "$output" != *save_snapshots* ]]
    [[ "$output" != *play_activity* ]]
}

@test "fails closed without exposing malformed backend state" {
    printf '%s\n' '#!/bin/sh' 'printf '\''%s\n'\'' '\''{"schema":1,"phase":"testing-now"}'\''' >"$BLOOM_UPDATE_STATE_BIN"
    chmod +x "$BLOOM_UPDATE_STATE_BIN"
    run "$STATUS"
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"healthy":false'
    printf '%s' "$output" | grep -F '"phase":"invalid_state"'
}
