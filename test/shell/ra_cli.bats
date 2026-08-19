#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    ra="$BLOOM_TEST_ROOT/bloom-ra"
    cat >"$ra" <<'EOF'
#!/bin/sh
printf '%s\n%s\n%s\n%s\n' "$1" "${2:-}" "${3:-}" "${LD_LIBRARY_PATH:-}" >"$BLOOM_TEST_ROOT/ra-args"
case "$1" in
    status) printf '%s\n' '{"schema":1,"service":"bloom-ra","enabled":false,"state":"not_configured"}' ;;
    game) printf '%s\n' "{\"schema\":1,\"game_id\":\"$2\",\"status\":\"unindexed\",\"has_ra_badge\":false}" ;;
    collection) printf '%s\n' '{"schema":1,"collection":"retroachievements","items":[],"count":0}' ;;
    cores) printf '%s\n' '{"schema":1,"entries":[]}' ;;
    account) printf '%s\n' '{"schema":1,"configured":false,"authenticated":false}' ;;
    scan) printf '%s\n' '{"schema":1,"processed":0,"identified":0}' ;;
    *) exit 2 ;;
esac
EOF
    chmod +x "$ra"
    export BLOOM_RA_BIN="$ra"
    proxy="$BLOOM_TEST_ROOT/bloom-ra-proxy"
    cat >"$proxy" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >"$BLOOM_TEST_ROOT/proxy-args"
case "$1" in
status) printf '%s\n' '{"schema":1,"service":"bloom-ra-proxy","installed":false}' ;;
pending) printf '%s\n' '{"schema":1,"service":"bloom-ra-proxy","pending_awards":0}' ;;
*) exit 2 ;;
esac
EOF
    chmod +x "$proxy"
    export BLOOM_RA_PROXY_BIN="$proxy"
}

@test "achievements scan exposes only bounded scanner operations" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements scan --changed
    [ "$status" -eq 0 ]
    [ "$(sed -n '1p' "$BLOOM_TEST_ROOT/ra-args")" = scan ]
    [ "$(sed -n '2p' "$BLOOM_TEST_ROOT/ra-args")" = --changed ]

    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements scan --system GBA
    [ "$status" -eq 0 ]
    [ "$(sed -n '2p' "$BLOOM_TEST_ROOT/ra-args")" = --system ]
    [ "$(sed -n '3p' "$BLOOM_TEST_ROOT/ra-args")" = GBA ]

    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements scan --system '../../GBA'
    [ "$status" -eq 2 ]
}

teardown() {
    teardown_bloom_fixture
}

@test "achievements status delegates to the native service" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements status

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"schema":1'
    printf '%s' "$output" | grep -F '"state":"not_configured"'
    [ "$(sed -n '1p' "$BLOOM_TEST_ROOT/ra-args")" = status ]
    [ -z "$(sed -n '2p' "$BLOOM_TEST_ROOT/ra-args")" ]
    [ "$(sed -n '4p' "$BLOOM_TEST_ROOT/ra-args")" = "/mnt/SDCARD/.tmp_update/lib" ]
}

@test "achievements game preserves a validated identity as one argument" {
    game_id='bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'

    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements game "$game_id"

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"status":"unindexed"'
    [ "$(sed -n '1p' "$BLOOM_TEST_ROOT/ra-args")" = game ]
    [ "$(sed -n '2p' "$BLOOM_TEST_ROOT/ra-args")" = "$game_id" ]
}

@test "achievements collection delegates to the derived local index" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements collection

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"collection":"retroachievements"'
    [ "$(sed -n '1p' "$BLOOM_TEST_ROOT/ra-args")" = collection ]
    [ -z "$(sed -n '2p' "$BLOOM_TEST_ROOT/ra-args")" ]
}

@test "achievements cores delegates to the signed exact-SHA policy" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements cores

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"entries":[]'
    [ "$(sed -n '1p' "$BLOOM_TEST_ROOT/ra-args")" = cores ]
}

@test "achievements account status exposes only the bounded status operation" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements account status

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"authenticated":false'
    [ "$(sed -n '1p' "$BLOOM_TEST_ROOT/ra-args")" = account ]
    [ "$(sed -n '2p' "$BLOOM_TEST_ROOT/ra-args")" = status ]
}

@test "achievements account settings and sign-out delegate bounded arguments" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements account set mode hardcore
    [ "$status" -eq 0 ]
    [ "$(sed -n '1p' "$BLOOM_TEST_ROOT/ra-args")" = account ]
    [ "$(sed -n '2p' "$BLOOM_TEST_ROOT/ra-args")" = set ]
    [ "$(sed -n '3p' "$BLOOM_TEST_ROOT/ra-args")" = mode ]

    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements account sign-out
    [ "$status" -eq 0 ]
    [ "$(sed -n '2p' "$BLOOM_TEST_ROOT/ra-args")" = sign-out ]

    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements account set mode hardcore extra
    [ "$status" -eq 2 ]
}

@test "achievements proxy exposes bounded status and pending operations" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements proxy status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"installed":false'* ]]
    [ "$(cat "$BLOOM_TEST_ROOT/proxy-args")" = status ]

    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements proxy pending
    [ "$status" -eq 0 ]
    [[ "$output" == *'"pending_awards":0'* ]]
    [ "$(cat "$BLOOM_TEST_ROOT/proxy-args")" = pending ]
}

@test "achievements CLI rejects unsupported and malformed command shapes" {
    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements scan
    [ "$status" -eq 2 ]
    [ ! -e "$BLOOM_TEST_ROOT/ra-args" ]

    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements game one extra
    [ "$status" -eq 2 ]
    [ ! -e "$BLOOM_TEST_ROOT/ra-args" ]
}

@test "achievements CLI fails clearly when the service is unavailable" {
    export BLOOM_RA_BIN="$BLOOM_TEST_ROOT/missing-bloom-ra"

    run sh /workspace/static/build/.tmp_update/bin/bloomctl achievements status

    [ "$status" -eq 1 ]
    [[ "$output" == *"achievements service is unavailable"* ]]
}
