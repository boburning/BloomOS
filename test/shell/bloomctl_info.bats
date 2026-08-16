#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export BLOOM_ROOT="$BLOOM_TEST_ROOT"
    mkdir -p \
        "$BLOOM_ROOT/tmp" \
        "$BLOOM_ROOT/sys/class/graphics/fb0" \
        "$BLOOM_ROOT/sys/class/net"
    printf 'v0.0.0-test\n' >"$SDCARD/.tmp_update/onionVersion/version.txt"
}

teardown() {
    teardown_bloom_fixture
}

run_info() {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl info --json
}

@test "reports an original Mini without guessing its hardware revision" {
    printf '283\n' >"$BLOOM_ROOT/tmp/deviceModel"
    printf '640,480\n' >"$BLOOM_ROOT/sys/class/graphics/fb0/virtual_size"

    run_info

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"model": "mini"'
    printf '%s' "$output" | grep -F '"hardware_revision": "unknown"'
    printf '%s' "$output" | grep -F '"width": "640"'
}

@test "reports Plus capabilities from observable paths" {
    printf '354\n' >"$BLOOM_ROOT/tmp/deviceModel"
    printf '640,1440\n' >"$BLOOM_ROOT/sys/class/graphics/fb0/virtual_size"

    run_info

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"model": "mini_plus"'
    printf '%s' "$output" | grep -F '"wifi": true'
    printf '%s' "$output" | grep -F '"lid": false'
    printf '%s' "$output" | grep -F '"height": "480"'
}

@test "detects Flip from the hall sensor when legacy model data is absent" {
    hall="$BLOOM_ROOT/sys/devices/soc0/soc/soc:hall-mh248"
    mkdir -p "$hall"
    printf '1\n' >"$hall/hallvalue"
    touch "$SDCARD/.bloom-dev"

    run_info

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"model": "mini_flip"'
    printf '%s' "$output" | grep -F '"lid": true'
    printf '%s' "$output" | grep -F '"developer_mode": true'
}

@test "rejects unknown commands without mutating the fixture" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update rollback

    [ "$status" -eq 2 ]
    [ ! -e "$SDCARD/.tmp_update/bloom-update-state" ]
}
