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
    printf '354\n' >"$BLOOM_ROOT/tmp/deviceModel"
    mkdir -p "$SDCARD/miyoo/app" "$SDCARD/RetroArch"
    touch \
        "$SDCARD/.tmp_update/runtime.sh" \
        "$SDCARD/.tmp_update/bin/bloomctl" \
        "$SDCARD/.tmp_update/bin/bloom-platform" \
        "$SDCARD/RetroArch/retroarch" \
        "$SDCARD/miyoo/app/MainUI"
    default_update_state="$BLOOM_TEST_ROOT/default-update-state"
    cat >"$default_update_state" <<'EOF'
#!/bin/sh
[ "$1" = status ] || exit 2
printf '%s\n' '{"schema":1,"phase":"idle"}'
EOF
    chmod +x "$default_update_state"
    export BLOOM_UPDATE_STATE_BIN="$default_update_state"
    default_snapshot="$BLOOM_TEST_ROOT/default-snapshot"
    cat >"$default_snapshot" <<'EOF'
#!/bin/sh
[ "$1" = health ] || exit 2
printf '%s\n' '{"schema":1,"healthy":true,"total":0,"unverified":0,"referenced":0}'
EOF
    chmod +x "$default_snapshot"
    export BLOOM_SAVE_SNAPSHOT_BIN="$default_snapshot"
    default_ra="$BLOOM_TEST_ROOT/default-ra"
    cat >"$default_ra" <<'EOF'
#!/bin/sh
[ "$1" = status ] || exit 2
printf '%s\n' '{"schema":1,"service":"bloom-ra","enabled":false,"state":"not_configured","catalog":{"status":"unavailable"},"indexed_games":0,"identified_games":0}'
EOF
    chmod +x "$default_ra"
    export BLOOM_RA_BIN="$default_ra"
    default_proxy="$BLOOM_TEST_ROOT/default-ra-proxy"
    cat >"$default_proxy" <<'EOF'
#!/bin/sh
[ "$1" = status ] || exit 2
printf '%s\n' '{"schema":1,"service":"bloom-ra-proxy","installed":false,"running":false,"state":"not_installed"}'
EOF
    chmod +x "$default_proxy"
    export BLOOM_RA_PROXY_BIN="$default_proxy"
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
    rm -f "$BLOOM_ROOT/tmp/deviceModel"
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
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update unknown

    [ "$status" -eq 2 ]
    [ ! -e "$SDCARD/.tmp_update/bloom-update-state" ]
}

@test "platform capabilities exposes the centralized structured inspection" {
    printf '640,1440\n' >"$BLOOM_ROOT/sys/class/graphics/fb0/virtual_size"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl platform capabilities

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"schema": 1'
    printf '%s' "$output" | grep -F '"model": "mini_plus"'
    printf '%s' "$output" | grep -F '"capabilities": {"wifi": true, "ssh": true'
    printf '%s' "$output" | grep -F '"height": "480"'

    run sh /workspace/static/build/.tmp_update/bin/bloomctl platform model
    [ "$status" -eq 2 ]
}

@test "test smoke preserves the ROM path as encoded data for the guarded runner" {
    smoke="$BLOOM_TEST_ROOT/bloom-game-smoke"
    cat >"$smoke" <<'EOF'
#!/bin/sh
printf '%s\n%s\n%s\n' "$1" "$2" "$3" >"$BLOOM_TEST_ROOT/smoke-args"
printf '%s\n' '{"schema":1,"status":"passed"}'
EOF
    chmod +x "$smoke"
    export BLOOM_GAME_SMOKE_BIN="$smoke"
    rom="$SDCARD/Roms/GB/Test [Rev 1] (World).zip"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl test smoke GB "$rom" 12

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"status":"passed"'
    [ "$(sed -n '1p' "$BLOOM_TEST_ROOT/smoke-args")" = GB ]
    encoded="$(sed -n '2p' "$BLOOM_TEST_ROOT/smoke-args")"
    [ "$(printf '%s' "$encoded" | base64 -d)" = "$rom" ]
    [ "$(sed -n '3p' "$BLOOM_TEST_ROOT/smoke-args")" = 12 ]

    run sh /workspace/static/build/.tmp_update/bin/bloomctl test smoke GB "$rom"
    [ "$status" -eq 0 ]
    [ "$(sed -n '3p' "$BLOOM_TEST_ROOT/smoke-args")" = 10 ]
}

