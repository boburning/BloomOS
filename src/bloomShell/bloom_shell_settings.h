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
    int network_services_ready;
    int ssh_available;
    int ssh_enabled;
    int sftp_available;
    int sftp_enabled;
    int samba_available;
    int samba_enabled;
    int battery_available;
    int battery_capacity_available;
    int battery_capacity;
    int battery_charging;
} BloomShellQuickValues;

typedef struct {
    int ready;
    int complete;
} BloomShellFirstRun;

typedef struct {
    int ready;
    int configured;
    int enabled;
    int authenticated;
    int hardcore;
    int offline_casual;
} BloomShellRaValues;

typedef enum {
    BLOOM_SHELL_SETTINGS_ROW_SECTION,
    BLOOM_SHELL_SETTINGS_ROW_SLIDER,
    BLOOM_SHELL_SETTINGS_ROW_TOGGLE,
    BLOOM_SHELL_SETTINGS_ROW_ENUM,
    BLOOM_SHELL_SETTINGS_ROW_DETAIL,
    BLOOM_SHELL_SETTINGS_ROW_ACTION,
    BLOOM_SHELL_SETTINGS_ROW_READ_ONLY,
} BloomShellSettingsRowKind;

typedef enum {
    BLOOM_SHELL_SETTINGS_DISPLAY_SECTION,
    BLOOM_SHELL_SETTINGS_BRIGHTNESS,
    BLOOM_SHELL_SETTINGS_THEME,
    BLOOM_SHELL_SETTINGS_AUDIO_SECTION,
    BLOOM_SHELL_SETTINGS_VOLUME,
    BLOOM_SHELL_SETTINGS_MUTE,
    BLOOM_SHELL_SETTINGS_CONTROLS_SECTION,
    BLOOM_SHELL_SETTINGS_BUTTON_GRAMMAR,
    BLOOM_SHELL_SETTINGS_LAUNCH_BEHAVIOR,
    BLOOM_SHELL_SETTINGS_NETWORK_SECTION,
    BLOOM_SHELL_SETTINGS_WIFI,
    BLOOM_SHELL_SETTINGS_SSH,
    BLOOM_SHELL_SETTINGS_SFTP,
    BLOOM_SHELL_SETTINGS_SAMBA,
    BLOOM_SHELL_SETTINGS_RA_SECTION,
    BLOOM_SHELL_SETTINGS_RA_ENABLED,
    BLOOM_SHELL_SETTINGS_RA_ACCOUNT,
    BLOOM_SHELL_SETTINGS_RA_MODE,
    BLOOM_SHELL_SETTINGS_RA_OFFLINE,
    BLOOM_SHELL_SETTINGS_RA_CONNECTION,
    BLOOM_SHELL_SETTINGS_SYSTEM_SECTION,
    BLOOM_SHELL_SETTINGS_UPDATE,
    BLOOM_SHELL_SETTINGS_STORAGE,
    BLOOM_SHELL_SETTINGS_HEALTH,
    BLOOM_SHELL_SETTINGS_ABOUT,
    BLOOM_SHELL_SETTINGS_DEVELOPER_SECTION,
    BLOOM_SHELL_SETTINGS_DEVELOPER_MODE,
    BLOOM_SHELL_SETTINGS_DIAGNOSTICS,
} BloomShellSettingsRowId;

typedef struct {
    BloomShellSettingsRowId id;
    BloomShellSettingsRowKind kind;
    const char *label;
} BloomShellSettingsRow;

int bloom_shell_capabilities_from_model(int model, int developer_mode,
                                        BloomShellCapabilities *capabilities);
size_t bloom_shell_settings_count(const BloomShellCapabilities *capabilities);
int bloom_shell_settings_row(const BloomShellCapabilities *capabilities, size_t row,
                             BloomShellSettingsRow *settings_row);
int bloom_shell_settings_row_format(const BloomShellCapabilities *capabilities,
                                    const BloomShellQuickValues *values, size_t row, char *label,
                                    size_t label_size);
int bloom_shell_settings_row_selectable(const BloomShellCapabilities *capabilities, size_t row);
size_t bloom_shell_settings_first_selectable(const BloomShellCapabilities *capabilities);
size_t bloom_shell_settings_next_selectable(const BloomShellCapabilities *capabilities,
                                            size_t row, int direction);
size_t bloom_shell_quick_settings_count(const BloomShellCapabilities *capabilities);
const char *bloom_shell_quick_settings_label(const BloomShellCapabilities *capabilities, size_t row);
int bloom_shell_quick_values_parse(const char *json, BloomShellQuickValues *values);
int bloom_shell_quick_values_load(const char *settings_path, BloomShellQuickValues *values);
int bloom_shell_network_services_parse(const char *json, BloomShellQuickValues *values);
int bloom_shell_network_services_load(const char *services_path, BloomShellQuickValues *values);
int bloom_shell_network_service_change(BloomShellQuickValues *values,
                                       BloomShellSettingsRowId id, int enabled,
                                       const char *services_path);
int bloom_shell_quick_battery_parse(const char *json, BloomShellQuickValues *values);
int bloom_shell_quick_battery_load(const char *platform_path, BloomShellQuickValues *values);
int bloom_shell_quick_settings_format(const BloomShellCapabilities *capabilities,
                                      const BloomShellQuickValues *values, size_t row,
                                      char *label, size_t label_size);
int bloom_shell_quick_settings_adjust(const BloomShellCapabilities *capabilities,
                                      BloomShellQuickValues *values, size_t row, int direction,
                                      const char *controls_path, const char *network_path);
int bloom_shell_mute_toggle(BloomShellQuickValues *values, const char *controls_path);
int bloom_shell_quick_settings_activate(const BloomShellCapabilities *capabilities,
                                        BloomShellQuickValues *values, size_t row,
                                        const char *controls_path, const char *network_path,
                                        int *open_settings);
int bloom_shell_first_run_parse(const char *json, BloomShellFirstRun *first_run);
int bloom_shell_first_run_load(const char *settings_path, BloomShellFirstRun *first_run);
int bloom_shell_first_run_finish(const char *settings_path, BloomShellFirstRun *first_run,
                                 BloomShellQuickValues *values);
int bloom_shell_ra_values_parse(const char *json, BloomShellRaValues *values);
int bloom_shell_ra_values_load(const char *ra_path, BloomShellRaValues *values);
int bloom_shell_ra_settings_format(const BloomShellRaValues *values,
                                   BloomShellSettingsRowId id, char *label,
                                   size_t label_size);
int bloom_shell_ra_settings_change(BloomShellRaValues *values,
                                   BloomShellSettingsRowId id, int direction,
                                   const char *ra_path);

#endif
