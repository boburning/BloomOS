#!/usr/bin/env bats

setup() {
    export ADAPTER=/workspace/src/bloomRaProxy/bloom-ra-proxy
    export BLOOM_RA_PROXY_UPSTREAM="$BATS_TEST_TMPDIR/raofflineproxy"
    export BLOOM_RA_PROXY_CONFIG_DIR="$BATS_TEST_TMPDIR/config"
    export BLOOM_RA_PROXY_LOG_FILE="$BATS_TEST_TMPDIR/proxy.log"
}

make_upstream() {
    cat >"$BLOOM_RA_PROXY_UPSTREAM" <<'SH'
#!/bin/sh
case "$1" in
home-status) printf '%s\n' '{"cached_games_count":2,"pending_awards_count":3,"service_running":false,"service_pid":null,"autostart_enabled":false,"is_online":false}' ;;
service-status) printf '%s\n' '{"running":false,"pid":null}' ;;
pending-awards-count) printf '%s\n' '3' ;;
cached-games) printf '%s\n' 'Example ##GAMEID:42' ;;
*) exit 2 ;;
esac
SH
    chmod +x "$BLOOM_RA_PROXY_UPSTREAM"
}

@test "adapter reports an absent optional package cleanly" {
    run "$ADAPTER" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"installed":false'* ]]
    [[ "$output" == *'"state":"not_installed"'* ]]
}

@test "adapter translates bounded upstream status and counts" {
    make_upstream
    run "$ADAPTER" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"service":"bloom-ra-proxy"'* ]]
    [[ "$output" == *'"pending_awards_count":3'* ]]
    run "$ADAPTER" pending
    [ "$status" -eq 0 ]
    [[ "$output" == *'"pending_awards":3'* ]]
}

@test "cached-game validates identifiers and does not expose titles" {
    make_upstream
    run "$ADAPTER" cached-game 42
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"service":"bloom-ra-proxy","ra_game_id":42,"cached":true}' ]
    run "$ADAPTER" cached-game '../42'
    [ "$status" -eq 1 ]
    [[ "$output" == *'"code":"invalid_game_id"'* ]]
}

@test "adapter rejects malformed upstream output" {
    make_upstream
    sed -i "s/printf '%s\\\\n' '3'/printf '%s\\\\n' 'secret'/" "$BLOOM_RA_PROXY_UPSTREAM"
    run "$ADAPTER" pending
    [ "$status" -eq 1 ]
    [[ "$output" == *'"code":"invalid_upstream_response"'* ]]
}

@test "stopping an already stopped service never invokes config patching" {
    make_upstream
    run "$ADAPTER" stop
    [ "$status" -eq 0 ]
    [[ "$output" == *'"state":"stopped"'* ]]
    [ ! -e "$BATS_TEST_TMPDIR/retroarch.cfg" ]
}

@test "service start rejects a symlinked durable config path" {
    make_upstream
    mkdir "$BATS_TEST_TMPDIR/outside"
    ln -s "$BATS_TEST_TMPDIR/outside" "$BLOOM_RA_PROXY_CONFIG_DIR"
    run "$ADAPTER" start
    [ "$status" -eq 1 ]
    [[ "$output" == *'"code":"unsafe_config_path"'* ]]
}
