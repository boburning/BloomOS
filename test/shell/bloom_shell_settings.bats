#!/usr/bin/env bats

@test "Bloom Shell exposes capability-filtered settings without legacy Tweaks" {
    model=/workspace/src/bloomShell/bloom_shell_settings.c
    grep -F '"Display", "Audio", "Controls", "Gameplay"' "$model"
    grep -F 'return "Network";' "$model"
    grep -F 'return capabilities->developer_mode && row == 0 ? "Advanced" : NULL;' "$model"
    ! grep -F 'Tweaks' "$model"
}

@test "every Settings category opens a bounded detail page with canonical adapters" {
    shell=/workspace/src/bloomShell/main.c
    model=/workspace/src/bloomShell/bloom_shell_settings.c

    grep -F 'settings_page = bloom_shell_settings_page(&capabilities, settings_focus.selected);' "$shell"
    grep -F 'bloom_shell_settings_page_count(settings_page)' "$shell"
    grep -F 'settings_page = BLOOM_SHELL_SETTINGS_TOP;' "$shell"
    grep -F 'settings_focus->window_start + row' "$shell"
    grep -F 'bloom_shell_quick_settings_adjust(' "$shell"
    grep -F 'BLOOM_CONTROLS_BINARY,' "$shell"
    grep -F 'BLOOM_NETWORK_BINARY);' "$shell"
    grep -F 'bloom_shell_support_export(BLOOMCTL_BINARY)' "$shell"
    grep -F '"Health: Support export %s"' "$shell"
    grep -F 'case BLOOM_SHELL_SETTINGS_SYSTEM:' "$model"
    grep -F 'return 4;' "$model"
    grep -F '"A: Confirm"' "$model"
    grep -F '"MENU: GameSwitcher"' "$model"

    render=$(sed -n '/static void draw(/,/^}/p' "$shell")
    [[ "$render" != *'system('* ]]
    [[ "$render" != *'popen('* ]]
}

@test "START owns a bounded quick settings state without render-path subprocesses" {
    shell=/workspace/src/bloomShell/main.c
    grep -F 'action == BLOOM_UI_ACTION_QUICK_SETTINGS' "$shell"
    grep -F 'quick_settings = !quick_settings;' "$shell"
    grep -F 'bloom_shell_quick_settings_count(&capabilities)' "$shell"
    grep -F 'bloom_shell_quick_values_load(BLOOM_SETTINGS_BINARY, &quick_values);' "$shell"
    grep -F 'BLOOM_CONTROLS_BINARY' "$shell"
    grep -F 'BLOOM_NETWORK_BINARY' "$shell"
    grep -F 'action == BLOOM_UI_ACTION_FOCUS_RIGHT ? 1 : -1' "$shell"

    render=$(sed -n '/static void draw(/,/^}/p' "$shell")
    [[ "$render" != *'system('* ]]
    [[ "$render" != *'popen('* ]]
    [[ "$render" != *'fork('* ]]
    [[ "$render" != *'exec'* ]]
}
