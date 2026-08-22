#ifndef GAME_SWITCHER_QUICK_SETTINGS_H__
#define GAME_SWITCHER_QUICK_SETTINGS_H__

#include "components/list.h"
#include "theme/theme.h"

#include "../bloomShell/bloom_shell_settings.h"
#include "gs_appState.h"

#define BLOOM_CONTROLS_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-controls"
#define BLOOM_NETWORK_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-network"
#define BLOOM_PLATFORM_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-platform"
#define BLOOM_SETTINGS_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-settings"
#define DEVICE_MODEL_PATH "/tmp/deviceModel"

static BloomShellCapabilities quick_capabilities = {0};
static BloomShellQuickValues quick_values = {0};

static void quickSettings_refresh(void)
{
    if (!appState.quick_settings_list._created)
        return;
    for (int row = 0; row < appState.quick_settings_list.item_count; ++row) {
        char label[STR_MAX] = {0};
        if (bloom_shell_quick_settings_format(&quick_capabilities, &quick_values, (size_t)row,
                                              label, sizeof(label)) == 0)
            snprintf(appState.quick_settings_list.items[row].label,
                     sizeof(appState.quick_settings_list.items[row].label), "%s", label);
    }
}

static void quickSettings_init(void)
{
    int model = 0;
    FILE *model_file = fopen(DEVICE_MODEL_PATH, "r");
    if (model_file == NULL || fscanf(model_file, "%d", &model) != 1 ||
        bloom_shell_capabilities_from_model(model, 0, &quick_capabilities) != 0) {
        if (model_file != NULL)
            fclose(model_file);
        return;
    }
    fclose(model_file);
    bloom_shell_quick_values_load(BLOOM_SETTINGS_BINARY, &quick_values);
    bloom_shell_quick_battery_load(BLOOM_PLATFORM_BINARY, &quick_values);
}

static void quickSettings_open(void)
{
    if (!appState.quick_settings_list._created) {
        size_t count = bloom_shell_quick_settings_count(&quick_capabilities);
        appState.quick_settings_list = list_create((int)count, LIST_SMALL);
        for (size_t row = 0; row < count; ++row) {
            ListItem item = {.item_type = ACTION, .action_id = (int)row};
            if (row + 1 == count)
                item.disable_a_btn = true;
            list_addItem(&appState.quick_settings_list, item);
        }
        appState.quick_settings_list.scroll_height = appState.quick_settings_list.item_count;
    }
    quickSettings_refresh();
    appState.quick_settings_open = true;
    appState.changed = true;
}

static void quickSettings_close(void)
{
    appState.quick_settings_open = false;
    appState.changed = true;
}

static void quickSettings_adjust(int direction)
{
    int row = appState.quick_settings_list.active_pos;
    if (bloom_shell_quick_settings_adjust(&quick_capabilities, &quick_values, (size_t)row,
                                          direction, BLOOM_CONTROLS_BINARY,
                                          BLOOM_NETWORK_BINARY) == 0) {
        quickSettings_refresh();
        appState.changed = true;
    }
}

static void quickSettings_activate(void)
{
    int row = appState.quick_settings_list.active_pos;
    if (row == 1) {
        if (bloom_shell_mute_toggle(&quick_values, BLOOM_CONTROLS_BINARY) == 0) {
            quickSettings_refresh();
            appState.changed = true;
        }
    }
    else if (quick_capabilities.wifi && row == 2)
        quickSettings_adjust(quick_values.wifi_enabled ? -1 : 1);
}

static void quickSettings_handle(KeyState *keystate)
{
    if (keystate[SW_BTN_B] == PRESSED) {
        quickSettings_close();
        return;
    }
    if (keystate[SW_BTN_DOWN] >= PRESSED) {
        if (list_keyDown(&appState.quick_settings_list, keystate[SW_BTN_DOWN] == REPEATING))
            appState.changed = true;
    }
    else if (keystate[SW_BTN_UP] >= PRESSED) {
        if (list_keyUp(&appState.quick_settings_list, keystate[SW_BTN_UP] == REPEATING))
            appState.changed = true;
    }
    else if (keystate[SW_BTN_LEFT] >= PRESSED)
        quickSettings_adjust(-1);
    else if (keystate[SW_BTN_RIGHT] >= PRESSED)
        quickSettings_adjust(1);
    else if (keystate[SW_BTN_A] == PRESSED)
        quickSettings_activate();
}

static void quickSettings_render(AppState *state)
{
    if (state->quick_settings_open)
        theme_renderPopMenu(screen, state->header_height, &state->quick_settings_list, NULL);
}

static void quickSettings_destroy(void)
{
    list_free(&appState.quick_settings_list);
}

#endif
