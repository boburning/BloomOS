#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export SHUTDOWN_HELPER=/workspace/static/build/.tmp_update/bin/bloom-shutdown
    mkdir -p "$BLOOM_TEST_ROOT/proc"
    printf 'Filename Type Size Used Priority\n%s/cachefile file 131068 0 -2\n' "$SDCARD" >"$BLOOM_TEST_ROOT/proc/swaps"
    for command_name in sync swapoff mount poweroff-test; do
        mock_command "$command_name"
    done
}

teardown() { teardown_bloom_fixture; }

@test "shutdown disables SD swap and remounts FAT read-only before poweroff" {
    run env BLOOM_ROOT="$BLOOM_TEST_ROOT" BLOOM_SHUTDOWN_COMMAND=poweroff-test "$SHUTDOWN_HELPER"

    [ "$status" -eq 0 ]
    swap_line="$(grep -n '/swapoff ' "$MOCK_LOG" | cut -d: -f1)"
    mount_line="$(grep -n '/mount -o remount,ro ' "$MOCK_LOG" | cut -d: -f1)"
    power_line="$(grep -n '/poweroff-test ' "$MOCK_LOG" | cut -d: -f1)"
    [ "$swap_line" -lt "$mount_line" ]
    [ "$mount_line" -lt "$power_line" ]
}

@test "all BloomOS shutdown paths use the clean helper" {
    grep -F 'system("bloom-shutdown")' /workspace/src/keymon/keymon.c
    grep -F 'system("bloom-shutdown; sleep 10")' /workspace/src/chargingState/chargingState.c
    grep -F '        bloom-shutdown' /workspace/static/build/.tmp_update/runtime.sh
}
