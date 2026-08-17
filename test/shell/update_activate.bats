#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export ACTIVATE=/workspace/static/build/.tmp_update/bin/bloom-update-activate
    export BLOOM_SD_ROOT="$SDCARD"
    export BLOOM_UPDATE_ROOT="$SDCARD/.bloom/update"
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_UPDATE_STATE_BIN="$BLOOM_TEST_ROOT/mock-state"
    export BLOOM_SAVE_SNAPSHOT_BIN="$BLOOM_TEST_ROOT/mock-snapshot"
    export VERSION=1.2.3
    mkdir -p \
        "$BLOOM_UPDATE_ROOT/candidates/$VERSION/miyoo/app/.tmp_update" \
        "$BLOOM_UPDATE_ROOT/candidates/$VERSION/RetroArch" \
        "$BLOOM_UPDATE_ROOT/staged/1.2.2"
    printf 'trigger\n' >"$BLOOM_UPDATE_ROOT/candidates/$VERSION/miyoo/app/MainUI"
    printf 'core\n' >"$BLOOM_UPDATE_ROOT/candidates/$VERSION/miyoo/app/.tmp_update/onion.pak"
    printf 'retroarch\n' >"$BLOOM_UPDATE_ROOT/candidates/$VERSION/RetroArch/retroarch.pak"
    printf '{"schema":1,"version":"1.2.2","path":"%s"}\n' \
        "$BLOOM_UPDATE_ROOT/staged/1.2.2" >"$BLOOM_UPDATE_ROOT/known-good.json"
    cat >"$BLOOM_UPDATE_STATE_BIN" <<EOF
#!/bin/sh
if [ "\$1" = status ]; then
    printf '%s\n' '{"schema":1,"phase":"armed","pending_version":"$VERSION","boot_attempts":0}'
elif [ "\$1" = activation-start ]; then
    printf '%s:%s:%s\n' "\$1" "\$2" "\$3" >>"$BLOOM_TEST_ROOT/calls"
else
    exit 2
fi
EOF
    cat >"$BLOOM_SAVE_SNAPSHOT_BIN" <<EOF
#!/bin/sh
printf '%s:%s\n' "\$1" "\$2" >>"$BLOOM_TEST_ROOT/calls"
printf '%s\n' '{"schema":1,"status":"created","id":"snapshot-1"}'
EOF
    chmod +x "$BLOOM_UPDATE_STATE_BIN" "$BLOOM_SAVE_SNAPSHOT_BIN"
}

teardown() { teardown_bloom_fixture; }

@test "publishes installer payloads before the boot trigger" {
    run "$ACTIVATE" "$VERSION"

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"status":"activation_pending"'
    [ -f "$SDCARD/miyoo/app/.tmp_update/onion.pak" ]
    [ -f "$SDCARD/RetroArch/retroarch.pak" ]
    [ -f "$SDCARD/miyoo/app/MainUI" ]
    [ "$(sed -n '1p' "$BLOOM_TEST_ROOT/calls")" = 'create:pre-update' ]
    [ "$(sed -n '2p' "$BLOOM_TEST_ROOT/calls")" = 'activation-start:1.2.3:snapshot-1' ]
}

@test "refuses activation without a retained known-good release" {
    rm "$BLOOM_UPDATE_ROOT/known-good.json"

    run "$ACTIVATE" "$VERSION"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'no prior known-good release'
    [ ! -e "$SDCARD/miyoo/app/MainUI" ]
    [ ! -e "$BLOOM_TEST_ROOT/calls" ]
}

@test "refuses to replace any pending installer" {
    mkdir -p "$SDCARD/miyoo/app"
    printf 'existing\n' >"$SDCARD/miyoo/app/MainUI"

    run "$ACTIVATE" "$VERSION"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'installer is already pending'
    grep -F existing "$SDCARD/miyoo/app/MainUI"
    [ ! -e "$BLOOM_TEST_ROOT/calls" ]
}

@test "durable update state is outside the replaceable system directory" {
    grep -F 'UPDATE_ROOT="${BLOOM_UPDATE_ROOT:-$SD_ROOT/.bloom/update}"' \
        /workspace/static/build/.tmp_update/bin/bloom-update-stage
    grep -F 'UPDATE_ROOT="${BLOOM_UPDATE_ROOT:-$SD_ROOT/.bloom/update}"' \
        /workspace/static/build/.tmp_update/bin/bloom-update-prepare
    grep -F 'UPDATE_ROOT="${BLOOM_UPDATE_ROOT:-$SD_ROOT/.bloom/update}"' \
        /workspace/static/build/.tmp_update/bin/bloom-update-state
    grep -F 'UPDATE_ROOT="${BLOOM_UPDATE_ROOT:-$SD_ROOT/.bloom/update}"' "$ACTIVATE"
}
