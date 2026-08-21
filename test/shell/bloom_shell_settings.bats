#!/usr/bin/env bats

@test "Bloom Shell exposes capability-filtered settings without legacy Tweaks" {
    model=/workspace/src/bloomShell/bloom_shell_settings.c
    grep -F '"Display", "Audio", "Controls", "Gameplay"' "$model"
    grep -F 'return "Network";' "$model"
    grep -F 'return capabilities->developer_mode && row == 0 ? "Advanced" : NULL;' "$model"
    ! grep -F 'Tweaks' "$model"
}

@test "START owns a bounded quick settings state without render-path subprocesses" {
    shell=/workspace/src/bloomShell/main.c
    grep -F 'action == BLOOM_UI_ACTION_QUICK_SETTINGS' "$shell"
    grep -F 'quick_settings = !quick_settings;' "$shell"
    grep -F 'bloom_shell_quick_settings_count(&capabilities)' "$shell"

    render=$(sed -n '/static void draw(/,/^}/p' "$shell")
    [[ "$render" != *'system('* ]]
    [[ "$render" != *'popen('* ]]
    [[ "$render" != *'fork('* ]]
    [[ "$render" != *'exec'* ]]
}
