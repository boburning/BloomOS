#ifndef BLOOM_SHELL_STATUS_H
#define BLOOM_SHELL_STATUS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int ready;
    int healthy;
    int system_healthy;
    int update_healthy;
    int ra_healthy;
    int ra_enabled;
    uint64_t storage_free_kb;
    char update_phase[32];
    char ra_state[32];
} BloomShellStatus;

int bloom_shell_status_parse(const char *json, BloomShellStatus *status);
int bloom_shell_status_load(const char *bloomctl_path, BloomShellStatus *status);
int bloom_shell_support_export(const char *bloomctl_path);
int bloom_shell_update_confirm(const char *bloomctl_path);
int bloom_shell_update_rollback(const char *bloomctl_path);
int bloom_shell_settings_reset(const char *bloomctl_path);
int bloom_shell_status_label(const BloomShellStatus *status, size_t row, char *label,
                             size_t label_size);
int bloom_shell_storage_label(const BloomShellStatus *status, char *label, size_t label_size);

#endif
