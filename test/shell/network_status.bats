#!/usr/bin/env bats

setup() {
    export SERVICE=/workspace/static/build/.tmp_update/bin/bloom-network
    export BLOOM_NETWORK_ROOT="$BATS_TEST_TMPDIR/root"
    export BLOOM_PLATFORM_BIN="$BATS_TEST_TMPDIR/bloom-platform"
    export BLOOM_NETWORK_JSONVAL_BIN="$BATS_TEST_TMPDIR/jsonval"
    export BLOOM_NETWORK_LIB_DIR="$BATS_TEST_TMPDIR/runtime-lib"
    export BLOOM_NETWORK_BACKEND_BIN="$BATS_TEST_TMPDIR/bloom-wifi"
    export BLOOM_NETWORK_COMPAT_BIN="$BATS_TEST_TMPDIR/update-networking"
    export BLOOM_SETTINGS_BIN="$BATS_TEST_TMPDIR/bloom-settings"
    export BACKEND_LOG="$BATS_TEST_TMPDIR/backend.log"
    export COMPAT_LOG="$BATS_TEST_TMPDIR/compat.log"
    export SETTINGS_LOG="$BATS_TEST_TMPDIR/settings.log"
    mkdir -p "$BLOOM_NETWORK_ROOT/sys/class/net/wlan0"
    printf up >"$BLOOM_NETWORK_ROOT/sys/class/net/wlan0/operstate"
    printf 1 >"$BLOOM_NETWORK_ROOT/sys/class/net/wlan0/carrier"
    printf '#!/bin/sh\nprintf true\n' >"$BLOOM_PLATFORM_BIN"
    printf '#!/bin/sh\n[ "$1" = wifi ] || exit 2\nprintf 1\n' >"$BLOOM_NETWORK_JSONVAL_BIN"
    cat >"$BLOOM_NETWORK_BACKEND_BIN" <<'SH'
#!/bin/sh
printf '%s\n' "$*" >"$BACKEND_LOG"
SH
    cat >"$BLOOM_NETWORK_COMPAT_BIN" <<'SH'
#!/bin/sh
printf '%s\n' "$*" >"$COMPAT_LOG"
SH
    cat >"$BLOOM_SETTINGS_BIN" <<'SH'
#!/bin/sh
printf '%s\n' "$*" >"$SETTINGS_LOG"
SH
    chmod +x "$BLOOM_PLATFORM_BIN" "$BLOOM_NETWORK_JSONVAL_BIN" "$BLOOM_NETWORK_BACKEND_BIN" "$BLOOM_NETWORK_COMPAT_BIN" "$BLOOM_SETTINGS_BIN"
}

@test "status reports associated Wi-Fi without exposing network identity" {
    export JSONVAL_ENV_LOG="$BATS_TEST_TMPDIR/jsonval-env"
    cat >"$BLOOM_NETWORK_JSONVAL_BIN" <<'SH'
#!/bin/sh
printf '%s' "${LD_LIBRARY_PATH:-}" >"$JSONVAL_ENV_LOG"
printf 1
SH
    chmod +x "$BLOOM_NETWORK_JSONVAL_BIN"
    run "$SERVICE" status
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"service":"bloom-network","available":true,"wifi_capable":true,"wifi_enabled":true,"associated":true,"state":"associated"}' ]
    [[ "$output" != *'ssid'* ]]
    [[ "$output" != *'192.'* ]]
    [ "$(cat "$JSONVAL_ENV_LOG")" = "/lib:/config/lib:/customer/lib:$BLOOM_NETWORK_LIB_DIR" ]
}

@test "status distinguishes absent hardware and disabled Wi-Fi" {
    printf '#!/bin/sh\nprintf false\n' >"$BLOOM_PLATFORM_BIN"
    chmod +x "$BLOOM_PLATFORM_BIN"
    run "$SERVICE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"wifi_capable":false'* ]]
    [[ "$output" == *'"state":"no_network_hardware"'* ]]

    printf '#!/bin/sh\nprintf true\n' >"$BLOOM_PLATFORM_BIN"
    printf '#!/bin/sh\nprintf 0\n' >"$BLOOM_NETWORK_JSONVAL_BIN"
    chmod +x "$BLOOM_PLATFORM_BIN" "$BLOOM_NETWORK_JSONVAL_BIN"
    run "$SERVICE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"wifi_enabled":false'* ]]
    [[ "$output" == *'"state":"wifi_disabled"'* ]]
}

