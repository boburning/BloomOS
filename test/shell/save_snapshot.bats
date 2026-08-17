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

@test "automatically retains only the newest configured verified snapshots" {
    export BLOOM_SNAPSHOT_RETENTION=2
    ids=""
    for value in 1 2 3 4; do
        printf 'save-%s\n' "$value" >"$SDCARD/Saves/CurrentProfile/saves/Gambatte/game.srm"
        run "$SNAPSHOT" create manual
        [ "$status" -eq 0 ]
        id="$(printf '%s' "$output" | sed -n 's/.*"id":"\([^"]*\)".*/\1/p')"
        ids="$ids $id"
    done

    [ "$(find "$SDCARD/Saves/BloomSnapshots" -mindepth 1 -maxdepth 1 -type d ! -name '.lock' | wc -l)" -eq 2 ]
    oldest="$(printf '%s\n' $ids | sed -n '1p')"
    newest="$(printf '%s\n' $ids | sed -n '4p')"
    [ ! -e "$SDCARD/Saves/BloomSnapshots/$oldest" ]
    [ -d "$SDCARD/Saves/BloomSnapshots/$newest" ]
    printf '%s' "$output" | grep -F '"retention":2'
}

@test "retention preserves the snapshot referenced by active update state" {
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_SNAPSHOT_RETENTION=1
    create_snapshot
    protected="$SNAPSHOT_ID"
    mkdir -p "$SDCARD/.bloom/update"
    printf '{"schema":1,"phase":"testing","snapshot_id":"%s"}\n' "$protected" >"$SDCARD/.bloom/update/state.json"

    for value in 2 3; do
        printf 'save-%s\n' "$value" >"$SDCARD/Saves/CurrentProfile/saves/Gambatte/game.srm"
        run "$SNAPSHOT" create pre-update
        [ "$status" -eq 0 ]
    done

    [ -d "$SDCARD/Saves/BloomSnapshots/$protected" ]
    [ "$(find "$SDCARD/Saves/BloomSnapshots" -mindepth 1 -maxdepth 1 -type d ! -name '.lock' | wc -l)" -eq 2 ]
}

@test "prune preserves corrupt evidence and rejects an invalid limit" {
    export BLOOM_SNAPSHOT_RETENTION=3
    create_snapshot
    corrupt="$SNAPSHOT_ID"
    printf 'damage\n' >>"$SDCARD/Saves/BloomSnapshots/$corrupt/saves.tar"
    create_snapshot
    valid="$SNAPSHOT_ID"

    run "$SNAPSHOT" prune 1
    [ "$status" -eq 0 ]
    [ -d "$SDCARD/Saves/BloomSnapshots/$corrupt" ]
    [ -d "$SDCARD/Saves/BloomSnapshots/$valid" ]

    run "$SNAPSHOT" prune 0
    [ "$status" -eq 1 ]
    [ -d "$SDCARD/Saves/BloomSnapshots/$corrupt" ]
    [ -d "$SDCARD/Saves/BloomSnapshots/$valid" ]
}

@test "lists verified referenced and corrupt snapshots as structured data" {
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_SNAPSHOT_RETENTION=5
    create_snapshot
    referenced="$SNAPSHOT_ID"
    mkdir -p "$SDCARD/.bloom/update"
    printf '{"schema":1,"phase":"testing","snapshot_id":"%s"}\n' "$referenced" >"$SDCARD/.bloom/update/state.json"
    create_snapshot
    corrupt="$SNAPSHOT_ID"
    printf 'damage\n' >>"$SDCARD/Saves/BloomSnapshots/$corrupt/saves.tar"

    run "$SNAPSHOT" list

    [ "$status" -eq 0 ]
    printf '%s' "$output" | jq -e --arg id "$referenced" \
        '.schema == 1 and any(.snapshots[]; .id == $id and .status == "verified" and .referenced == true)' >/dev/null
    printf '%s' "$output" | jq -e --arg id "$corrupt" \
        'any(.snapshots[]; .id == $id and .status == "unverified" and .referenced == false)' >/dev/null
    [ ! -e "$SDCARD/Saves/BloomSnapshots/.list-$$" ]
    [ ! -e "$SDCARD/Saves/BloomSnapshots/.lock" ]
}

@test "lists an empty snapshot set without creating storage" {
    export BLOOM_JQ_BIN=/usr/bin/jq

    run "$SNAPSHOT" list

    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"snapshots":[]}' ]
    [ ! -d "$SDCARD/Saves/BloomSnapshots" ]
}

@test "snapshot health is healthy for an empty or fully verified inventory" {
    export BLOOM_JQ_BIN=/usr/bin/jq
    run "$SNAPSHOT" health
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"healthy":true,"total":0,"unverified":0,"referenced":0}' ]
    [ ! -d "$SDCARD/Saves/BloomSnapshots" ]

    create_snapshot
    run "$SNAPSHOT" health
    [ "$status" -eq 0 ]
    printf '%s' "$output" | jq -e '.healthy == true and .total == 1 and .unverified == 0' >/dev/null
}

@test "snapshot health reports aggregate corruption without save details" {
    export BLOOM_JQ_BIN=/usr/bin/jq
    create_snapshot
    printf 'private-game-name\n' >>"$SDCARD/Saves/BloomSnapshots/$SNAPSHOT_ID/saves.tar"

    run "$SNAPSHOT" health

    [ "$status" -eq 1 ]
    printf '%s' "$output" | jq -e '.healthy == false and .total == 1 and .unverified == 1' >/dev/null
    ! printf '%s' "$output" | grep -F 'private-game-name'
    ! printf '%s' "$output" | grep -F "$SNAPSHOT_ID"
}
