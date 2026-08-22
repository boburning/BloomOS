#!/usr/bin/env bats

@test "Bloom Shell exposes capability-filtered settings without legacy Tweaks" {
    model=/workspace/src/bloomShell/bloom_shell_settings.c
    grep -F 'BLOOM_SHELL_SETTINGS_DISPLAY_SECTION' "$model"
    grep -F 'BLOOM_SHELL_SETTINGS_NETWORK_SECTION' "$model"
    grep -F 'BLOOM_SHELL_SETTINGS_DEVELOPER_SECTION' "$model"
    grep -F 'requires_developer' "$model"
    grep -F 'lstat(DEVELOPER_MODE_PATH, &status)' /workspace/src/bloomShell/main.c
    grep -F 'S_ISREG(status.st_mode)' /workspace/src/bloomShell/main.c
    ! grep -F 'Tweaks' "$model"
}

@test "Settings is one flat sectioned surface with inline canonical adapters" {
    shell=/workspace/src/bloomShell/main.c
    model=/workspace/src/bloomShell/bloom_shell_settings.c

    grep -F 'bloom_shell_settings_first_selectable(&capabilities)' "$shell"
    grep -F 'settings_focus_step(&settings_focus, &capabilities' "$shell"
    grep -F '*held_repeats >= 6 ? 2 : 1' "$shell"
    grep -F 'settings_held_repeats = 0;' "$shell"
    grep -F 'BLOOM_SHELL_SETTINGS_ROW_SECTION ? sand' "$shell"
    grep -F 'settings_focus->window_start + row' "$shell"
    grep -F 'bloom_shell_quick_settings_adjust(' "$shell"
    grep -F 'BLOOM_CONTROLS_BINARY,' "$shell"
    grep -F 'BLOOM_NETWORK_BINARY);' "$shell"
    grep -F 'bloom_shell_support_export(BLOOMCTL_BINARY)' "$shell"
    grep -F 'bloom_shell_mute_toggle(&quick_values, BLOOM_CONTROLS_BINARY);' "$shell"
    grep -F '"Health: Support export %s"' "$shell"
    grep -F 'BLOOM_SHELL_SETTINGS_ROW_SLIDER' "$model"
    grep -F 'BLOOM_SHELL_SETTINGS_ROW_TOGGLE' "$model"
    grep -F 'BLOOM_SHELL_SETTINGS_ROW_DETAIL' "$model"
    ! grep -F 'settings_page' "$shell"
    ! grep -F 'bloom_shell_settings_page' "$model"

    render=$(sed -n '/static void draw(/,/^}/p' "$shell")
    [[ "$render" != *'system('* ]]
    [[ "$render" != *'popen('* ]]
}

@test "START owns a bounded quick settings state without render-path subprocesses" {
    shell=/workspace/src/bloomShell/main.c
    grep -F 'action == BLOOM_UI_ACTION_QUICK_SETTINGS' "$shell"
    grep -F 'quick_settings = !quick_settings;' "$shell"
    grep -F 'bloom_shell_quick_settings_count(&capabilities)' "$shell"
    grep -F 'bloom_shell_quick_settings_activate(' "$shell"
    grep -F 'destination = BLOOM_UI_DESTINATION_SETTINGS;' "$shell"
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

@test "first run is one recoverable welcome surface with fixed migration operations" {
    shell=/workspace/src/bloomShell/main.c
    adapter=/workspace/src/bloomShell/bloom_shell_settings.c

    grep -F 'bloom_shell_first_run_load(BLOOM_SETTINGS_BINARY, &first_run);' "$shell"
    grep -F 'int first_run_open = !safe_mode && !first_run.complete;' "$shell"
    grep -F 'int first_run_result = first_run.ready ? 0 : -1;' "$shell"
    grep -F 'if (!first_run.ready)' "$shell"
    grep -F '"Your library is ready."' "$shell"
    grep -F '"Games, saves, and settings stay in place."' "$shell"
    grep -F '"A Finish Setup   MENU Switcher   START Quick"' "$shell"
    grep -F 'else if (first_run_open)' "$shell"
    grep -F 'run_request(settings_path, "activate-bloom", NULL, NULL)' "$adapter"
    grep -F 'run_request(settings_path, "complete-first-run", NULL, NULL)' "$adapter"
}
