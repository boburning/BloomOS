#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export MINI_TOOL=/workspace/tools/bloom-mini-test
    export RUNNER=/workspace/static/build/.tmp_update/bin/bloom-test-runner
}

teardown() { teardown_bloom_fixture; }

@test "prepare supports every original Mini revision" {
    for revision in v1 v2 v3 v4; do
        rm -rf "$SDCARD/BloomTest"
        run "$MINI_TOOL" prepare "$SDCARD" "$revision"
        [ "$status" -eq 0 ]
        grep -F "\"expected_revision\": \"$revision\"" "$SDCARD/BloomTest/request.json"
        grep -F '"safe_only": true' "$SDCARD/BloomTest/request.json"
        [ -e "$SDCARD/.bloom-dev" ]
    done
}

@test "prepare refuses an unknown revision and a non-device directory" {
    run "$MINI_TOOL" prepare "$SDCARD" v5
    [ "$status" -eq 1 ]
    run "$MINI_TOOL" prepare "$BLOOM_TEST_ROOT" v2
    [ "$status" -eq 1 ]
}

@test "device runner refuses requests without developer mode or safe_only" {
    mkdir -p "$SDCARD/BloomTest"
    printf '%s\n' '{"safe_only": true}' >"$SDCARD/BloomTest/request.json"
    run env BLOOM_ROOT="$SDCARD" "$RUNNER"
    [ "$status" -eq 1 ]
    : >"$SDCARD/.bloom-dev"
    printf '%s\n' '{"safe_only": false}' >"$SDCARD/BloomTest/request.json"
    run env BLOOM_ROOT="$SDCARD" "$RUNNER"
    [ "$status" -eq 1 ]
}

@test "consume requires completed results and preserves the request" {
    "$MINI_TOOL" prepare "$SDCARD" v2
    run "$MINI_TOOL" consume "$SDCARD" "$BLOOM_TEST_ROOT/collected"
    [ "$status" -eq 1 ]
    printf '%s\n' '{"status":"collected"}' >"$SDCARD/BloomTest/results/test-results.json"
    run "$MINI_TOOL" consume "$SDCARD" "$BLOOM_TEST_ROOT/collected"
    [ "$status" -eq 0 ]
    [ -f "$BLOOM_TEST_ROOT/collected/request.json" ]
    [ -f "$BLOOM_TEST_ROOT/collected/results/test-results.json" ]
}
