#include "bloom_shell_safe_mode.h"

#include <string.h>

static const char *labels[BLOOM_SHELL_SAFE_MODE_ROW_COUNT] = {
    "Browse Games",
    "System Health",
    "Export Support File",
    "Restore Previous Version",
    "Restart Normally",
};

int bloom_shell_safe_mode_enabled(const char *value)
{
    return value != NULL && strcmp(value, "true") == 0;
}

const char *bloom_shell_safe_mode_label(BloomShellSafeModeRow row)
{
    return row >= BLOOM_SHELL_SAFE_MODE_GAMES && row < BLOOM_SHELL_SAFE_MODE_ROW_COUNT
               ? labels[row]
               : NULL;
}
