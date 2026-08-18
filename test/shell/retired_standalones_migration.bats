#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export MIGRATION=/workspace/static/build/.tmp_update/script/migration/00021_remove_retired_emulator_standalones.sh
    mkdir -p "$SDCARD/RApp/PICO/FAKE08" "$SDCARD/RApp/PCSX-ReARMed" "$SDCARD/RApp/OpenBOR"
    sed "s#/mnt/SDCARD#$SDCARD#g" "$MIGRATION" >"$BLOOM_TEST_ROOT/migration"
    chmod +x "$BLOOM_TEST_ROOT/migration"
}

@test "removes retired standalones and preserves unrelated packages" {
    run "$BLOOM_TEST_ROOT/migration"

    [ "$status" -eq 0 ]
    [ ! -e "$SDCARD/RApp/PICO/FAKE08" ]
    [ ! -e "$SDCARD/RApp/PCSX-ReARMed" ]
    [ -d "$SDCARD/RApp/OpenBOR" ]
}

@test "is idempotent and does not follow a retired-package symlink" {
    rm -rf "$SDCARD/RApp/PICO/FAKE08"
    mkdir -p "$BLOOM_TEST_ROOT/outside"
    ln -s "$BLOOM_TEST_ROOT/outside" "$SDCARD/RApp/PICO/FAKE08"

    run "$BLOOM_TEST_ROOT/migration"
    [ "$status" -eq 0 ]
    [ -L "$SDCARD/RApp/PICO/FAKE08" ]
    [ -d "$BLOOM_TEST_ROOT/outside" ]

    run "$BLOOM_TEST_ROOT/migration"
    [ "$status" -eq 0 ]
}
