#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export BLOOM_ROOT="$BLOOM_TEST_ROOT"
    export PLATFORM=/workspace/static/build/.tmp_update/bin/bloom-platform
    mkdir -p "$BLOOM_ROOT/tmp" "$BLOOM_ROOT/sys/class/graphics/fb0" "$BLOOM_ROOT/sys/class/net" "$BLOOM_ROOT/dev/input"
}

teardown() { teardown_bloom_fixture; }

platform() { run sh "$PLATFORM" "$@"; }

@test "original Mini exposes only observed capabilities" {
    printf '283\n' >"$BLOOM_ROOT/tmp/deviceModel"
    printf '640,960\n' >"$BLOOM_ROOT/sys/class/graphics/fb0/virtual_size"
    touch "$BLOOM_ROOT/dev/input/event0" "$BLOOM_ROOT/dev/rtc0"

    platform inspect --json

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"model": "mini"'
    printf '%s' "$output" | grep -F '"width": "640", "height": "480"'
    printf '%s' "$output" | grep -F '"wifi": false, "ssh": false, "lid": false, "rtc": true, "input": true, "battery": false'
    printf '%s' "$output" | grep -F '"input_event_count": 1'
}

@test "Plus capabilities and triple buffering are normalized centrally" {
    printf '354\n' >"$BLOOM_ROOT/tmp/deviceModel"
    printf '640,1440\n' >"$BLOOM_ROOT/sys/class/graphics/fb0/virtual_size"
    mkdir -p "$BLOOM_ROOT/customer/app"
    touch "$BLOOM_ROOT/customer/app/axp_test"
    chmod +x "$BLOOM_ROOT/customer/app/axp_test"

    platform inspect --json

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"model": "mini_plus"'
    printf '%s' "$output" | grep -F '"height": "480", "virtual_size": "640,1440"'
    printf '%s' "$output" | grep -F '"wifi": true, "ssh": true, "lid": false'
    printf '%s' "$output" | grep -F '"battery": true'
    printf '%s' "$output" | grep -F '"battery_backend": "axp_live"'
}

@test "Flip uses the hall signal and reports lid capability" {
    hall="$BLOOM_ROOT/sys/devices/soc0/soc/soc:hall-mh248"
    mkdir -p "$hall"
    printf '1\n' >"$hall/hallvalue"
    printf '752,1680\n' >"$BLOOM_ROOT/sys/class/graphics/fb0/virtual_size"

    platform inspect --json

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"model": "mini_flip"'
    printf '%s' "$output" | grep -F '"width": "752", "height": "560"'
    printf '%s' "$output" | grep -F '"lid": true'
}

@test "Flip current firmware hall path is also normalized centrally" {
    hall="$BLOOM_ROOT/sys/devices/platform/hall-mh248"
    mkdir -p "$hall"
    printf '1\n' >"$hall/hallvalue"

    platform capability lid
    [ "$status" -eq 0 ]
    [ "$output" = true ]

    platform model
    [ "$status" -eq 0 ]
    [ "$output" = mini_flip ]
}

@test "unknown capabilities and extra arguments fail closed" {
    platform capability rumble
    [ "$status" -eq 2 ]

    platform model extra
    [ "$status" -eq 2 ]
}

@test "malformed framebuffer geometry is not exposed as structured data" {
    printf '640,480\"oops\n' >"$BLOOM_ROOT/sys/class/graphics/fb0/virtual_size"

    platform inspect --json

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"width": "unknown", "height": "unknown", "virtual_size": "unknown"'
    ! printf '%s' "$output" | grep -Fq 'oops'
}
