#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export MIGRATION=/workspace/static/build/.tmp_update/script/migration/00022_remove_retired_nds_package.sh
    mkdir -p \
        "$SDCARD/Emu/NDS/libs" \
        "$SDCARD/Emu/GBA" \
        "$SDCARD/Roms/NDS" \
        "$SDCARD/Saves/CurrentProfile/saves/Drastic"
    touch \
        "$SDCARD/Emu/NDS/drastic" \
        "$SDCARD/Emu/NDS/libs/libSDL2.so" \
        "$SDCARD/Emu/GBA/launch.sh" \
        "$SDCARD/Roms/NDS/game.nds" \
        "$SDCARD/Saves/CurrentProfile/saves/Drastic/game.dsv"
    sed "s#/mnt/SDCARD#$SDCARD#g" "$MIGRATION" >"$BLOOM_TEST_ROOT/migration"
    chmod +x "$BLOOM_TEST_ROOT/migration"
}

@test "removes the retired NDS package and preserves ROMs, saves, and other emulators" {
    run "$BLOOM_TEST_ROOT/migration"

    [ "$status" -eq 0 ]
    [ ! -e "$SDCARD/Emu/NDS" ]
    [ -f "$SDCARD/Emu/GBA/launch.sh" ]
    [ -f "$SDCARD/Roms/NDS/game.nds" ]
    [ -f "$SDCARD/Saves/CurrentProfile/saves/Drastic/game.dsv" ]
}

@test "is idempotent and does not follow an NDS package symlink" {
    rm -rf "$SDCARD/Emu/NDS"
    mkdir -p "$BLOOM_TEST_ROOT/outside"
    touch "$BLOOM_TEST_ROOT/outside/drastic"
    ln -s "$BLOOM_TEST_ROOT/outside" "$SDCARD/Emu/NDS"

    run "$BLOOM_TEST_ROOT/migration"
    [ "$status" -eq 0 ]
    [ -L "$SDCARD/Emu/NDS" ]
    [ -f "$BLOOM_TEST_ROOT/outside/drastic" ]

    run "$BLOOM_TEST_ROOT/migration"
    [ "$status" -eq 0 ]
}
