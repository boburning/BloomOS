#!/usr/bin/env bats

setup() {
    export ADAPTER=/workspace/src/bloomRaProxy/bloom-ra-proxy
    export BLOOM_RA_PROXY_UPSTREAM="$BATS_TEST_TMPDIR/raofflineproxy"
    export BLOOM_RA_PROXY_CONFIG_DIR="$BATS_TEST_TMPDIR/config"
    export BLOOM_RA_PROXY_LOG_FILE="$BATS_TEST_TMPDIR/proxy.log"
    export BLOOM_RA_PROXY_ROM_ROOT="$BATS_TEST_TMPDIR/Roms"
    export BLOOM_RA_BIN="$BATS_TEST_TMPDIR/bloom-ra"
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_RA_CREDENTIALS="$BATS_TEST_TMPDIR/credentials"
    export BLOOM_RA_PROXY_RUNTIME_DIR="$BATS_TEST_TMPDIR/runtime"
    printf '%s' fixture-token >"$BLOOM_RA_CREDENTIALS"
    cat >"$BLOOM_RA_BIN" <<'SH'
#!/bin/sh
[ "$1:$2" = account:launch ] || exit 1
printf '%s\n' '{"authenticated":true,"username":"BloomUser"}'
SH
    chmod +x "$BLOOM_RA_BIN"
}

make_upstream() {
    cat >"$BLOOM_RA_PROXY_UPSTREAM" <<'SH'
#!/bin/sh
case "$1" in
home-status)
    if [ -f "$BLOOM_RA_PROXY_CONFIG_DIR/running" ]; then
        printf '{"cached_games_count":2,"pending_awards_count":3,"service_running":true,"service_pid":%s,"autostart_enabled":false,"is_online":false}\n' "$(cat "$BLOOM_RA_PROXY_CONFIG_DIR/running")"
    else
        printf '%s\n' '{"cached_games_count":2,"pending_awards_count":3,"service_running":false,"service_pid":null,"autostart_enabled":false,"is_online":false}'
    fi
    ;;
run-service) printf '%s\n' "$$" >"$BLOOM_RA_PROXY_CONFIG_DIR/running"; sleep 30 ;;
pending-awards-count) printf '%s\n' '3' ;;
cached-games) printf '%s\n' 'Example ##GAMEID:42' ;;
cache-rom) printf '%s\n' "$*" >"$BLOOM_RA_PROXY_CONFIG_DIR/cache-rom.args" ;;
cache-folder-listing) printf '%s\n' "$*" >"$BLOOM_RA_PROXY_CONFIG_DIR/cache-system.args" ;;
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

@test "adapter owns the fixed loopback session endpoint" {
    make_upstream
    run "$ADAPTER" endpoint
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"service":"bloom-ra-proxy","host":"127.0.0.1:8080"}' ]
}

@test "adapter rejects upstream endpoint overrides" {
    make_upstream
    mkdir -p "$BLOOM_RA_PROXY_CONFIG_DIR"
    printf '%s\n' '{"proxy_port":9000}' >"$BLOOM_RA_PROXY_CONFIG_DIR/config.json"
    run "$ADAPTER" endpoint
    [ "$status" -eq 1 ]
    [[ "$output" == *'"code":"unsupported_endpoint_override"'* ]]
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

@test "service lifecycle consumes the pinned home-status JSON contract" {
    make_upstream
    run "$ADAPTER" start
    [ "$status" -eq 0 ]
    [[ "$output" == *'"service_running":true'* ]]
    run "$ADAPTER" stop
    [ "$status" -eq 0 ]
    [[ "$output" == *'"state":"stopped"'* ]]
}

@test "service start rejects a symlinked durable config path" {
    make_upstream
    mkdir "$BATS_TEST_TMPDIR/outside"
    ln -s "$BATS_TEST_TMPDIR/outside" "$BLOOM_RA_PROXY_CONFIG_DIR"
    run "$ADAPTER" start
    [ "$status" -eq 1 ]
    [[ "$output" == *'"code":"unsafe_config_path"'* ]]
}

@test "cache operations confine paths and preserve arguments as data" {
    make_upstream
    mkdir -p "$BLOOM_RA_PROXY_CONFIG_DIR" "$BLOOM_RA_PROXY_ROM_ROOT/GBA"
    rom="$BLOOM_RA_PROXY_ROM_ROOT/GBA/Bob's game.zip"
    printf rom >"$rom"
    run "$ADAPTER" cache-rom "$rom"
    [ "$status" -eq 0 ]
    [[ "$output" == *'"cached":true'* ]]
    grep -F -- "--path $rom" "$BLOOM_RA_PROXY_CONFIG_DIR/cache-rom.args"

    run "$ADAPTER" cache-system gba
    [ "$status" -eq 0 ]
    grep -F -- "--path $BLOOM_RA_PROXY_ROM_ROOT/GBA" "$BLOOM_RA_PROXY_CONFIG_DIR/cache-system.args"

    printf outside >"$BATS_TEST_TMPDIR/outside.rom"
    run "$ADAPTER" cache-rom "$BATS_TEST_TMPDIR/outside.rom"
    [ "$status" -eq 1 ]
    [[ "$output" == *'"code":"invalid_rom_path"'* ]]
}

@test "cache credential bridge is private and never changes permanent RetroArch config" {
    make_upstream
    mkdir -p "$BLOOM_RA_PROXY_CONFIG_DIR" "$BLOOM_RA_PROXY_ROM_ROOT/GBA"
    printf rom >"$BLOOM_RA_PROXY_ROM_ROOT/GBA/fixture.gba"
    printf permanent >"$BATS_TEST_TMPDIR/retroarch.cfg"
    before=$(sha256sum "$BATS_TEST_TMPDIR/retroarch.cfg")
    run "$ADAPTER" cache-rom "$BLOOM_RA_PROXY_ROM_ROOT/GBA/fixture.gba"
    [ "$status" -eq 0 ]
    [ ! -e "$BLOOM_RA_PROXY_RUNTIME_DIR/credentials.append" ]
    [ "$(stat -c %a "$BLOOM_RA_PROXY_CONFIG_DIR/config.json")" = 600 ]
    ! grep -F fixture-token "$output"
    [ "$before" = "$(sha256sum "$BATS_TEST_TMPDIR/retroarch.cfg")" ]
}
