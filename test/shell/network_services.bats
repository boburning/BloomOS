#!/usr/bin/env bats

setup() {
    export SERVICE=/workspace/static/build/.tmp_update/bin/bloom-network-services
    export BLOOM_NETWORK_SERVICES_ROOT="$BATS_TEST_TMPDIR/root"
    export SDCARD="$BLOOM_NETWORK_SERVICES_ROOT/mnt/SDCARD"
    export MOCK="$BATS_TEST_TMPDIR/mock"
    mkdir -p "$SDCARD/.bloom" "$SDCARD/.tmp_update/config" "$MOCK"
    printf '%s\n' 'ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAITestKey bloom-test' \
        >"$SDCARD/.bloom/authorized_keys"
    for name in platform ssh-helper sftp-server samba compat; do
        printf '#!/bin/sh\nexit 0\n' >"$MOCK/$name"
        chmod +x "$MOCK/$name"
    done
    printf '#!/bin/sh\n[ "$*" = "capability wifi" ] && printf true\n' >"$MOCK/platform"
    printf '#!/bin/sh\nprintf "%%s\\n" "$*" >>"$MOCK_LOG"\n' >"$MOCK/ssh-helper"
    printf '#!/bin/sh\nprintf "%%s\\n" "$*" >>"$MOCK_LOG"\n' >"$MOCK/compat"
    printf '#!/bin/sh\nprintf "%%s\\n" "$LD_LIBRARY_PATH" >"$MOCK_SFTP_ENV"\n' \
        >"$MOCK/sftp-server"
    chmod +x "$MOCK/platform" "$MOCK/ssh-helper" "$MOCK/compat"
    export MOCK_LOG="$BATS_TEST_TMPDIR/calls"
    export MOCK_SFTP_ENV="$BATS_TEST_TMPDIR/sftp-library-path"
    export BLOOM_PLATFORM_BIN="$MOCK/platform"
    export BLOOM_SSH_HELPER_BIN="$MOCK/ssh-helper"
    export BLOOM_SFTP_SERVER_BIN="$MOCK/sftp-server"
    export BLOOM_SAMBA_SERVER_BIN="$MOCK/samba"
    export BLOOM_NETWORK_COMPAT_BIN="$MOCK/compat"
}

@test "status reports bounded service capabilities and defaults" {
    run sh "$SERVICE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"ssh":{"available":true,"enabled":false}'* ]]
    [[ "$output" == *'"sftp":{"available":true,"enabled":false}'* ]]
    [[ "$output" == *'"samba":{"available":true,"enabled":false}'* ]]
}

@test "SSH and SFTP require explicit state and SSH key material" {
    run sh "$SERVICE" request ssh enable
    [ "$status" -eq 0 ]
    [ -f "$SDCARD/.bloom/network-services/ssh.enabled" ]
    grep -Fx 'start' "$MOCK_LOG"

    run sh "$SERVICE" request sftp enable
    [ "$status" -eq 0 ]
    [ -f "$SDCARD/.bloom/network-services/sftp.enabled" ]

    run sh "$SERVICE" request ssh disable
    [ "$status" -eq 0 ]
    [ -f "$SDCARD/.bloom/network-services/ssh.disabled" ]
    [ -f "$SDCARD/.bloom/network-services/sftp.disabled" ]
    [ ! -e "$SDCARD/.bloom/network-services/sftp.enabled" ]
    grep -Fx 'stop' "$MOCK_LOG"
}

@test "SSH enable fails closed without a valid public key" {
    printf 'not-a-key\n' >"$SDCARD/.bloom/authorized_keys"
    run sh "$SERVICE" request ssh enable
    [ "$status" -ne 0 ]
    [ ! -e "$SDCARD/.bloom/network-services/ssh.enabled" ]
}

@test "Samba preference is applied only through the compatibility boundary" {
    run sh "$SERVICE" request samba enable
    [ "$status" -eq 0 ]
    [ -f "$SDCARD/.tmp_update/config/.smbdState" ]
    grep -Fx 'services' "$MOCK_LOG"

    run sh "$SERVICE" request samba disable
    [ "$status" -eq 0 ]
    [ ! -e "$SDCARD/.tmp_update/config/.smbdState" ]
}

@test "SFTP gate refuses disabled transfers and execs the pinned subsystem when enabled" {
    gate=/workspace/static/build/.tmp_update/bin/bloom-sftp-gate
    run env BLOOM_ROOT="$BLOOM_NETWORK_SERVICES_ROOT" BLOOM_SFTP_SERVER="$MOCK/sftp-server" \
        sh "$gate"
    [ "$status" -ne 0 ]
    mkdir -p "$SDCARD/.bloom/network-services"
    : >"$SDCARD/.bloom/network-services/sftp.enabled"
    run env BLOOM_ROOT="$BLOOM_NETWORK_SERVICES_ROOT" BLOOM_SFTP_SERVER="$MOCK/sftp-server" \
        sh "$gate"
    [ "$status" -eq 0 ]
    grep -Fx "$SDCARD/.tmp_update/lib" "$MOCK_SFTP_ENV"
}
