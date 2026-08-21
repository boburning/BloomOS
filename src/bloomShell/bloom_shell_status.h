#ifndef BLOOM_SHELL_STATUS_H
#define BLOOM_SHELL_STATUS_H

#include <stddef.h>

typedef struct {
    int ready;
    int healthy;
    int system_healthy;
    int update_healthy;
    int ra_healthy;
    int ra_enabled;
    char update_phase[32];
    char ra_state[32];
} BloomShellStatus;

int bloom_shell_status_parse(const char *json, BloomShellStatus *status);
int bloom_shell_status_load(const char *bloomctl_path, BloomShellStatus *status);
int bloom_shell_status_label(const BloomShellStatus *status, size_t row, char *label,
                             size_t label_size);

#endif
