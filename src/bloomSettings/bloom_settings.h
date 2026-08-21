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

int bloom_settings_status(const char *settings_path, int *schema, char *source, size_t source_size,
                          char *authority, size_t authority_size, char *error, size_t error_size);
int bloom_settings_import_onion(const char *onion_system_path, const char *onion_config_root,
                                const char *settings_path, const char *snapshot_path,
                                BloomSettingsImportResult *result, char *error, size_t error_size);
int bloom_settings_sync_onion(const char *onion_system_path, const char *onion_config_root,
                              const char *settings_path, BloomSettingsSyncResult *result,
                              char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
