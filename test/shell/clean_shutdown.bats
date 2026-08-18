#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export SHUTDOWN_HELPER=/workspace/static/build/.tmp_update/bin/shutdown
}

teardown() { teardown_bloom_fixture; }

@test "shutdown disables SD swap and remounts FAT read-only before poweroff" {
    settings_line="$(grep -n 'mv -f /mnt/SDCARD/system.json' "$SHUTDOWN_HELPER" | cut -d: -f1)"
    swap_line="$(grep -n 'swapoff /mnt/SDCARD/cachefile' "$SHUTDOWN_HELPER" | cut -d: -f1)"
    mount_line="$(grep -n 'remount_sd_read_only' "$SHUTDOWN_HELPER" | tail -n 1 | cut -d: -f1)"
    power_line="$(grep -n '/sbin/poweroff' "$SHUTDOWN_HELPER" | cut -d: -f1)"
    [ "$settings_line" -lt "$swap_line" ]
    [ "$swap_line" -lt "$mount_line" ]
    [ "$mount_line" -lt "$power_line" ]
}

@test "shutdown verifies the read-only remount and records developer telemetry" {
    grep -F 'remount_ro_attempt_1=' "$SHUTDOWN_HELPER"
    grep -F 'remount_ro_attempt_2=' "$SHUTDOWN_HELPER"
    grep -F 'remount_ro_final=' "$SHUTDOWN_HELPER"
    grep -F 'umount_final=' "$SHUTDOWN_HELPER"
    grep -F 'SHUTDOWN_LOG_ENABLED=1' "$SHUTDOWN_HELPER"
    grep -F 'shutdown_mode=${1:-poweroff}' "$SHUTDOWN_HELPER"
}

@test "reboot falls back to the direct kernel syscall after clean unmount" {
    recursive_unmount_line="$(grep -n 'umount -r /mnt/SDCARD' "$SHUTDOWN_HELPER" | cut -d: -f1)"
    reboot_call_line="$(grep -n '^\t\trun_reboot' "$SHUTDOWN_HELPER" | head -n 1 | cut -d: -f1)"
    init_reboot_line="$(grep -n '^\t/sbin/reboot$' "$SHUTDOWN_HELPER" | cut -d: -f1)"
    kernel_reboot_line="$(grep -n '^\t/sbin/reboot -f$' "$SHUTDOWN_HELPER" | cut -d: -f1)"
    [ "$recursive_unmount_line" -lt "$reboot_call_line" ]
    [ "$init_reboot_line" -lt "$kernel_reboot_line" ]
    grep -F 'reboot_command=init' "$SHUTDOWN_HELPER"
    grep -F 'reboot_command=kernel' "$SHUTDOWN_HELPER"
}

@test "all BloomOS shutdown paths use the detached clean shutdown script" {
    grep -F 'system("shutdown")' /workspace/src/keymon/keymon.c
    grep -F 'system("shutdown; sleep 10")' /workspace/src/chargingState/chargingState.c
    grep -F '        shutdown' /workspace/static/build/.tmp_update/runtime.sh
}
