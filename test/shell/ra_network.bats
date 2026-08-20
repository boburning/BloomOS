#!/usr/bin/env bats

setup() {
    export PROBE=/workspace/static/build/.tmp_update/bin/bloom-ra-network
    export BLOOM_RA_NETWORK_ROOT="$BATS_TEST_TMPDIR/root"
    export BLOOM_PLATFORM_BIN="$BATS_TEST_TMPDIR/bloom-platform"
    export BLOOM_CURL_BIN="$BATS_TEST_TMPDIR/curl"
    export BLOOM_DATE_BIN="$BATS_TEST_TMPDIR/date"
    export BLOOM_RA_NETWORK_CA_FILE="$BATS_TEST_TMPDIR/cacert.pem"
    : >"$BLOOM_RA_NETWORK_CA_FILE"
    mkdir -p "$BLOOM_RA_NETWORK_ROOT/sys/class/net/wlan0"
    printf up >"$BLOOM_RA_NETWORK_ROOT/sys/class/net/wlan0/operstate"
    printf 1 >"$BLOOM_RA_NETWORK_ROOT/sys/class/net/wlan0/carrier"
    cat >"$BLOOM_PLATFORM_BIN" <<'SH'
#!/bin/sh
printf '%s\n' true
SH
    cat >"$BLOOM_DATE_BIN" <<'SH'
#!/bin/sh
printf '%s\n' 1800000000
SH
    cat >"$BLOOM_CURL_BIN" <<'SH'
#!/bin/sh
printf '%s' 200
SH
    chmod +x "$BLOOM_PLATFORM_BIN" "$BLOOM_DATE_BIN" "$BLOOM_CURL_BIN"
}

@test "readiness probe reports a bounded ready state" {
    export BLOOM_RA_NETWORK_LIB_DIR="$BATS_TEST_TMPDIR/runtime-lib"
    export CURL_ENV_LOG="$BATS_TEST_TMPDIR/curl-env"
    cat >"$BLOOM_CURL_BIN" <<'SH'
#!/bin/sh
printf '%s\n%s' "${LD_LIBRARY_PATH:-}" "${CURL_CA_BUNDLE:-}" >"$CURL_ENV_LOG"
printf '%s' 200
SH
    chmod +x "$BLOOM_CURL_BIN"

    run "$PROBE" status
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"service":"bloom-ra-network","online":true,"state":"ready"}' ]
    expected_path="/lib:/config/lib:$BLOOM_RA_NETWORK_LIB_DIR:/customer/lib:$BLOOM_RA_NETWORK_LIB_DIR/parasyte"
    [ "$(sed -n '1p' "$CURL_ENV_LOG")" = "$expected_path" ]
    [ "$(sed -n '2p' "$CURL_ENV_LOG")" = "$BLOOM_RA_NETWORK_CA_FILE" ]
}

@test "readiness probe fails closed when no trusted CA bundle is available" {
    export BLOOM_RA_NETWORK_CA_FILE="$BATS_TEST_TMPDIR/missing-ca"
    run "$PROBE" status
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"tls_unavailable"'* ]]
}

@test "readiness probe distinguishes hardware wifi association and clock" {
    printf '#!/bin/sh\nprintf false\n' >"$BLOOM_PLATFORM_BIN"
    chmod +x "$BLOOM_PLATFORM_BIN"
    run "$PROBE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"state":"no_network_hardware"'* ]]

    printf '#!/bin/sh\nprintf true\n' >"$BLOOM_PLATFORM_BIN"
    chmod +x "$BLOOM_PLATFORM_BIN"
    rm -rf "$BLOOM_RA_NETWORK_ROOT/sys/class/net/wlan0"
    run "$PROBE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"state":"wifi_disabled"'* ]]

    mkdir -p "$BLOOM_RA_NETWORK_ROOT/sys/class/net/wlan0"
    printf down >"$BLOOM_RA_NETWORK_ROOT/sys/class/net/wlan0/operstate"
    printf 0 >"$BLOOM_RA_NETWORK_ROOT/sys/class/net/wlan0/carrier"
    run "$PROBE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"state":"not_associated"'* ]]

    printf up >"$BLOOM_RA_NETWORK_ROOT/sys/class/net/wlan0/operstate"
    printf 1 >"$BLOOM_RA_NETWORK_ROOT/sys/class/net/wlan0/carrier"
    printf '#!/bin/sh\nprintf 100\n' >"$BLOOM_DATE_BIN"
    chmod +x "$BLOOM_DATE_BIN"
    run "$PROBE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"state":"clock_invalid"'* ]]
}

@test "readiness probe maps bounded transport failures without response data" {
    for pair in '6 dns_failure' '7 network_unreachable' '28 timeout' '60 tls_failure' '22 ra_service_failure'; do
        code=${pair%% *}
        expected=${pair#* }
        printf '#!/bin/sh\nexit %s\n' "$code" >"$BLOOM_CURL_BIN"
        chmod +x "$BLOOM_CURL_BIN"
        run "$PROBE" status
        [ "$status" -eq 1 ]
        [[ "$output" == *"\"state\":\"$expected\""* ]]
        [[ "$output" != *'retroachievements.org'* ]]
    done
}

@test "readiness probe rejects malformed calls and service responses" {
    run "$PROBE" status extra
    [ "$status" -eq 2 ]
    [[ "$output" == *'"state":"invalid_arguments"'* ]]

    printf '#!/bin/sh\nprintf 500\n' >"$BLOOM_CURL_BIN"
    chmod +x "$BLOOM_CURL_BIN"
    run "$PROBE" status
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"ra_service_failure"'* ]]
}
