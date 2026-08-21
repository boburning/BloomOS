#!/usr/bin/env bats

setup() {
    export SERVICE=/workspace/static/build/.tmp_update/bin/bloom-network
    export BLOOM_NETWORK_ROOT="$BATS_TEST_TMPDIR/root"
    export BLOOM_PLATFORM_BIN="$BATS_TEST_TMPDIR/bloom-platform"
    export BLOOM_NETWORK_JSONVAL_BIN="$BATS_TEST_TMPDIR/jsonval"
    export BLOOM_NETWORK_LIB_DIR="$BATS_TEST_TMPDIR/runtime-lib"
    mkdir -p "$BLOOM_NETWORK_ROOT/sys/class/net/wlan0"
    printf up >"$BLOOM_NETWORK_ROOT/sys/class/net/wlan0/operstate"
    printf 1 >"$BLOOM_NETWORK_ROOT/sys/class/net/wlan0/carrier"
    printf '#!/bin/sh\nprintf true\n' >"$BLOOM_PLATFORM_BIN"
    printf '#!/bin/sh\n[ "$1" = wifi ] || exit 2\nprintf 1\n' >"$BLOOM_NETWORK_JSONVAL_BIN"
    chmod +x "$BLOOM_PLATFORM_BIN" "$BLOOM_NETWORK_JSONVAL_BIN"
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
