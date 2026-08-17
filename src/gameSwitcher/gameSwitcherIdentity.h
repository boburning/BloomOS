#ifndef GAME_SWITCHER_IDENTITY_H
#define GAME_SWITCHER_IDENTITY_H

#include <stddef.h>

int gameswitcher_game_id(const char *launcher, const char *rom_path, char *game_id, size_t game_id_size);
int gameswitcher_romscreen_path(const char *game_id, char *path, size_t path_size);

#endif
