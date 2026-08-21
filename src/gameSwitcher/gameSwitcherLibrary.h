#ifndef GAME_SWITCHER_LIBRARY_H
#define GAME_SWITCHER_LIBRARY_H

#include "../bloomGameId/bloom_game_id.h"

#include <stddef.h>

#define GAMESWITCHER_LIBRARY_TEXT_SIZE 1024

typedef struct {
    char game_id[BLOOM_GAME_ID_LENGTH + 1];
    char label[GAMESWITCHER_LIBRARY_TEXT_SIZE];
    char rom_path[GAMESWITCHER_LIBRARY_TEXT_SIZE];
    char image_path[GAMESWITCHER_LIBRARY_TEXT_SIZE];
    char launcher[GAMESWITCHER_LIBRARY_TEXT_SIZE];
} GameSwitcherLibraryRecent;

int gameswitcher_library_read_recents(const char *database_path, size_t limit,
                                      GameSwitcherLibraryRecent *recents, size_t capacity,
                                      size_t *count);
int gameswitcher_library_remove_recent(const char *database_path, const char *game_id);

#endif
