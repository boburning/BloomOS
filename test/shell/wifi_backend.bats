#!/usr/bin/env bats

setup() {
    export SERVICE=/workspace/static/build/.tmp_update/bin/bloom-wifi
    export BLOOM_WIFI_ROOT="$BATS_TEST_TMPDIR/root"
    export BLOOM_PLATFORM_BIN="$BATS_TEST_TMPDIR/bloom-platform"
    export BLOOM_WIFI_JSONVAL_BIN="$BATS_TEST_TMPDIR/jsonval"
    export BLOOM_WIFI_RADIO_BIN="$BATS_TEST_TMPDIR/axp_test"
    export BLOOM_WIFI_IFCONFIG_BIN="$BATS_TEST_TMPDIR/ifconfig"
    export BLOOM_WIFI_WPA_BIN="$BATS_TEST_TMPDIR/wpa_supplicant"
    export BLOOM_WIFI_CONFIG="$BATS_TEST_TMPDIR/wpa_supplicant.conf"
    export BLOOM_WIFI_UDHCPC_BIN="$BATS_TEST_TMPDIR/udhcpc"
    export BLOOM_WIFI_UDHCPC_SCRIPT="$BATS_TEST_TMPDIR/udhcpc.script"
    export BLOOM_WIFI_IW_BIN="$BATS_TEST_TMPDIR/iw"
    export BLOOM_WIFI_PKILL_BIN="$BATS_TEST_TMPDIR/pkill"
    export BLOOM_WIFI_SLEEP_BIN="$BATS_TEST_TMPDIR/sleep"
    export WIFI_LOG="$BATS_TEST_TMPDIR/wifi.log"
    mkdir -p "$BLOOM_WIFI_ROOT/sys/class/net/wlan0"
    printf down >"$BLOOM_WIFI_ROOT/sys/class/net/wlan0/operstate"
    printf 0 >"$BLOOM_WIFI_ROOT/sys/class/net/wlan0/carrier"
    printf '#!/bin/sh\nprintf mini_plus\n' >"$BLOOM_PLATFORM_BIN"
    printf '#!/bin/sh\nprintf 1\n' >"$BLOOM_WIFI_JSONVAL_BIN"
    : >"$BLOOM_WIFI_CONFIG"
    : >"$BLOOM_WIFI_UDHCPC_SCRIPT"
    for name in axp_test ifconfig wpa_supplicant udhcpc iw pkill sleep; do
        cat >"$BATS_TEST_TMPDIR/$name" <<'SH'
#!/bin/sh
printf '%s %s\n' "$(basename "$0")" "$*" >>"$WIFI_LOG"
SH
        chmod +x "$BATS_TEST_TMPDIR/$name"
    done
    chmod +x "$BLOOM_PLATFORM_BIN" "$BLOOM_WIFI_JSONVAL_BIN"
}

@test "associated Plus is preserved without invoking lifecycle commands" {
    printf up >"$BLOOM_WIFI_ROOT/sys/class/net/wlan0/operstate"
    printf 1 >"$BLOOM_WIFI_ROOT/sys/class/net/wlan0/carrier"
    run "$SERVICE" reconcile
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"service":"bloom-wifi","supported":true,"applied":false,"state":"preserved"}' ]
    [ ! -s "$WIFI_LOG" ]
}

@test "enabled disconnected Plus starts the fixed lifecycle" {
    run "$SERVICE" reconcile
    [ "$status" -eq 0 ]
    [[ "$output" == *'"state":"started"'* ]]
    grep -Fx 'axp_test wifion' "$WIFI_LOG"
    grep -Fx 'ifconfig wlan0 up' "$WIFI_LOG"
    grep -Fx "wpa_supplicant -B -D nl80211 -iwlan0 -c $BLOOM_WIFI_CONFIG" "$WIFI_LOG"
    grep -Fx "udhcpc -i wlan0 -s $BLOOM_WIFI_UDHCPC_SCRIPT" "$WIFI_LOG"
    grep -Fx 'iw dev wlan0 set power_save off' "$WIFI_LOG"
}

@test "disabled Plus stops clients and powers off the radio" {
    printf '#!/bin/sh\nprintf 0\n' >"$BLOOM_WIFI_JSONVAL_BIN"
    chmod +x "$BLOOM_WIFI_JSONVAL_BIN"
    run "$SERVICE" reconcile
    [ "$status" -eq 0 ]
    [[ "$output" == *'"state":"stopped"'* ]]
    grep -Fx 'pkill -TERM wpa_supplicant' "$WIFI_LOG"
    grep -Fx 'pkill -TERM udhcpc' "$WIFI_LOG"
    grep -Fx 'axp_test wifioff' "$WIFI_LOG"
}

@test "unsupported model requests compatibility fallback without mutations" {
    printf '#!/bin/sh\nprintf mini_flip\n' >"$BLOOM_PLATFORM_BIN"
    chmod +x "$BLOOM_PLATFORM_BIN"
    run "$SERVICE" reconcile
    [ "$status" -eq 3 ]
    [[ "$output" == *'"state":"unsupported_model"'* ]]
    [ ! -s "$WIFI_LOG" ]
}

@test "malformed preference and missing dependencies fail closed" {
    printf '#!/bin/sh\nprintf maybe\n' >"$BLOOM_WIFI_JSONVAL_BIN"
    chmod +x "$BLOOM_WIFI_JSONVAL_BIN"
    run "$SERVICE" reconcile
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"preference_unavailable"'* ]]

    printf '#!/bin/sh\nprintf 1\n' >"$BLOOM_WIFI_JSONVAL_BIN"
    chmod +x "$BLOOM_WIFI_JSONVAL_BIN"
    rm "$BLOOM_WIFI_IW_BIN"
    run "$SERVICE" reconcile
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"start_failed"'* ]]
}

@test "invalid arguments fail without side effects" {
    run "$SERVICE" status
    [ "$status" -eq 2 ]
    [[ "$output" == *'"state":"invalid_arguments"'* ]]
    [ ! -s "$WIFI_LOG" ]
}
