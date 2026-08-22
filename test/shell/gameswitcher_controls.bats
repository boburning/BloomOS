#!/usr/bin/env bats

@test "GameSwitcher uses the global MENU START X and B grammar" {
    keys=/workspace/src/gameSwitcher/gs_keystate.h
    menu=/workspace/src/gameSwitcher/gs_popMenu.h

    grep -F 'keystate[SW_BTN_MENU] == PRESSED' "$keys"
    grep -F 'state->quit = true;' "$keys"
    grep -F 'keystate[SW_BTN_START] == PRESSED' "$keys"
    grep -F 'quickSettings_open();' "$keys"
    grep -F 'keystate[SW_BTN_X] == PRESSED && game_list_len != 0' "$keys"
    grep -F 'state->pop_menu_open = !state->pop_menu_open;' "$keys"
    grep -F 'keystate[SW_BTN_A] == PRESSED && game_list_len > 0' "$keys"
    grep -F 'if (game_list_len == 0)' "$keys"
    grep -F 'state->exit_to_menu = true;' "$keys"
    grep -F 'Remove from Recent' "$menu"
    grep -F 'action_confirmRemove' "$menu"
}

@test "GameSwitcher Quick Settings uses canonical bounded adapters" {
    quick=/workspace/src/gameSwitcher/gs_quickSettings.h

    grep -F 'bloom_shell_quick_values_load(BLOOM_SETTINGS_BINARY' "$quick"
    grep -F 'bloom_shell_quick_battery_load(BLOOM_PLATFORM_BINARY' "$quick"
    grep -F 'bloom_shell_quick_settings_adjust(' "$quick"
    grep -F 'bloom_shell_mute_toggle(' "$quick"
    grep -F 'BLOOM_CONTROLS_BINARY' "$quick"
    grep -F 'BLOOM_NETWORK_BINARY' "$quick"
}

@test "GameSwitcher has one presentation and no inherited navigation quirks" {
    keys=/workspace/src/gameSwitcher/gs_keystate.h
    main=/workspace/src/gameSwitcher/gameSwitcher.c

    ! grep -F 'settings_setBrightness' "$keys"
    ! grep -E 'SW_BTN_(L1|L2|R1|R2)' "$keys"
    ! grep -F 'SW_BTN_SELECT' "$keys"
    ! grep -F 'SW_BTN_Y' "$keys"
    ! grep -F 'button_y_repeat' "$keys"
    ! grep -F 'action_toggleHeader' "$keys"
    ! grep -F 'action_confirmRemove(state)' "$keys"
    grep -F 'appState.view_mode = VIEW_NORMAL;' "$main"
    ! grep -F 'gameSwitcher/minimal' "$main"
    ! grep -F 'renderLegend(&appState)' "$main"
}
