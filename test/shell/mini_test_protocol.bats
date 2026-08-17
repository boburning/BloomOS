#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export MINI_TOOL=/workspace/tools/bloom-mini-test
    export RUNNER=/workspace/static/build/.tmp_update/bin/bloom-test-runner
    cp /workspace/static/build/.tmp_update/bin/bloom-platform "$SDCARD/.tmp_update/bin/bloom-platform"
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

    [ "$status" -eq 0 ] || { printf '%s\n' "$output"; false; }
    grep -F '"status": "collected"' "$SDCARD/BloomTest/results/test-results.json"
    grep -F 'physical_size=640,480' "$SDCARD/BloomTest/results/display.txt"
    grep -F 'battery_source=sysfs' "$SDCARD/BloomTest/results/battery.txt"
    grep -F 'battery_capacity=75' "$SDCARD/BloomTest/results/battery.txt"

    first_hash="$(sha256sum "$SDCARD/BloomTest/results/test-results.json")"
    mkdir -p "$BLOOM_TEST_ROOT/appconfigs"
    printf '%s\n' 'remount_ro_final=0' >"$BLOOM_TEST_ROOT/appconfigs/bloom-shutdown.log"
    env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$RUNNER"
    [ "$first_hash" = "$(sha256sum "$SDCARD/BloomTest/results/test-results.json")" ]
    grep -F 'remount_ro_final=0' "$SDCARD/BloomTest/results/previous-shutdown.txt"
}

@test "device runner carries forward developer shutdown telemetry" {
    mkdir -p \
        "$BLOOM_TEST_ROOT/appconfigs" \
        "$SDCARD/.tmp_update/bin" \
        "$SDCARD/BloomTest/results"
    : >"$SDCARD/.bloom-dev"
    printf '%s\n' '{"safe_only": true}' >"$SDCARD/BloomTest/request.json"
    printf '%s\n' 'remount_ro_final=0' >"$BLOOM_TEST_ROOT/appconfigs/bloom-shutdown.log"
    cp /workspace/static/build/.tmp_update/bin/bloomctl "$SDCARD/.tmp_update/bin/bloomctl"

    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$RUNNER"

    [ "$status" -eq 0 ] || { printf '%s\n' "$output"; false; }
    grep -F 'remount_ro_final=0' "$SDCARD/BloomTest/results/previous-shutdown.txt"
}

@test "device runner normalizes a triple-buffered display and reads Plus battery data" {
    mkdir -p \
        "$BLOOM_TEST_ROOT/sys/class/graphics/fb0" \
        "$BLOOM_TEST_ROOT/customer/app" \
        "$SDCARD/BloomTest/results"
    : >"$SDCARD/.bloom-dev"
    printf '%s\n' '{"safe_only": true}' >"$SDCARD/BloomTest/request.json"
    printf '%s\n' '640,1440' >"$BLOOM_TEST_ROOT/sys/class/graphics/fb0/virtual_size"
    cat >"$BLOOM_TEST_ROOT/customer/app/axp_test" <<'EOF'
#!/bin/sh
printf '%s\n' '{"battery":72, "voltage":3910, "charging":0}'
EOF
    chmod +x "$BLOOM_TEST_ROOT/customer/app/axp_test"
    cp /workspace/static/build/.tmp_update/bin/bloomctl "$SDCARD/.tmp_update/bin/bloomctl"

    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$RUNNER"

    [ "$status" -eq 0 ] || { printf '%s\n' "$output"; false; }
    grep -F 'physical_size=640,480' "$SDCARD/BloomTest/results/display.txt"
    grep -F 'framebuffer_virtual_size=640,1440' "$SDCARD/BloomTest/results/display.txt"
    grep -F 'battery_source=axp_live' "$SDCARD/BloomTest/results/battery.txt"
    grep -F 'battery_capacity=72' "$SDCARD/BloomTest/results/battery.txt"
}

@test "device runner records read-only Flip capability signals" {
    hall="$BLOOM_TEST_ROOT/sys/devices/soc0/soc/soc:hall-mh248"
    mkdir -p \
        "$hall" \
        "$BLOOM_TEST_ROOT/sys/class/graphics/fb0" \
        "$BLOOM_TEST_ROOT/customer/app" \
        "$BLOOM_TEST_ROOT/dev/input" \
        "$BLOOM_TEST_ROOT/tmp" \
        "$SDCARD/BloomTest/results"
    : >"$SDCARD/.bloom-dev"
    : >"$BLOOM_TEST_ROOT/customer/app/axp_test"
    : >"$BLOOM_TEST_ROOT/dev/input/event0"
    : >"$BLOOM_TEST_ROOT/dev/input/event1"
    printf '285\n' >"$BLOOM_TEST_ROOT/tmp/deviceModel"
    printf '1\n' >"$hall/hallvalue"
    printf '752,1120\n' >"$BLOOM_TEST_ROOT/sys/class/graphics/fb0/virtual_size"
    printf '%s\n' '{"safe_only": true}' >"$SDCARD/BloomTest/request.json"
    cp /workspace/static/build/.tmp_update/bin/bloomctl "$SDCARD/.tmp_update/bin/bloomctl"

    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$RUNNER"

    [ "$status" -eq 0 ] || { printf '%s\n' "$output"; false; }
    grep -F '"model": "mini_flip"' "$SDCARD/BloomTest/results/device.json"
    grep -F '"height": "560"' "$SDCARD/BloomTest/results/device.json"
    grep -F 'hall_sensor=present' "$SDCARD/BloomTest/results/platform.txt"
    grep -F 'lid_state_raw=1' "$SDCARD/BloomTest/results/platform.txt"
    grep -F 'input_event_count=2' "$SDCARD/BloomTest/results/platform.txt"
}

@test "device runner falls back to original Mini batmon output" {
    mkdir -p \
        "$BLOOM_TEST_ROOT/sys/class/graphics/fb0" \
        "$BLOOM_TEST_ROOT/tmp" \
        "$SDCARD/BloomTest/results"
    : >"$SDCARD/.bloom-dev"
    printf '%s\n' '{"safe_only": true}' >"$SDCARD/BloomTest/request.json"
    printf '%s\n' '640,1440' >"$BLOOM_TEST_ROOT/sys/class/graphics/fb0/virtual_size"
    printf '%s\n' '63' >"$BLOOM_TEST_ROOT/tmp/percBat"
    cp /workspace/static/build/.tmp_update/bin/bloomctl "$SDCARD/.tmp_update/bin/bloomctl"

    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" "$RUNNER"

    [ "$status" -eq 0 ] || { printf '%s\n' "$output"; false; }
    grep -F 'battery_source=batmon' "$SDCARD/BloomTest/results/battery.txt"
    grep -F 'battery_capacity=63' "$SDCARD/BloomTest/results/battery.txt"
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
