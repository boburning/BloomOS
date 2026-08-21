#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export DETECTOR=/workspace/static/build/.tmp_update/bin/bloom-detect-model
}

teardown() { teardown_bloom_fixture; }

@test "detects every family from strong hardware signals" {
    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$DETECTOR"
    [ "$status" -eq 0 ]
    [ "$output" = 283 ]

    mock_command axp
    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$DETECTOR"
    [ "$output" = 354 ]

    hall="$BLOOM_TEST_ROOT/sys/devices/soc0/soc/soc:hall-mh248"
    mkdir -p "$hall"
    printf '1\n' >"$hall/hallvalue"
    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$DETECTOR"
    [ "$output" = 285 ]
}

@test "Flip uses explicit identity with the proven Plus compatibility assets" {
    hall="$BLOOM_TEST_ROOT/sys/devices/soc0/soc/soc:hall-mh248"
    mkdir -p "$hall"
    printf '1\n' >"$hall/hallvalue"

    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$DETECTOR" compatibility-id

    [ "$status" -eq 0 ]
    [ "$output" = 354 ]
    grep -F 'COMPAT_DEVICE_ID="$(bloom-detect-model compatibility-id)"' /workspace/static/build/.tmp_update/runtime.sh
    grep -F 'COMPAT_DEVICE_ID="$(bloom-detect-model compatibility-id)"' /workspace/static/dist/miyoo/app/.tmp_update/install.sh
}

@test "detects Flip from the current platform hall path" {
    hall="$BLOOM_TEST_ROOT/sys/devices/platform/hall-mh248"
    mkdir -p "$hall"
    printf '1\n' >"$hall/hallvalue"

    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$DETECTOR"
    [ "$status" -eq 0 ]
    [ "$output" = 285 ]
}
