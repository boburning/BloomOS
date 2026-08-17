#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export GAME_SMOKE=/workspace/static/build/.tmp_update/bin/bloom-game-smoke
    export BLOOM_SD_ROOT="$SDCARD"
    mkdir -p "$SDCARD/.tmp_update/config" "$SDCARD/Roms/GB" "$SDCARD/Roms/GBC"
}

teardown() { teardown_bloom_fixture; }

encode() { printf '%s' "$1" | base64 | tr -d '\r\n'; }

@test "game smoke requires explicit developer mode" {
    rom="$SDCARD/Roms/GB/Test.gb"
    : >"$rom"

    run "$GAME_SMOKE" GB "$(encode "$rom")" 10

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"reason":"developer_mode_required"'
}

@test "game smoke rejects unsupported systems before launch" {
    : >"$SDCARD/.bloom-dev"

    run "$GAME_SMOKE" N64 "$(encode "$SDCARD/Roms/N64/Test.z64")" 10

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"reason":"unsupported_system"'
}

@test "game smoke confines ROMs to the selected system directory" {
    : >"$SDCARD/.bloom-dev"
    rom="$SDCARD/Roms/GBC/Test.gbc"
    : >"$rom"

    run "$GAME_SMOKE" GB "$(encode "$rom")" 10

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"reason":"rom_outside_system_directory"'
}

@test "game smoke rejects command-sensitive ROM paths" {
    : >"$SDCARD/.bloom-dev"
    rom="$SDCARD/Roms/GB/Unsafe\$Name.gb"
    : >"$rom"

    run "$GAME_SMOKE" GB "$(encode "$rom")" 10

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"reason":"unsafe_rom_path"'
}
