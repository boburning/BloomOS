#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export HEALTH=/workspace/static/build/.tmp_update/bin/bloom-health-system
    export BLOOM_ROOT="$BLOOM_TEST_ROOT"
    export BLOOM_SD_ROOT="$SDCARD"
    mkdir -p \
        "$BLOOM_ROOT/tmp" \
        "$SDCARD/miyoo/app" \
        "$SDCARD/RetroArch"
    printf '354\n' >"$BLOOM_ROOT/tmp/deviceModel"
    touch \
        "$SDCARD/.tmp_update/runtime.sh" \
        "$SDCARD/.tmp_update/bin/bloomctl" \
        "$SDCARD/.tmp_update/bin/bloom-platform" \
        "$SDCARD/.tmp_update/bin/bloom-shell" \
        "$SDCARD/.tmp_update/onionVersion/version.txt" \
        "$SDCARD/RetroArch/retroarch"
}

@test "requires Bloom Shell rather than the consumed MainUI installer trigger" {
    [ ! -e "$SDCARD/miyoo/app/MainUI" ]

    run "$HEALTH"
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"runtime":true'

    rm "$SDCARD/.tmp_update/bin/bloom-shell"
    run "$HEALTH"
    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"runtime":false'
}

teardown() { teardown_bloom_fixture; }

@test "reports known hardware complete runtime and writable capacity" {
    run "$HEALTH"

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"healthy":true'
    printf '%s' "$output" | grep -F '"model":"mini_plus"'
    printf '%s' "$output" | grep -F '"runtime":true'
    printf '%s' "$output" | grep -F '"storage_writable":true'
    [ ! -e "$SDCARD/.bloom/.health-probe-$$" ]
}

@test "fails closed for unknown hardware or missing runtime payload" {
    rm "$BLOOM_ROOT/tmp/deviceModel"
    run "$HEALTH"
    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"model":"unknown"'

    printf '354\n' >"$BLOOM_ROOT/tmp/deviceModel"
    rm "$SDCARD/RetroArch/retroarch"
    run "$HEALTH"
    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"runtime":false'
}

@test "fails closed below the configured free-space floor" {
    export BLOOM_HEALTH_MIN_FREE_KB=999999999999

    run "$HEALTH"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"capacity":false'
    printf '%s' "$output" | grep -F '"minimum_free_kb":999999999999'
}