@test "status treats missing and down interfaces as not associated" {
    rm -rf "$BLOOM_NETWORK_ROOT/sys/class/net/wlan0"
    run "$SERVICE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"state":"not_associated"'* ]]

    mkdir "$BLOOM_NETWORK_ROOT/sys/class/net/wlan0"
    printf down >"$BLOOM_NETWORK_ROOT/sys/class/net/wlan0/operstate"
    printf 0 >"$BLOOM_NETWORK_ROOT/sys/class/net/wlan0/carrier"
    run "$SERVICE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"associated":false'* ]]
}

@test "status fails closed for unavailable dependencies and malformed calls" {
    printf '#!/bin/sh\nprintf maybe\n' >"$BLOOM_NETWORK_JSONVAL_BIN"
    chmod +x "$BLOOM_NETWORK_JSONVAL_BIN"
    run "$SERVICE" status
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"wifi_state_unavailable"'* ]]

    run "$SERVICE" status extra
    [ "$status" -eq 2 ]
    [[ "$output" == *'"state":"invalid_arguments"'* ]]
}

@test "reconcile delegates Wi-Fi and compatibility services through separate fixed actions" {
    run "$SERVICE" request reconcile
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"service":"bloom-network","operation":"reconcile","applied":true,"state":"applied"}' ]
    [ "$(cat "$BACKEND_LOG")" = reconcile ]
    [ "$(cat "$COMPAT_LOG")" = services ]

    run "$SERVICE" request restart
    [ "$status" -eq 2 ]
    [ "$(cat "$BACKEND_LOG")" = reconcile ]
}

@test "unsupported Bloom Wi-Fi backend falls back to the complete compatibility check" {
    printf '#!/bin/sh\nexit 3\n' >"$BLOOM_NETWORK_BACKEND_BIN"
    chmod +x "$BLOOM_NETWORK_BACKEND_BIN"
    run "$SERVICE" request reconcile
    [ "$status" -eq 0 ]
    [[ "$output" == *'"state":"compatibility_applied"'* ]]
    [ "$(cat "$COMPAT_LOG")" = check ]
}

@test "reconcile is a successful no-op without network hardware" {
    printf '#!/bin/sh\nprintf false\n' >"$BLOOM_PLATFORM_BIN"
    chmod +x "$BLOOM_PLATFORM_BIN"
    run "$SERVICE" request reconcile
    [ "$status" -eq 0 ]
    [[ "$output" == *'"applied":false'* ]]
    [[ "$output" == *'"state":"no_network_hardware"'* ]]
    [ ! -e "$BACKEND_LOG" ]
}

@test "runtime networking reaches the inherited backend only through Bloom" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh
    grep -F '$sysdir/bin/bloom-network request reconcile' "$runtime"
    ! grep -F '$sysdir/script/network/update_networking.sh check' "$runtime"
}

@test "enable and disable persist only the canonical Wi-Fi field before applying" {
    run "$SERVICE" request enable
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"service":"bloom-network","operation":"enable","preference_saved":true,"applied":true,"state":"applied"}' ]
    [ "$(cat "$SETTINGS_LOG")" = 'set device.wifi_enabled true' ]
    [ "$(cat "$BACKEND_LOG")" = reconcile ]
    [ "$(cat "$COMPAT_LOG")" = services ]

    run "$SERVICE" request disable
    [ "$status" -eq 0 ]
    [[ "$output" == *'"operation":"disable"'* ]]
    [ "$(cat "$SETTINGS_LOG")" = 'set device.wifi_enabled false' ]
    [ "$(cat "$BACKEND_LOG")" = reconcile ]
}

@test "preference mutation reports settings and apply failures separately" {
    printf '#!/bin/sh\nexit 1\n' >"$BLOOM_SETTINGS_BIN"
    chmod +x "$BLOOM_SETTINGS_BIN"
    run "$SERVICE" request enable
    [ "$status" -eq 1 ]
    [[ "$output" == *'"preference_saved":false,"applied":false,"state":"settings_rejected"'* ]]

    printf '#!/bin/sh\nexit 0\n' >"$BLOOM_SETTINGS_BIN"
    printf '#!/bin/sh\nexit 1\n' >"$BLOOM_NETWORK_BACKEND_BIN"
    chmod +x "$BLOOM_SETTINGS_BIN" "$BLOOM_NETWORK_BACKEND_BIN"
    run "$SERVICE" request disable
    [ "$status" -eq 1 ]
    [[ "$output" == *'"preference_saved":true,"applied":false,"state":"backend_failed"'* ]]
}

@test "preference mutation refuses hardware without networking" {
    printf '#!/bin/sh\nprintf false\n' >"$BLOOM_PLATFORM_BIN"
    chmod +x "$BLOOM_PLATFORM_BIN"
    run "$SERVICE" request enable
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"no_network_hardware"'* ]]
    [ ! -e "$SETTINGS_LOG" ]
}
