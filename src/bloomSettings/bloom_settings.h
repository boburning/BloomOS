#ifndef BLOOM_SETTINGS_H
#define BLOOM_SETTINGS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLOOM_SETTINGS_SCHEMA 1

typedef struct {
    int schema;
    int imported;
    int used_defaults;
    int legacy_snapshot_written;
} BloomSettingsImportResult;

typedef struct {
    int changed;
    int generation;
} BloomSettingsSyncResult;

typedef struct {
    int changed;
    int rolled_back;
    int generation;
} BloomSettingsAuthorityResult;

typedef struct {
    int changed;
    int generation;
    int materialized;
} BloomSettingsMutationResult;

#define BLOOM_SETTINGS_TEXT_MAX 256

typedef struct {
    int generation;
    char authority[16];
    int volume;
    int mute;
    int background_music_volume;
    int brightness;
    int wifi_enabled;
    int sleep_minutes;
    int luminance;
    int hue;
    int saturation;
    int contrast;
    int audio_fix;
    int vibration;
    int pwm_frequency;
    char language[BLOOM_SETTINGS_TEXT_MAX];
    char theme[BLOOM_SETTINGS_TEXT_MAX];
    int font_size;
    int background_music_muted;
    int show_recents;
    int show_expert;
    int blue_light_enabled;
    int blue_light_scheduled;
    int blue_light_level;
    int blue_light_rgb;
    char blue_light_start[16];
    char blue_light_end[16];
    int recording_indicator;
    int recording_hotkey;
    int recording_countdown;
    int startup_auto_resume;
    int menu_button_haptics;
    int disable_standby;
    int logging;
    int low_battery_warn_at;
    int low_battery_autosave_at;
    int startup_tab;
    int startup_application;
    int time_skip_hours;
    char layout[BLOOM_SETTINGS_TEXT_MAX];
    int mainui_single_press;
    int mainui_long_press;
    int mainui_double_press;
    int ingame_single_press;
    int ingame_long_press;
    int ingame_double_press;
    char mainui_button_x[BLOOM_SETTINGS_TEXT_MAX];
    char mainui_button_y[BLOOM_SETTINGS_TEXT_MAX];
} BloomSettingsValues;

int bloom_settings_status(const char *settings_path, int *schema, char *source, size_t source_size,
                          char *authority, size_t authority_size, char *error, size_t error_size);
int bloom_settings_import_onion(const char *onion_system_path, const char *onion_config_root,
                                const char *settings_path, const char *snapshot_path,
                                BloomSettingsImportResult *result, char *error, size_t error_size);
int bloom_settings_sync_onion(const char *onion_system_path, const char *onion_config_root,
                              const char *settings_path, BloomSettingsSyncResult *result,
                              char *error, size_t error_size);
int bloom_settings_reconcile_onion(const char *onion_system_path, const char *onion_config_root,
                                   const char *settings_path, BloomSettingsSyncResult *result,
                                   char *error, size_t error_size);

int bloom_settings_read_values(const char *settings_path, BloomSettingsValues *values, char *error,
                               size_t error_size);

int bloom_settings_materialize_onion(const char *settings_path, const char *onion_system_path,
                                     const char *onion_config_root, char *error,
                                     size_t error_size);

int bloom_settings_activate(const char *settings_path, const char *onion_system_path,
                            const char *onion_config_root, BloomSettingsAuthorityResult *result,
                            char *error, size_t error_size);
int bloom_settings_rollback_authority(const char *settings_path,
                                      BloomSettingsAuthorityResult *result, char *error,
                                      size_t error_size);
int bloom_settings_set(const char *settings_path, const char *onion_system_path,
                       const char *onion_config_root, const char *field, const char *value,
                       BloomSettingsMutationResult *result, char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
