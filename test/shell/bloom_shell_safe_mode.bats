#!/usr/bin/env bats

@test "Safe Mode is one flat Bloom-owned recovery surface" {
    shell=/workspace/src/bloomShell/main.c
    model=/workspace/src/bloomShell/bloom_shell_safe_mode.c

    grep -F 'bloom_shell_safe_mode_enabled(getenv("BLOOM_SAFE_MODE"))' "$shell"
    grep -F '"Safe Mode"' "$shell"
    grep -F 'BLOOM_SHELL_SAFE_MODE_GAMES' "$shell"
    grep -F 'bloom_shell_support_export(BLOOMCTL_BINARY)' "$shell"
    grep -F 'bloom_shell_update_rollback(BLOOMCTL_BINARY)' "$shell"
    grep -F 'RESTART_NORMAL_EXIT' "$shell"
    grep -F '"Browse Games"' "$model"
    grep -F '"Restart Normally"' "$model"
}

@test "Safe Mode keeps global buttons and recovery actions bounded" {
    shell=/workspace/src/bloomShell/main.c
    status=/workspace/src/bloomShell/bloom_shell_status.c

    grep -F '"A Open   MENU Switcher   START Quick"' "$shell"
    grep -F 'bloom_ui_dialog_init(&rollback_dialog, 2, 0, 1)' "$shell"
    grep -F 'execl(bloomctl_path, bloomctl_path, "update", "rollback"' "$status"
    ! grep -F 'system(' "$status"
}

@test "runtime suppresses resume and optional achievement work until explicit restart" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh

    grep -F 'if bloom_shell_safe_mode_pending; then' "$runtime"
    grep -F 'Bloom Safe Mode pending; suppressing automatic resume' "$runtime"
    grep -F 'Bloom Safe Mode pending; suppressing custom startup scripts' "$runtime"
    grep -F 'export BLOOM_RA_FORCE_DISABLED=1' "$runtime"
    grep -F '"$shell_guard" clear-safe-mode' "$runtime"
    grep -F 'Bloom Safe Mode cleared; restarting normally' "$runtime"
}
