#!/usr/bin/env bats

load 'support/test_helper'

@test "legacy SSH binaries and password toggles are excluded" {
    [ ! -e /workspace/static/build/.tmp_update/bin/dropbear ]
    [ ! -e /workspace/static/build/.tmp_update/bin/sftp-server ]
    ! grep -F 'ftp | telnet | http | ssh | smbd' /workspace/static/build/.tmp_update/script/network/update_networking.sh
    ! grep -F '.authsshState' /workspace/src/tweaks/network.h
    ! grep -F 'menu_ssh' /workspace/src/tweaks/network.h
}

setup() {
    setup_bloom_fixture
    export SSH_HELPER=/workspace/static/build/.tmp_update/bin/bloom-dev-ssh
    export BLOOM_ROOT="$BLOOM_TEST_ROOT"
    mkdir -p "$BLOOM_TEST_ROOT/tmp" "$SDCARD/.bloom"
    cat >"$MOCK_BIN/pgrep" <<'EOF'
#!/bin/sh
exit 1
EOF
    mock_command killall
    cat >"$MOCK_BIN/dropbear" <<'EOF'
#!/bin/sh
printf '%s\n' "$0 $*" >>"$MOCK_LOG"
if [ "${1:-}" = dropbearkey ]; then
    : >"$SDCARD/.tmp_update/etc/dropbear/dropbear_ed25519_host_key"
elif [ "${1:-}" = dropbear ]; then
    sleep 30
fi
EOF
    chmod +x "$MOCK_BIN/dropbear"
    export BLOOM_DROPBEAR="$MOCK_BIN/dropbear"
    mock_command ifconfig
    chmod +x "$MOCK_BIN/pgrep"
}

teardown() {
    if [ -r "$BLOOM_TEST_ROOT/tmp/bloom-dev-ssh.pid" ]; then
        kill "$(cat "$BLOOM_TEST_ROOT/tmp/bloom-dev-ssh.pid")" 2>/dev/null || true
    fi
    teardown_bloom_fixture
}

enable_flip() {
    : >"$SDCARD/.bloom-dev"
    printf '285\n' >"$BLOOM_TEST_ROOT/tmp/deviceModel"
}

install_valid_key() {
    printf '%s\n' 'ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIBloomDeveloperFixture bloom-test' \
        >"$SDCARD/.bloom/authorized_keys"
}

@test "developer SSH is disabled without the explicit flag" {
    printf '285\n' >"$BLOOM_TEST_ROOT/tmp/deviceModel"
    install_valid_key

    run "$SSH_HELPER" start

    [ "$status" -eq 0 ]
    [ "$(cat "$BLOOM_TEST_ROOT/tmp/bloom-dev-ssh.state")" = disabled ]
    ! grep -Fq "$MOCK_BIN/dropbear " "$MOCK_LOG"
}

@test "developer SSH fails closed without a valid public key" {
    enable_flip
    printf '%s\n' 'not-a-public-key' >"$SDCARD/.bloom/authorized_keys"

    run "$SSH_HELPER" start

    [ "$status" -eq 1 ]
    [ "$(cat "$BLOOM_TEST_ROOT/tmp/bloom-dev-ssh.state")" = missing_or_invalid_key ]
    ! grep -Fq "$MOCK_BIN/dropbear " "$MOCK_LOG"
}

@test "Flip developer SSH starts Dropbear in key-only mode" {
    enable_flip
    install_valid_key

    run "$SSH_HELPER" start

    [ "$status" -eq 0 ]
    [ "$(cat "$BLOOM_TEST_ROOT/tmp/bloom-dev-ssh.state")" = running_key_only ]
    grep -F 'dropbear dropbear -F -r' "$MOCK_LOG"
    grep -F -- '-D ' "$MOCK_LOG"
    grep -F -- '-p 22' "$MOCK_LOG"
    cmp "$SDCARD/.bloom/authorized_keys" "$BLOOM_TEST_ROOT/home/root/.ssh/authorized_keys"
}

@test "Plus is supported and original Mini remains harmless" {
    : >"$SDCARD/.bloom-dev"
    install_valid_key
    printf '354\n' >"$BLOOM_TEST_ROOT/tmp/deviceModel"
    run "$SSH_HELPER" start
    [ "$status" -eq 0 ]
    grep -F 'dropbear dropbear -F -r' "$MOCK_LOG"

    : >"$MOCK_LOG"
    printf '283\n' >"$BLOOM_TEST_ROOT/tmp/deviceModel"
    run "$SSH_HELPER" start
    [ "$status" -eq 0 ]
    [ "$(cat "$BLOOM_TEST_ROOT/tmp/bloom-dev-ssh.state")" = unsupported ]
    ! grep -Fq dropbear "$MOCK_LOG"
}

@test "network initialization delegates developer mode to the secure helper" {
    grep -F 'if [ -f /mnt/SDCARD/.bloom-dev ]' /workspace/static/build/.tmp_update/script/network/update_networking.sh
    grep -F '$sysdir/bin/bloom-dev-ssh start' /workspace/static/build/.tmp_update/script/network/update_networking.sh
}

@test "read-only firmware home has a checked bind-mount fallback" {
    grep -F 'mount -o bind "$DEV_HOME" "$ROOT/home"' "$SSH_HELPER"
    grep -F 'state runtime_key_failed' "$SSH_HELPER"
}

@test "startup is reported only while the tracked server remains alive" {
    enable_flip
    install_valid_key
    cat >"$MOCK_BIN/dropbear" <<'EOF'
#!/bin/sh
printf '%s\n' "$0 $*" >>"$MOCK_LOG"
if [ "${1:-}" = dropbearkey ]; then
    : >"$SDCARD/.tmp_update/etc/dropbear/dropbear_ed25519_host_key"
    exit 0
fi
exit 1
EOF
    chmod +x "$MOCK_BIN/dropbear"

    run "$SSH_HELPER" start

    [ "$status" -eq 1 ]
    [ "$(cat "$BLOOM_TEST_ROOT/tmp/bloom-dev-ssh.state")" = startup_failed ]
    [ ! -e "$BLOOM_TEST_ROOT/tmp/bloom-dev-ssh.pid" ]
}
