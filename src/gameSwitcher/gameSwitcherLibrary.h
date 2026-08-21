#ifndef GAME_SWITCHER_LIBRARY_H
#define GAME_SWITCHER_LIBRARY_H

#include "../bloomGameId/bloom_game_id.h"

#include <stddef.h>

#define GAMESWITCHER_LIBRARY_TEXT_SIZE 512

typedef struct {
    char game_id[BLOOM_GAME_ID_LENGTH + 1];
    char system_id[GAMESWITCHER_LIBRARY_TEXT_SIZE];
    char label[GAMESWITCHER_LIBRARY_TEXT_SIZE];
    char rom_path[GAMESWITCHER_LIBRARY_TEXT_SIZE];
    char image_path[GAMESWITCHER_LIBRARY_TEXT_SIZE];
    char launcher[GAMESWITCHER_LIBRARY_TEXT_SIZE];
} GameSwitcherLibraryRecent;

int gameswitcher_library_read_recents(const char *database_path, size_t limit,
                                      GameSwitcherLibraryRecent *recents, size_t capacity,
                                      size_t *count);
int gameswitcher_library_remove_recent(const char *database_path, const char *game_id);
int gameswitcher_library_stage_recent(const char *database_path,
                                      const GameSwitcherLibraryRecent *recent,
                                      const char *request_path, const char *command_path,
                                      char *error, size_t error_size);

#endif
