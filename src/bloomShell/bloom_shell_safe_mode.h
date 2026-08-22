#ifndef BLOOM_SHELL_SAFE_MODE_H
#define BLOOM_SHELL_SAFE_MODE_H

#include <stddef.h>

typedef enum {
    BLOOM_SHELL_SAFE_MODE_GAMES = 0,
    BLOOM_SHELL_SAFE_MODE_HEALTH,
    BLOOM_SHELL_SAFE_MODE_EXPORT_SUPPORT,
    BLOOM_SHELL_SAFE_MODE_ROLLBACK,
    BLOOM_SHELL_SAFE_MODE_RESET_SETTINGS,
    BLOOM_SHELL_SAFE_MODE_RESTART_NORMAL,
    BLOOM_SHELL_SAFE_MODE_ROW_COUNT,
} BloomShellSafeModeRow;

int bloom_shell_safe_mode_enabled(const char *value);
const char *bloom_shell_safe_mode_label(BloomShellSafeModeRow row);

#endif
