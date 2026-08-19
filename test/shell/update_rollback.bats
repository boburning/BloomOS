#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export ROLLBACK=/workspace/static/build/.tmp_update/bin/bloom-update-rollback
    export BLOOM_SD_ROOT="$SDCARD"
    export BLOOM_UPDATE_ROOT="$SDCARD/.bloom/update"
    export BLOOM_UPDATE_STATE_BIN="$BLOOM_TEST_ROOT/mock-state"
    export BLOOM_UPDATE_PREPARE_BIN="$BLOOM_TEST_ROOT/mock-prepare"
    export BLOOM_SAVE_SNAPSHOT_BIN="$BLOOM_TEST_ROOT/mock-snapshot"
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_MOUNT_BIN="$BLOOM_TEST_ROOT/mock-mount"
    export BLOOM_UMOUNT_BIN="$BLOOM_TEST_ROOT/mock-umount"
    cat >"$BLOOM_UPDATE_STATE_BIN" <<'EOF'
#!/bin/sh
case "$1" in
    recovery)
        [ "${RECOVERY_AVAILABLE:-1}" = 1 ] || exit 1
        printf '{"schema":1,"version":"1.2.2","status":"recovery_available"}\n'
        ;;
    *) printf 'state:%s:%s:%s\n' "$1" "${2:-}" "${3:-}" >>"$BLOOM_TEST_ROOT/calls" ;;
esac
EOF
    cat >"$BLOOM_UPDATE_PREPARE_BIN" <<'EOF'
#!/bin/sh
printf 'prepare:%s:%s\n' "$1" "$2" >>"$BLOOM_TEST_ROOT/calls"
candidate="$BLOOM_UPDATE_ROOT/candidates/$2"
rm -rf "$candidate"
mkdir -p "$candidate/miyoo/app/.tmp_update" "$candidate/RetroArch"
printf 'fresh-trigger\n' >"$candidate/miyoo/app/MainUI"
printf 'fresh-core\n' >"$candidate/miyoo/app/.tmp_update/onion.pak"
printf 'fresh-ra\n' >"$candidate/RetroArch/retroarch.pak"
EOF
    cat >"$BLOOM_SAVE_SNAPSHOT_BIN" <<'EOF'
#!/bin/sh
printf 'snapshot:%s:%s\n' "$1" "$2" >>"$BLOOM_TEST_ROOT/calls"
printf '{"schema":1,"id":"snapshot-rollback"}\n'
EOF
    cat >"$BLOOM_MOUNT_BIN" <<EOF
#!/bin/sh
printf '/dev/mmcblk0p1 on $SDCARD/miyoo/app/MainUI type vfat (rw)\n'
EOF
    cat >"$BLOOM_UMOUNT_BIN" <<'EOF'
#!/bin/sh
printf 'umount:%s:%s\n' "$1" "$2" >>"$BLOOM_TEST_ROOT/calls"
EOF
    chmod +x "$BLOOM_UPDATE_STATE_BIN" "$BLOOM_UPDATE_PREPARE_BIN" "$BLOOM_SAVE_SNAPSHOT_BIN" "$BLOOM_MOUNT_BIN" "$BLOOM_UMOUNT_BIN"
}

teardown() { teardown_bloom_fixture; }

@test "rebuilds signed known-good payload before publishing rollback trigger" {
    mkdir -p "$SDCARD/miyoo/app"
    printf 'installed MainUI binary\n' >"$SDCARD/miyoo/app/MainUI"

    run "$ROLLBACK"

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"status":"rollback_pending"'
    [ "$(sed -n '1p' "$BLOOM_TEST_ROOT/calls")" = 'prepare:--replace:1.2.2' ]
    [ "$(sed -n '2p' "$BLOOM_TEST_ROOT/calls")" = 'snapshot:create:pre-rollback' ]
    [ "$(sed -n '3p' "$BLOOM_TEST_ROOT/calls")" = 'state:rollback-start:1.2.2:snapshot-rollback' ]
    [ "$(sed -n '4p' "$BLOOM_TEST_ROOT/calls")" = "umount:-l:$SDCARD/miyoo/app/MainUI" ]
    grep -Fx fresh-core "$SDCARD/miyoo/app/.tmp_update/onion.pak"
    grep -Fx fresh-ra "$SDCARD/RetroArch/retroarch.pak"
    grep -Fx fresh-trigger "$SDCARD/miyoo/app/MainUI"
}

@test "refuses rollback without recovery state before rebuilding or publishing" {
    export RECOVERY_AVAILABLE=0

    run "$ROLLBACK"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'signed recovery is unavailable'
    [ ! -e "$BLOOM_TEST_ROOT/calls" ]
    [ ! -e "$SDCARD/miyoo/app/MainUI" ]
}

@test "refuses to overwrite an existing installer script" {
    mkdir -p "$SDCARD/miyoo/app"
    printf '#!/bin/sh\nexisting\n' >"$SDCARD/miyoo/app/MainUI"

    run "$ROLLBACK"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'installer is already pending'
    grep -Fx existing "$SDCARD/miyoo/app/MainUI"
    ! grep -q '^snapshot:' "$BLOOM_TEST_ROOT/calls"
}

@test "refuses to replace a symlinked MainUI boundary" {
    mkdir -p "$SDCARD/miyoo/app" "$BLOOM_TEST_ROOT/outside"
    printf 'outside\n' >"$BLOOM_TEST_ROOT/outside/MainUI"
    ln -s "$BLOOM_TEST_ROOT/outside/MainUI" "$SDCARD/miyoo/app/MainUI"

    run "$ROLLBACK"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'installer is already pending'
    [ -L "$SDCARD/miyoo/app/MainUI" ]
    grep -Fx outside "$BLOOM_TEST_ROOT/outside/MainUI"
    ! grep -q '^snapshot:' "$BLOOM_TEST_ROOT/calls"
}

@test "removes empty residue but refuses populated or symlinked rollback payloads" {
    mkdir -p "$SDCARD/miyoo/app/.tmp_update"
    printf 'installed MainUI binary\n' >"$SDCARD/miyoo/app/MainUI"

    run "$ROLLBACK"
    [ "$status" -eq 0 ]
    grep -Fx fresh-core "$SDCARD/miyoo/app/.tmp_update/onion.pak"

    rm -rf "$SDCARD/miyoo/app/.tmp_update" "$SDCARD/RetroArch/retroarch.pak"
    mkdir -p "$SDCARD/miyoo/app/.tmp_update"
    touch "$SDCARD/miyoo/app/.tmp_update/pending"
    run "$ROLLBACK"
    [ "$status" -eq 1 ]
    [ -f "$SDCARD/miyoo/app/.tmp_update/pending" ]

    rm -rf "$SDCARD/miyoo/app/.tmp_update"
    mkdir -p "$BLOOM_TEST_ROOT/outside"
    ln -s "$BLOOM_TEST_ROOT/outside" "$SDCARD/miyoo/app/.tmp_update"
    run "$ROLLBACK"
    [ "$status" -eq 1 ]
    [ -L "$SDCARD/miyoo/app/.tmp_update" ]
    [ -d "$BLOOM_TEST_ROOT/outside" ]
}
