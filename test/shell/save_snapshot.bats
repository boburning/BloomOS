#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export SNAPSHOT=/workspace/static/build/.tmp_update/bin/bloom-save-snapshot
    export BLOOM_SD_ROOT="$SDCARD"
    export BLOOM_SESSION_STATE_FILE="$BLOOM_TEST_ROOT/session-state"
    mkdir -p "$SDCARD/Saves/CurrentProfile/saves/Gambatte" "$SDCARD/Saves/CurrentProfile/states/Gambatte"
    printf 'save-v1\n' >"$SDCARD/Saves/CurrentProfile/saves/Gambatte/game.srm"
    printf 'state-v1\n' >"$SDCARD/Saves/CurrentProfile/states/Gambatte/game.state"
}

teardown() { teardown_bloom_fixture; }

create_snapshot() {
    run "$SNAPSHOT" create manual
    [ "$status" -eq 0 ]
    SNAPSHOT_ID="$(printf '%s' "$output" | sed -n 's/.*"id":"\([^"]*\)".*/\1/p')"
    [ -n "$SNAPSHOT_ID" ]
}

@test "creates and verifies an atomic checksummed save snapshot" {
    create_snapshot

    run "$SNAPSHOT" verify "$SNAPSHOT_ID"

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"status":"verified"'
    [ -f "$SDCARD/Saves/BloomSnapshots/$SNAPSHOT_ID/complete" ]
    [ ! -e "$SDCARD/Saves/BloomSnapshots/.lock" ]
}

@test "detects snapshot corruption before restore" {
    create_snapshot
    printf 'damage\n' >>"$SDCARD/Saves/BloomSnapshots/$SNAPSHOT_ID/saves.tar"

    run "$SNAPSHOT" restore "$SNAPSHOT_ID"

    [ "$status" -eq 1 ]
    grep -Fx save-v1 "$SDCARD/Saves/CurrentProfile/saves/Gambatte/game.srm"
}

@test "restores both save trees while the session is inactive" {
    create_snapshot
    printf 'save-v2\n' >"$SDCARD/Saves/CurrentProfile/saves/Gambatte/game.srm"
    printf 'state-v2\n' >"$SDCARD/Saves/CurrentProfile/states/Gambatte/game.state"
    printf 'STOPPED\n' >"$BLOOM_SESSION_STATE_FILE"

    run "$SNAPSHOT" restore "$SNAPSHOT_ID"

    [ "$status" -eq 0 ]
    grep -Fx save-v1 "$SDCARD/Saves/CurrentProfile/saves/Gambatte/game.srm"
    grep -Fx state-v1 "$SDCARD/Saves/CurrentProfile/states/Gambatte/game.state"
    ! find "$SDCARD/Saves/CurrentProfile" -maxdepth 1 -name '.bloom-rollback.*' | grep -q .
}

@test "refuses restore during an active game session" {
    create_snapshot
    printf 'RUNNING\n' >"$BLOOM_SESSION_STATE_FILE"

    run "$SNAPSHOT" restore "$SNAPSHOT_ID"

    [ "$status" -eq 1 ]
    grep -Fx save-v1 "$SDCARD/Saves/CurrentProfile/saves/Gambatte/game.srm"
}

@test "rejects symlinks instead of archiving external content" {
    ln -s "$BLOOM_TEST_ROOT" "$SDCARD/Saves/CurrentProfile/saves/external"

    run "$SNAPSHOT" create manual

    [ "$status" -eq 1 ]
    [ ! -d "$SDCARD/Saves/BloomSnapshots" ]
}
