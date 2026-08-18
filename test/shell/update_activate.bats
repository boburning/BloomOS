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
    export BLOOM_MOUNT_BIN="$BLOOM_TEST_ROOT/mock-mount"
    export BLOOM_UMOUNT_BIN="$BLOOM_TEST_ROOT/mock-umount"
    export VERSION=1.2.3
    mkdir -p \
        "$BLOOM_UPDATE_ROOT/candidates/$VERSION/miyoo/app/.tmp_update" \
        "$BLOOM_UPDATE_ROOT/candidates/$VERSION/RetroArch" \
        "$BLOOM_UPDATE_ROOT/staged/1.2.2"
    printf 'trigger\n' >"$BLOOM_UPDATE_ROOT/candidates/$VERSION/miyoo/app/MainUI"
    printf 'core\n' >"$BLOOM_UPDATE_ROOT/candidates/$VERSION/miyoo/app/.tmp_update/onion.pak"
    printf 'retroarch\n' >"$BLOOM_UPDATE_ROOT/candidates/$VERSION/RetroArch/retroarch.pak"
    mkdir -p "$SDCARD/.tmp_update/etc/dropbear"
    printf 'stable-host-key\n' >"$SDCARD/.tmp_update/etc/dropbear/dropbear_ed25519_host_key"
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
    cat >"$BLOOM_MOUNT_BIN" <<EOF
#!/bin/sh
printf '/dev/mmcblk0p1 on $SDCARD/miyoo/app/MainUI type vfat (rw)\n'
EOF
    cat >"$BLOOM_UMOUNT_BIN" <<'EOF'
#!/bin/sh
printf 'umount:%s:%s\n' "$1" "$2" >>"$BLOOM_TEST_ROOT/calls"
EOF
    chmod +x "$BLOOM_MOUNT_BIN" "$BLOOM_UMOUNT_BIN"
}

teardown() { teardown_bloom_fixture; }

@test "publishes installer payloads before the boot trigger" {
    mkdir -p "$SDCARD/miyoo/app"
    printf 'installed MainUI binary\n' >"$SDCARD/miyoo/app/MainUI"

    run "$ACTIVATE" "$VERSION"

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"status":"activation_pending"'
    [ -f "$SDCARD/miyoo/app/.tmp_update/onion.pak" ]
    [ -f "$SDCARD/RetroArch/retroarch.pak" ]
    [ -f "$SDCARD/miyoo/app/MainUI" ]
    [ "$(sed -n '1p' "$BLOOM_TEST_ROOT/calls")" = 'create:pre-update' ]
    [ "$(sed -n '2p' "$BLOOM_TEST_ROOT/calls")" = 'activation-start:1.2.3:snapshot-1' ]
    [ "$(sed -n '3p' "$BLOOM_TEST_ROOT/calls")" = "umount:-l:$SDCARD/miyoo/app/MainUI" ]
    [ "$(cat "$SDCARD/.bloom/ssh/dropbear_ed25519_host_key")" = stable-host-key ]
    [ "$(stat -c '%a' "$SDCARD/.bloom/ssh/dropbear_ed25519_host_key")" = 600 ]
}

@test "refuses activation without a retained known-good release" {
    rm "$BLOOM_UPDATE_ROOT/known-good.json"

    run "$ACTIVATE" "$VERSION"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'no prior known-good release'
    [ ! -e "$SDCARD/miyoo/app/MainUI" ]
    [ ! -e "$BLOOM_TEST_ROOT/calls" ]
}

@test "bloomctl exposes guarded activation after arm" {
    mock_activate="$BLOOM_TEST_ROOT/mock-activate"
    cat >"$mock_activate" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >"$BLOOM_TEST_ROOT/activate-call"
EOF
    chmod +x "$mock_activate"

    run env \
        BLOOM_UPDATE_ACTIVATE_BIN="$mock_activate" \
        BLOOM_TEST_ROOT="$BLOOM_TEST_ROOT" \
        /workspace/static/build/.tmp_update/bin/bloomctl update activate 1.2.3

    [ "$status" -eq 0 ]
    grep -Fx '1.2.3' "$BLOOM_TEST_ROOT/activate-call"
}

@test "refuses to replace any pending installer" {
    mkdir -p "$SDCARD/miyoo/app"
    printf '#!/bin/sh\nexisting\n' >"$SDCARD/miyoo/app/MainUI"

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
    grep -F 'DEFAULT_UPDATE_ROOT="$SD_ROOT/.bloom/update"' \
        /workspace/static/build/.tmp_update/bin/bloom-update-state
    grep -F 'UPDATE_ROOT="${BLOOM_UPDATE_ROOT:-$DEFAULT_UPDATE_ROOT}"' \
        /workspace/static/build/.tmp_update/bin/bloom-update-state
    grep -F 'UPDATE_ROOT="${BLOOM_UPDATE_ROOT:-$SD_ROOT/.bloom/update}"' "$ACTIVATE"
}
