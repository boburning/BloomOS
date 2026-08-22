#ifndef BLOOM_SHELL_ROOT_H
#define BLOOM_SHELL_ROOT_H

#include "../bloomUi/bloom_ui_core.h"

typedef enum {
    BLOOM_SHELL_ROOT_GAMES = 0,
    BLOOM_SHELL_ROOT_FAVORITES,
    BLOOM_SHELL_ROOT_RECENT,
    BLOOM_SHELL_ROOT_APPS,
    BLOOM_SHELL_ROOT_SETTINGS,
    BLOOM_SHELL_ROOT_COUNT,
} BloomShellRootDestination;

typedef struct {
    BloomShellRootDestination selected;
    int continue_focused;
    int has_continue;
} BloomShellRootState;

void bloom_shell_root_init(BloomShellRootState *state, int has_continue);
int bloom_shell_root_handle(BloomShellRootState *state, BloomUiAction action);
BloomUiDestination bloom_shell_root_open(const BloomShellRootState *state);
const char *bloom_shell_root_label(BloomShellRootDestination destination);

#endif
