#!/usr/bin/env bats

load 'support/test_helper'

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
    mock_command dropbear
    mock_command ifconfig
    chmod +x "$MOCK_BIN/pgrep"
}

teardown() { teardown_bloom_fixture; }

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
    ! grep -Fq dropbear "$MOCK_LOG"
}

@test "developer SSH fails closed without a valid public key" {
    enable_flip
    printf '%s\n' 'not-a-public-key' >"$SDCARD/.bloom/authorized_keys"

    run "$SSH_HELPER" start

    [ "$status" -eq 1 ]
    [ "$(cat "$BLOOM_TEST_ROOT/tmp/bloom-dev-ssh.state")" = missing_or_invalid_key ]
    ! grep -Fq dropbear "$MOCK_LOG"
}

@test "Flip developer SSH starts Dropbear in key-only mode" {
    enable_flip
    install_valid_key

    run "$SSH_HELPER" start

    [ "$status" -eq 0 ]
    [ "$(cat "$BLOOM_TEST_ROOT/tmp/bloom-dev-ssh.state")" = running_key_only ]
    grep -F 'dropbear -E -R -s -g -p 22' "$MOCK_LOG"
    cmp "$SDCARD/.bloom/authorized_keys" "$BLOOM_TEST_ROOT/home/root/.ssh/authorized_keys"
}

@test "Plus is supported and original Mini remains harmless" {
    : >"$SDCARD/.bloom-dev"
    install_valid_key
    printf '354\n' >"$BLOOM_TEST_ROOT/tmp/deviceModel"
    run "$SSH_HELPER" start
    [ "$status" -eq 0 ]
    grep -F 'dropbear -E -R -s -g -p 22' "$MOCK_LOG"

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
