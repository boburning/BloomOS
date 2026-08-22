#include "bloom_shell_root.h"

static const char *labels[BLOOM_SHELL_ROOT_COUNT] = {
    "Games",
    "Favorites",
    "Recent",
    "Apps",
    "Settings",
};

void bloom_shell_root_init(BloomShellRootState *state, int has_continue)
{
    if (state == NULL)
        return;
    state->selected = BLOOM_SHELL_ROOT_GAMES;
    state->has_continue = has_continue != 0;
    state->continue_focused = state->has_continue;
}

int bloom_shell_root_handle(BloomShellRootState *state, BloomUiAction action)
{
    if (state == NULL)
        return -1;
    if (action == BLOOM_UI_ACTION_FOCUS_DOWN && state->continue_focused) {
        state->continue_focused = 0;
        return 1;
    }
    if (action == BLOOM_UI_ACTION_FOCUS_UP && state->has_continue &&
        !state->continue_focused) {
        state->continue_focused = 1;
        return 1;
    }
    if (state->continue_focused)
        return 0;
    if (action == BLOOM_UI_ACTION_FOCUS_LEFT) {
        state->selected = state->selected == BLOOM_SHELL_ROOT_GAMES
                              ? BLOOM_SHELL_ROOT_SETTINGS
                              : (BloomShellRootDestination)(state->selected - 1);
        return 1;
    }
    if (action == BLOOM_UI_ACTION_FOCUS_RIGHT) {
        state->selected = state->selected == BLOOM_SHELL_ROOT_SETTINGS
                              ? BLOOM_SHELL_ROOT_GAMES
                              : (BloomShellRootDestination)(state->selected + 1);
        return 1;
    }
    return 0;
}

BloomUiDestination bloom_shell_root_open(const BloomShellRootState *state)
{
    if (state == NULL || state->selected < BLOOM_SHELL_ROOT_GAMES ||
        state->selected >= BLOOM_SHELL_ROOT_COUNT)
        return BLOOM_UI_DESTINATION_ROOT;
    return (BloomUiDestination)(BLOOM_UI_DESTINATION_GAMES + state->selected);
}

const char *bloom_shell_root_label(BloomShellRootDestination destination)
{
    return destination >= BLOOM_SHELL_ROOT_GAMES && destination < BLOOM_SHELL_ROOT_COUNT
               ? labels[destination]
               : NULL;
}
