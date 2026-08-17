#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export DEVICE_TOOL=/workspace/tools/bloom-device
    export BLOOM_TARGETS_FILE="$BLOOM_TEST_ROOT/targets.toml"
    export IDENTITY_FILE="$BLOOM_TEST_ROOT/id_ed25519"
    export KNOWN_HOSTS_FILE="$BLOOM_TEST_ROOT/known_hosts"
    : >"$IDENTITY_FILE"
    : >"$KNOWN_HOSTS_FILE"
    cat >"$BLOOM_TARGETS_FILE" <<EOF
[targets.test-plus]
host = "test-device"
user = "root"
port = 22
identity_file = "$IDENTITY_FILE"
known_hosts_file = "$KNOWN_HOSTS_FILE"
host_key_alias = "bloom-test-card"
expected_model = "mini_plus"
EOF
    mock_ssh_device 'mini_plus'
}

teardown() {
    teardown_bloom_fixture
}

mock_ssh_device() {
    local model="$1"
    cat >"$MOCK_BIN/ssh" <<EOF
#!/bin/sh
printf '%s\n' "ssh \$*" >>"$MOCK_LOG"
case "\${*}" in
  *bloomctl*) printf '%s\n' '{"model": "$model", "developer_mode": true}' ;;
  *bloom-game-smoke*) printf '%s\n' '{"schema":1,"status":"passed","system":"GB"}' ;;
  *dmesg*) printf '%s\n' 'bounded diagnostic fixture' ;;
esac
EOF
    chmod +x "$MOCK_BIN/ssh"
}

@test "info verifies the configured model and uses hardened SSH options" {
    run "$DEVICE_TOOL" info test-plus

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"model": "mini_plus"'
    grep -F -- '-o BatchMode=yes -o IdentitiesOnly=yes -o PreferredAuthentications=publickey' "$MOCK_LOG"
    grep -F -- '-o PasswordAuthentication=no -o KbdInteractiveAuthentication=no' "$MOCK_LOG"
    grep -F -- '-o StrictHostKeyChecking=yes' "$MOCK_LOG"
    grep -F -- "-o UserKnownHostsFile=$KNOWN_HOSTS_FILE" "$MOCK_LOG"
    grep -F -- '-o HostKeyAlias=bloom-test-card' "$MOCK_LOG"
}

@test "info refuses a different physical model" {
    mock_ssh_device 'mini_flip'

    run "$DEVICE_TOOL" info test-plus

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F "does not match expected_model"
}

@test "collect writes local diagnostics without embedding target secrets" {
    output_dir="$BLOOM_TEST_ROOT/results"

    run "$DEVICE_TOOL" collect test-plus "$output_dir"

    [ "$status" -eq 0 ]
    grep -F '"model": "mini_plus"' "$output_dir/device.json"
    grep -F 'bounded diagnostic fixture' "$output_dir/runtime.log"
}

@test "unknown targets and commands fail closed" {
    run "$DEVICE_TOOL" info missing
    [ "$status" -eq 1 ]

    run "$DEVICE_TOOL" deploy test-plus
    [ "$status" -eq 2 ]
}

@test "game-smoke sends only a validated system and encoded ROM path" {
    rom_path="/mnt/SDCARD/Roms/GB/Test Game.zip"
    encoded="$(printf '%s' "$rom_path" | base64 | tr -d '\r\n')"

    run "$DEVICE_TOOL" game-smoke test-plus GB "$rom_path" 12

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"status":"passed"'
    grep -F "bloom-game-smoke 'GB' '$encoded' '12'" "$MOCK_LOG"
}

@test "game-smoke rejects unsupported systems and external paths locally" {
    run "$DEVICE_TOOL" game-smoke test-plus N64 /mnt/SDCARD/Roms/N64/Test.z64
    [ "$status" -eq 1 ]

    run "$DEVICE_TOOL" game-smoke test-plus GB /tmp/Test.gb
    [ "$status" -eq 1 ]

    ! grep -Fq bloom-game-smoke "$MOCK_LOG"
}