@test "test CLI rejects unsupported operations and newline paths before delegation" {
    smoke="$BLOOM_TEST_ROOT/bloom-game-smoke"
    cat >"$smoke" <<'EOF'
#!/bin/sh
touch "$BLOOM_TEST_ROOT/smoke-called"
EOF
    chmod +x "$smoke"
    export BLOOM_GAME_SMOKE_BIN="$smoke"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl test destructive
    [ "$status" -eq 2 ]

    run sh /workspace/static/build/.tmp_update/bin/bloomctl test smoke GB "bad
path.zip"
    [ "$status" -eq 2 ]
    [ ! -e "$BLOOM_TEST_ROOT/smoke-called" ]
}

@test "test achievements delegates only the explicit guarded argument shape" {
    helper="$BLOOM_TEST_ROOT/ra-test"
    cat >"$helper" <<'EOF'
#!/bin/sh
printf '%s\n' "$*"
EOF
    chmod +x "$helper"
    export BLOOM_RA_TEST_BIN="$helper"
    run sh /workspace/static/build/.tmp_update/bin/bloomctl test achievements --system GBA --rom-base64 ZGF0YQ== --core gpsp
    [ "$status" -eq 0 ]
    [ "$output" = '--system GBA --rom-base64 ZGF0YQ== --core gpsp' ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl test achievements --system GBA --core gpsp
    [ "$status" -eq 2 ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl test achievements --system GBA --rom-base64 ZGF0YQ== --core gpsp --session-seconds 10
    [ "$status" -eq 0 ]
    [ "$output" = '--system GBA --rom-base64 ZGF0YQ== --core gpsp --session-seconds 10' ]
}

@test "update commands delegate only the explicit offline operations" {
    mock_stage="$BLOOM_TEST_ROOT/mock-stage"
    mock_prepare="$BLOOM_TEST_ROOT/mock-prepare"
    mock_bootstrap="$BLOOM_TEST_ROOT/mock-bootstrap"
    mock_activate="$BLOOM_TEST_ROOT/mock-activate"
    mock_state="$BLOOM_TEST_ROOT/mock-state"
    mock_boot="$BLOOM_TEST_ROOT/mock-boot"
    mock_rollback="$BLOOM_TEST_ROOT/mock-rollback"
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
    cat >"$mock_bootstrap" <<'EOF'
#!/bin/sh
printf 'bootstrap:%s\n' "$1"
EOF
    cat >"$mock_activate" <<'EOF'
#!/bin/sh
printf 'activate:%s\n' "$1"
EOF
    cat >"$mock_boot" <<'EOF'
#!/bin/sh
printf 'boot:%s\n' "$1"
EOF
    cat >"$mock_rollback" <<'EOF'
#!/bin/sh
printf 'rollback\n'
EOF
    chmod +x "$mock_stage" "$mock_prepare" "$mock_bootstrap" "$mock_activate" "$mock_state" "$mock_boot" "$mock_rollback"
    export BLOOM_UPDATE_STAGE_BIN="$mock_stage"
    export BLOOM_UPDATE_PREPARE_BIN="$mock_prepare"
    export BLOOM_UPDATE_BOOTSTRAP_BIN="$mock_bootstrap"
    export BLOOM_UPDATE_ACTIVATE_BIN="$mock_activate"
    export BLOOM_UPDATE_STATE_BIN="$mock_state"
    export BLOOM_UPDATE_BOOT_BIN="$mock_boot"
    export BLOOM_UPDATE_ROLLBACK_BIN="$mock_rollback"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl update status
    [ "$status" -eq 0 ]
    [ "$output" = 'state:status:' ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update stage /media/manifest.json /media/manifest.sig /media/BloomOS.zip
    [ "$status" -eq 0 ]
    [ "$output" = 'stage:/media/manifest.json:/media/manifest.sig:/media/BloomOS.zip' ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update prepare 1.2.3
    [ "$status" -eq 0 ]
    [ "$output" = 'prepare:1.2.3' ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update bootstrap 1.2.3
    [ "$status" -eq 0 ]
    [ "$output" = 'bootstrap:1.2.3' ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update arm 1.2.3
    [ "$status" -eq 0 ]
    [ "$output" = 'state:arm:1.2.3' ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update activate 1.2.3
    [ "$status" -eq 0 ]
    [ "$output" = 'activate:1.2.3' ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update confirm
    [ "$status" -eq 0 ]
    [ "$output" = 'boot:confirm' ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update rollback
    [ "$status" -eq 0 ]
    [ "$output" = 'rollback' ]
}

@test "update CLI does not expose raw state mutation" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl update mark-good 1.2.3
    [ "$status" -eq 2 ]
}

@test "save snapshot inventory delegates only the read-only list operation" {
    mock_snapshot="$BLOOM_TEST_ROOT/mock-snapshot"
    cat >"$mock_snapshot" <<'EOF'
#!/bin/sh
printf 'snapshot:%s\n' "$1"
EOF
    chmod +x "$mock_snapshot"
    export BLOOM_SAVE_SNAPSHOT_BIN="$mock_snapshot"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl saves snapshots
    [ "$status" -eq 0 ]
    [ "$output" = 'snapshot:list' ]
    run sh /workspace/static/build/.tmp_update/bin/bloomctl saves restore anything
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
    printf '%s' "$output" | grep -F '"system": {"schema":1,"healthy":true'
    printf '%s' "$output" | grep -F '"play_activity": {"schema":1,"healthy":true,"quick_check":"ok"}'
    printf '%s' "$output" | grep -F '"update_state": {"schema":1,"healthy":true,"phase":"idle"}'
    printf '%s' "$output" | grep -F '"save_snapshots": {"schema":1,"healthy":true,"total":0,"unverified":0,"referenced":0}'
    printf '%s' "$output" | grep -F '"retroachievements": {"schema":1,"healthy":true,"enabled":false'
}

@test "health allowlists RetroAchievements fields and redacts secrets and game data" {
    cat >"$BLOOM_RA_BIN" <<'EOF'
#!/bin/sh
printf '%s\n' '{"schema":1,"enabled":true,"state":"ready","username":"private-user","token":"secret-token","catalog":{"status":"ready"},"indexed_games":12,"identified_games":5,"rom_path":"/private/game.zip"}'
EOF
    cat >"$BLOOM_RA_PROXY_BIN" <<'EOF'
#!/bin/sh
printf '%s\n' '{"schema":1,"installed":true,"upstream":{"cached_games_count":4,"pending_awards_count":2,"service_running":true,"award":"private achievement"}}'
EOF
    chmod +x "$BLOOM_RA_BIN" "$BLOOM_RA_PROXY_BIN"
    cat >"$SDCARD/.tmp_update/bin/playActivity" <<'EOF'
#!/bin/sh
printf '%s\n' '{"schema":1,"healthy":true}'
EOF
    chmod +x "$SDCARD/.tmp_update/bin/playActivity"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl health --json
    [ "$status" -eq 0 ]
    [[ "$output" == *'"cached_games":4,"pending_awards":2'* ]]
    [[ "$output" != *'private-user'* ]]
    [[ "$output" != *'secret-token'* ]]
    [[ "$output" != *'/private/game.zip'* ]]
    [[ "$output" != *'private achievement'* ]]
}

@test "health fails when snapshot inventory contains unverified evidence" {
    cat >"$SDCARD/.tmp_update/bin/playActivity" <<'EOF'
#!/bin/sh
printf '%s\n' '{"schema":1,"healthy":true,"quick_check":"ok"}'
EOF
    snapshot="$BLOOM_TEST_ROOT/unverified-snapshot"
    cat >"$snapshot" <<'EOF'
#!/bin/sh
printf '%s\n' '{"schema":1,"healthy":false,"total":2,"unverified":1,"referenced":0}'
exit 1
EOF
    chmod +x "$SDCARD/.tmp_update/bin/playActivity" "$snapshot"
    export BLOOM_SAVE_SNAPSHOT_BIN="$snapshot"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl health --json

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"save_snapshots": {"schema":1,"healthy":false,"total":2,"unverified":1,"referenced":0}'
}

@test "health fails closed for recovery-required update state" {
    cat >"$SDCARD/.tmp_update/bin/playActivity" <<'EOF'
#!/bin/sh
printf '%s\n' '{"schema":1,"healthy":true,"quick_check":"ok"}'
EOF
    update_state="$BLOOM_TEST_ROOT/recovery-update-state"
    cat >"$update_state" <<'EOF'
#!/bin/sh
printf '%s\n' '{"schema":1,"phase":"recovery_required","pending_version":"2.0.0"}'
EOF
    chmod +x "$SDCARD/.tmp_update/bin/playActivity" "$update_state"
    export BLOOM_UPDATE_STATE_BIN="$update_state"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl health --json

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"update_state": {"schema":1,"healthy":false,"phase":"recovery_required","error":"recovery_required"}'
    ! printf '%s' "$output" | grep -F 'pending_version'
}

@test "health replaces malformed update output with a bounded error" {
    update_state="$BLOOM_TEST_ROOT/malformed-update-state"
    cat >"$update_state" <<'EOF'
#!/bin/sh
printf '%s\n' 'not json and potentially sensitive'
EOF
    chmod +x "$update_state"
    export BLOOM_UPDATE_STATE_BIN="$update_state"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl health --json

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"update_state": {"schema":1,"healthy":false,"error":"invalid_state"}'
    ! printf '%s' "$output" | grep -F 'potentially sensitive'
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

    run sh /workspace/static/build/.tmp_update/bin/bloomctl health --json

    [ "$status" -eq 1 ]
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
