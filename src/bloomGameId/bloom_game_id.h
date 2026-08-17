#ifndef BLOOM_GAME_ID_H
#define BLOOM_GAME_ID_H

#include <stddef.h>

#define BLOOM_GAME_ID_LENGTH 78

int bloom_game_id_create(const char *system_id, const char *rom_path, char *game_id, size_t game_id_size,
                         char *relative_path, size_t relative_path_size, char *error, size_t error_size);
int bloom_game_id_valid(const char *game_id);

#endif
