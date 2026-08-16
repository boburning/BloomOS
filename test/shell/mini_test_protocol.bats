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
    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$RUNNER"
    [ "$status" -eq 1 ]
    : >"$SDCARD/.bloom-dev"
    printf '%s\n' '{"safe_only": false}' >"$SDCARD/BloomTest/request.json"
    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$RUNNER"
    [ "$status" -eq 1 ]
}

@test "device runner collects a safe request once" {
    mkdir -p \
        "$BLOOM_TEST_ROOT/sys/class/graphics/fb0" \
        "$BLOOM_TEST_ROOT/sys/class/power_supply/battery" \
        "$BLOOM_TEST_ROOT/dev/input" \
        "$SDCARD/BloomTest/results"
    : >"$SDCARD/.bloom-dev"
    printf '%s\n' '{"safe_only": true}' >"$SDCARD/BloomTest/request.json"
    printf '%s\n' '640,480' >"$BLOOM_TEST_ROOT/sys/class/graphics/fb0/virtual_size"
    printf '%s\n' '75' >"$BLOOM_TEST_ROOT/sys/class/power_supply/battery/capacity"
    : >"$BLOOM_TEST_ROOT/dev/input/event0"
    cp /workspace/static/build/.tmp_update/bin/bloomctl "$SDCARD/.tmp_update/bin/bloomctl"

    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$RUNNER"

    [ "$status" -eq 0 ]
    grep -F '"status": "collected"' "$SDCARD/BloomTest/results/test-results.json"
    grep -F 'virtual_size=640,480' "$SDCARD/BloomTest/results/display.txt"
    grep -F 'battery_capacity=75' "$SDCARD/BloomTest/results/battery.txt"

    first_hash="$(sha256sum "$SDCARD/BloomTest/results/test-results.json")"
    env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$RUNNER"
    [ "$first_hash" = "$(sha256sum "$SDCARD/BloomTest/results/test-results.json")" ]
}

@test "runtime only invokes the runner behind both opt-in files" {
    run grep -F 'if [ -f /mnt/SDCARD/.bloom-dev ] && [ -f /mnt/SDCARD/BloomTest/request.json ]' \
        /workspace/static/build/.tmp_update/runtime.sh
    [ "$status" -eq 0 ]
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
