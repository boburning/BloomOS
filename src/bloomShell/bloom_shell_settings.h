#ifndef BLOOM_SHELL_SETTINGS_H
#define BLOOM_SHELL_SETTINGS_H

#include <stddef.h>

typedef struct {
    int wifi;
    int flip;
    int developer_mode;
} BloomShellCapabilities;

typedef struct {
    int ready;
    int generation;
    int brightness;
    int volume;
    int mute;
    int wifi_enabled;
    int battery_available;
    int battery_capacity_available;
    int battery_capacity;
    int battery_charging;
} BloomShellQuickValues;

typedef enum {
    BLOOM_SHELL_SETTINGS_TOP = -1,
    BLOOM_SHELL_SETTINGS_DISPLAY = 0,
    BLOOM_SHELL_SETTINGS_AUDIO,
    BLOOM_SHELL_SETTINGS_CONTROLS,
    BLOOM_SHELL_SETTINGS_GAMEPLAY,
    BLOOM_SHELL_SETTINGS_NETWORK,
    BLOOM_SHELL_SETTINGS_RETROACHIEVEMENTS,
    BLOOM_SHELL_SETTINGS_APPEARANCE,
    BLOOM_SHELL_SETTINGS_SYSTEM,
    BLOOM_SHELL_SETTINGS_ADVANCED,
} BloomShellSettingsPage;

int bloom_shell_capabilities_from_model(int model, int developer_mode,
                                        BloomShellCapabilities *capabilities);
size_t bloom_shell_settings_count(const BloomShellCapabilities *capabilities);
const char *bloom_shell_settings_label(const BloomShellCapabilities *capabilities, size_t row);
BloomShellSettingsPage bloom_shell_settings_page(const BloomShellCapabilities *capabilities,
                                                 size_t row);
size_t bloom_shell_settings_page_count(BloomShellSettingsPage page);
int bloom_shell_settings_page_format(BloomShellSettingsPage page,
                                     const BloomShellQuickValues *values, size_t row,
                                     char *label, size_t label_size);
size_t bloom_shell_quick_settings_count(const BloomShellCapabilities *capabilities);
const char *bloom_shell_quick_settings_label(const BloomShellCapabilities *capabilities, size_t row);
int bloom_shell_quick_values_parse(const char *json, BloomShellQuickValues *values);
int bloom_shell_quick_values_load(const char *settings_path, BloomShellQuickValues *values);
int bloom_shell_quick_battery_parse(const char *json, BloomShellQuickValues *values);
int bloom_shell_quick_battery_load(const char *platform_path, BloomShellQuickValues *values);
int bloom_shell_quick_settings_format(const BloomShellCapabilities *capabilities,
                                      const BloomShellQuickValues *values, size_t row,
                                      char *label, size_t label_size);
int bloom_shell_quick_settings_adjust(const BloomShellCapabilities *capabilities,
                                      BloomShellQuickValues *values, size_t row, int direction,
                                      const char *controls_path, const char *network_path);

#endif
