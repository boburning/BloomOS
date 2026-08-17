#!/usr/bin/env bats

load 'support/test_helper'
bats_require_minimum_version 1.5.0

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
    printf '%s' "$output" | grep -F '"ssh": true'
    printf '%s' "$output" | grep -F '"lid": false'
    printf '%s' "$output" | grep -F '"rtc": false'
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
    printf '%s' "$output" | grep -F '"developer_ssh": "disabled"'
}

@test "rejects unknown commands without mutating the fixture" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update rollback

    [ "$status" -eq 2 ]
    [ ! -e "$SDCARD/.tmp_update/bloom-update-state" ]
}

@test "update commands delegate only the explicit offline operations" {
    mock_stage="$BLOOM_TEST_ROOT/mock-stage"
    mock_prepare="$BLOOM_TEST_ROOT/mock-prepare"
    mock_state="$BLOOM_TEST_ROOT/mock-state"
    cat >"$mock_stage" <<'EOF'
#!/bin/sh
printf 'stage:%s:%s:%s\n' "$1" "$2" "$3"
EOF
    cat >"$mock_state" <<'EOF'
#!/bin/sh
printf 'state:%s:%s\n' "$1" "${2:-}"
EOF
    cat >"$mock_prepare" <<'EOF'
#!/bin/sh
printf 'prepare:%s\n' "$1"
EOF
    chmod +x "$mock_stage" "$mock_prepare" "$mock_state"
    export BLOOM_UPDATE_STAGE_BIN="$mock_stage"
    export BLOOM_UPDATE_PREPARE_BIN="$mock_prepare"
    export BLOOM_UPDATE_STATE_BIN="$mock_state"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl update status
    [ "$status" -eq 0 ]
    [ "$output" = 'state:status:' ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update stage /media/manifest.json /media/manifest.sig /media/BloomOS.zip
    [ "$status" -eq 0 ]
    [ "$output" = 'stage:/media/manifest.json:/media/manifest.sig:/media/BloomOS.zip' ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update prepare 1.2.3
    [ "$status" -eq 0 ]
    [ "$output" = 'prepare:1.2.3' ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update arm 1.2.3
    [ "$status" -eq 0 ]
    [ "$output" = 'state:arm:1.2.3' ]
}

@test "update CLI does not expose activation or health promotion" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update activate 1.2.3
    [ "$status" -eq 2 ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update mark-good 1.2.3
    [ "$status" -eq 2 ]
}

@test "health exposes the Play Activity diagnostic as structured JSON" {
    cat >"$SDCARD/.tmp_update/bin/playActivity" <<'EOF'
#!/bin/sh
[ "$1" = health ] || exit 2
printf '%s\n' '{"schema":1,"healthy":true,"quick_check":"ok"}'
EOF
    chmod +x "$SDCARD/.tmp_update/bin/playActivity"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl health --json

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"healthy": true'
    printf '%s' "$output" | grep -F '"play_activity": {"schema":1,"healthy":true,"quick_check":"ok"}'
}

@test "health fails closed when Play Activity diagnostics are unavailable" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl health --json

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"healthy": false'
    printf '%s' "$output" | grep -F '"error":"unavailable"'
}

@test "health keeps valid JSON when Play Activity cannot start" {
    cat >"$SDCARD/.tmp_update/bin/playActivity" <<'EOF'
#!/bin/sh
exit 127
EOF
    chmod +x "$SDCARD/.tmp_update/bin/playActivity"

    run -127 sh /workspace/static/build/.tmp_update/bin/bloomctl health --json

    [ "$status" -eq 127 ]
    printf '%s' "$output" | grep -F '"healthy": false'
    printf '%s' "$output" | grep -F '"error":"execution_failed"'
}

@test "health does not embed non-JSON output from an incompatible binary" {
    cat >"$SDCARD/.tmp_update/bin/playActivity" <<'EOF'
#!/bin/sh
printf '%s\n' 'Error: Invalid argument health'
exit 1
EOF
    chmod +x "$SDCARD/.tmp_update/bin/playActivity"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl health --json

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"error":"execution_failed"'
    ! printf '%s' "$output" | grep -F 'Invalid argument'
}
