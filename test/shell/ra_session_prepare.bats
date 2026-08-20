#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export PREPARE=/workspace/static/build/.tmp_update/bin/bloom-ra-session-prepare
    export BLOOM_RA_BIN="$MOCK_BIN/bloom-ra"
    export BLOOM_LAUNCH_BIN="$MOCK_BIN/bloom-launch"
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_CORE_ROOT="$BLOOM_TEST_ROOT/cores"
    export BLOOM_RA_CREDENTIALS="$BLOOM_TEST_ROOT/credentials"
    export BLOOM_SESSION_ROOT="$BLOOM_TEST_ROOT/session"
    mkdir -p "$BLOOM_CORE_ROOT" "$BLOOM_SESSION_ROOT"
    printf '%s' core-bytes >"$BLOOM_CORE_ROOT/gambatte_libretro.so"
    printf '%s' fixture-token >"$BLOOM_RA_CREDENTIALS"
    export REQUEST="$BLOOM_TEST_ROOT/request.json"
    printf '%s\n' '{}' >"$REQUEST"
    core_sha=$(sha256sum "$BLOOM_CORE_ROOT/gambatte_libretro.so" | awk '{print $1}')
    export CORE_SHA="$core_sha"

    cat >"$BLOOM_LAUNCH_BIN" <<'EOF'
#!/bin/sh
case "$1" in
validate) exit 0 ;;
get)
    case "$3" in
    game_id) printf '%s\n' bloom-game-v1:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ;;
    system_id) printf '%s\n' gb ;;
    core) printf '%s\n' gambatte_libretro.so ;;
    *) exit 1 ;;
    esac
    ;;
resolve-achievements) printf 'resolve:%s\n' "$*" >>"$MOCK_LOG" ;;
write-ra-config)
    IFS= read -r token
    [ "$token" = fixture-token ] || exit 1
    printf 'cheevos_enable = "true"\n' >"$3"
    chmod 600 "$3"
    printf 'config:%s\n' "$*" >>"$MOCK_LOG"
    ;;
*) exit 1 ;;
esac
EOF
    chmod +x "$BLOOM_LAUNCH_BIN"

    cat >"$BLOOM_RA_BIN" <<'EOF'
#!/bin/sh
case "$1:$2" in
account:launch)
    printf '%s\n' '{"schema":1,"enabled":true,"authenticated":true,"username":"BloomUser","mode":"softcore","offline_casual":false}'
    ;;
game:*)
    if [ "${RA_GAME_MISSING:-0}" -eq 1 ]; then id=null; badge=false; else id=1234; badge=true; fi
    printf '{"schema":1,"has_ra_badge":%s,"ra":{"game_id":%s}}\n' "$badge" "$id"
    ;;
cores:)
    status=${RA_CORE_STATUS:-best_effort}
    printf '{"schema":1,"entries":[{"system":"gb","core":"gambatte_libretro.so","binary_sha256":"%s","bloom_ra_status":"%s"}]}\n' "$CORE_SHA" "$status"
    ;;
*) exit 1 ;;
esac
EOF
    chmod +x "$BLOOM_RA_BIN"
}

teardown() { teardown_bloom_fixture; }

@test "direct softcore policy creates only a private session config" {
    export BLOOM_RA_LOG_BIN="$MOCK_BIN/bloom-ra-log"
    printf '#!/bin/sh\nprintf "log:%%s\\n" "$*" >>"$MOCK_LOG"\n' >"$BLOOM_RA_LOG_BIN"
    chmod +x "$BLOOM_RA_LOG_BIN"
    run "$PREPARE" "$REQUEST"
    [ "$status" -eq 0 ]
    [[ "$output" == *'"transport":"direct"'* ]]
    grep -F 'resolve:resolve-achievements' "$MOCK_LOG"
    grep -F 'config:write-ra-config' "$MOCK_LOG"
    [ "$(stat -c %a "$BLOOM_SESSION_ROOT/ra.cfg")" = 600 ]
    ! grep -F fixture-token "$MOCK_LOG"
    grep -F "log:launch softcore direct gambatte_libretro.so $CORE_SHA best_effort" "$MOCK_LOG"
    ! grep -E '^log:.*BloomUser' "$MOCK_LOG"
}

@test "unknown softcore game degrades to RA-disabled launch" {
    export RA_GAME_MISSING=1
    run "$PREPARE" "$REQUEST"
    [ "$status" -eq 0 ]
    [[ "$output" == *'"enabled":false'* ]]
    grep -F 'false disabled false false 0 not_applicable' "$MOCK_LOG"
    [ ! -e "$BLOOM_SESSION_ROOT/ra.cfg" ]
}

@test "unknown Hardcore game fails before launch without downgrading" {
    export RA_GAME_MISSING=1
    sed -i 's/"mode":"softcore"/"mode":"hardcore"/' "$BLOOM_RA_BIN"
    run "$PREPARE" "$REQUEST"
    [ "$status" -eq 1 ]
    [[ "$output" == *'Hardcore requires exact game identification'* ]]
    ! grep -F 'false disabled' "$MOCK_LOG"
}

@test "incompatible core disables softcore RA without blocking the game" {
    export RA_CORE_STATUS=incompatible
    run "$PREPARE" "$REQUEST"
    [ "$status" -eq 0 ]
    [[ "$output" == *'"enabled":false'* ]]
    [ ! -e "$BLOOM_SESSION_ROOT/ra.cfg" ]
}

@test "missing core policy is reported as untested without blocking RA" {
    sed -i 's/cores:)/cores:) exit 1 ;;\nignored:)/' "$BLOOM_RA_BIN"
    run "$PREPARE" "$REQUEST"
    [ "$status" -eq 0 ]
    [[ "$output" == *'"core_certification":"untested"'* ]]
    grep -F 'true softcore false true 1234 untested' "$MOCK_LOG"
}
