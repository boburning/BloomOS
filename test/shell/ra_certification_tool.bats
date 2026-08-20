#!/usr/bin/env bats

setup() {
    export SDCARD="$BATS_TEST_TMPDIR/sd"
    mkdir -p "$SDCARD/.tmp_update/bin" "$SDCARD/Roms/GBA" "$SDCARD/RetroArch/.retroarch/cores"
    touch "$SDCARD/.bloom-dev" "$SDCARD/Roms/GBA/test.gba"
    printf core >"$SDCARD/RetroArch/.retroarch/cores/gpsp_libretro.so"
    printf fallback >"$SDCARD/RetroArch/.retroarch/cores/mgba_libretro.so"
    game_id="$BATS_TEST_TMPDIR/game-id"
    cat >"$game_id" <<'SH'
#!/bin/sh
printf '%s\n' 'bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'
SH
    ra="$BATS_TEST_TMPDIR/ra"
    jq_bin=$(command -v jq)
    core_sha=$(sha256sum "$SDCARD/RetroArch/.retroarch/cores/gpsp_libretro.so" | sed 's/[[:space:]].*//')
    fallback_sha=$(sha256sum "$SDCARD/RetroArch/.retroarch/cores/mgba_libretro.so" | sed 's/[[:space:]].*//')
    cat >"$ra" <<SH
#!/bin/sh
case "\$1" in
game) printf '%s\n' '{"schema":1,"status":"identified","ra_game_id":1234}' ;;
cores) printf '%s\n' '{"schema":1,"entries":[{"system":"gba","core":"gpsp_libretro.so","binary_sha256":"$core_sha","bloom_ra_status":"best_effort"},{"system":"gba","core":"mgba_libretro.so","binary_sha256":"$fallback_sha","bloom_ra_status":"best_effort"}]}' ;;
account) printf '%s\n' '{"schema":1,"authenticated":true}' ;;
*) exit 2 ;;
esac
SH
    chmod +x "$game_id" "$ra"
    export BLOOM_GAME_ID_BIN="$game_id"
    export BLOOM_RA_BIN="$ra"
    export BLOOM_JQ_BIN="$jq_bin"
    smoke="$BATS_TEST_TMPDIR/smoke"
    cat >"$smoke" <<SH
#!/bin/sh
printf '%s\n' '{"schema":1,"status":"passed","selected_core":"mgba_libretro.so","selected_core_sha256":"$fallback_sha","graceful_exit":true,"save_flush":true}'
SH
    chmod +x "$smoke"
    export BLOOM_GAME_SMOKE_BIN="$smoke"
    export BLOOM_SD_ROOT="$SDCARD"
    export TOOL="$BATS_TEST_TMPDIR/bloom-ra-test"
    tr -d '\r' </workspace/src/bloomRaTest/bloom-ra-test >"$TOOL"
    chmod +x "$TOOL"
}

@test "guarded RA certification preflight reports no private ROM data" {
    encoded=$(printf '%s' "$SDCARD/Roms/GBA/test.gba" | base64 | tr -d '\r\n')
    run sh "$TOOL" --system GBA --rom-base64 "$encoded" --core gpsp
    [ "$status" -eq 0 ]
    [[ "$output" == *'"identification":"pass"'* ]]
    [[ "$output" == *'"bloom_ra_status":"best_effort"'* ]]
    [[ "$output" == *'"login":"pass"'* ]]
    [[ "$output" == *'"unlock_test":"operator_required"'* ]]
    [[ "$output" != *'/Roms/'* ]]
}

@test "certification session mode reports only a validated lifecycle result" {
    encoded=$(printf '%s' "$SDCARD/Roms/GBA/test.gba" | base64 | tr -d '\r\n')
    run sh "$TOOL" --system GBA --rom-base64 "$encoded" --core gpsp --session-seconds 5
    [ "$status" -eq 0 ]
    [[ "$output" == *'"session_exit":"pass"'* ]]
    [[ "$output" == *'"save_flush":"pass"'* ]]
    [[ "$output" == *'"core":"mgba_libretro.so"'* ]]
    [[ "$output" == *"\"core_sha256\":\"$fallback_sha\""* ]]
    [[ "$output" == *'"unlock_test":"operator_required"'* ]]
}

@test "certification session mode rejects unbounded duration and a non-default core" {
    encoded=$(printf '%s' "$SDCARD/Roms/GBA/test.gba" | base64 | tr -d '\r\n')
    run sh "$TOOL" --system GBA --rom-base64 "$encoded" --core gpsp --session-seconds 901
    [ "$status" -eq 1 ]
    [[ "$output" == *'invalid_session_duration'* ]]
    cp "$SDCARD/RetroArch/.retroarch/cores/gpsp_libretro.so" "$SDCARD/RetroArch/.retroarch/cores/mgba_libretro.so"
    run sh "$TOOL" --system GBA --rom-base64 "$encoded" --core mgba --session-seconds 5
    [ "$status" -eq 1 ]
    [[ "$output" == *'session_core_not_default'* ]]
}

@test "certification preflight reports authentication attention without exposing account data" {
    sed -i 's/"authenticated":true/"authenticated":false,"username":"PrivateUser"/' "$BLOOM_RA_BIN"
    encoded=$(printf '%s' "$SDCARD/Roms/GBA/test.gba" | base64 | tr -d '\r\n')
    run sh "$TOOL" --system GBA --rom-base64 "$encoded" --core gpsp
    [ "$status" -eq 0 ]
    [[ "$output" == *'"login":"attention_required"'* ]]
    [[ "$output" != *'PrivateUser'* ]]
}

@test "RA certification preflight requires developer mode and confines ROMs" {
    rm "$SDCARD/.bloom-dev"
    encoded=$(printf '%s' "$SDCARD/Roms/GBA/test.gba" | base64 | tr -d '\r\n')
    run sh "$TOOL" --system GBA --rom-base64 "$encoded" --core gpsp
    [ "$status" -eq 1 ]
    [[ "$output" == *'developer_mode_required'* ]]
    touch "$SDCARD/.bloom-dev"
    printf rom >"$BATS_TEST_TMPDIR/outside.gba"
    encoded=$(printf '%s' "$BATS_TEST_TMPDIR/outside.gba" | base64 | tr -d '\r\n')
    run sh "$TOOL" --system GBA --rom-base64 "$encoded" --core gpsp
    [ "$status" -eq 1 ]
    [[ "$output" == *'rom_outside_system'* ]]
}
