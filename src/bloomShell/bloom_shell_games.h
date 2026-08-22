#ifndef BLOOM_SHELL_GAMES_H
#define BLOOM_SHELL_GAMES_H

#include "../bloomLibrary/bloom_library_query.h"
#include "../bloomUi/bloom_ui_core.h"

#define BLOOM_SHELL_SYSTEM_CAPACITY 64

typedef struct {
    BloomLibrarySystem system;
    size_t game_offset;
    BloomUiFocus focus;
    char core[128];
} BloomShellGamesSystem;

typedef struct {
    BloomShellGamesSystem systems[BLOOM_SHELL_SYSTEM_CAPACITY];
    size_t system_count;
    size_t selected_system;
    int held_direction;
    unsigned int held_repeats;
} BloomShellGamesBrowser;

void bloom_shell_games_init(BloomShellGamesBrowser *browser);
int bloom_shell_games_add(BloomShellGamesBrowser *browser, const BloomLibrarySystem *system,
                          size_t game_offset, size_t game_count, const char *core);
int bloom_shell_games_switch(BloomShellGamesBrowser *browser, int direction);
int bloom_shell_games_step(BloomShellGamesBrowser *browser, int direction, size_t visible_rows);
void bloom_shell_games_release(BloomShellGamesBrowser *browser);
const BloomShellGamesSystem *bloom_shell_games_current(const BloomShellGamesBrowser *browser);
BloomShellGamesSystem *bloom_shell_games_current_mutable(BloomShellGamesBrowser *browser);
const char *bloom_shell_games_default_core(const char *system_id);

#endif
