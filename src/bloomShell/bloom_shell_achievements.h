#ifndef BLOOM_SHELL_ACHIEVEMENTS_H
#define BLOOM_SHELL_ACHIEVEMENTS_H

#include "../bloomGameId/bloom_game_id.h"

#include <stddef.h>

typedef struct {
    char (*game_ids)[BLOOM_GAME_ID_LENGTH + 1];
    size_t count;
} BloomShellAchievementIndex;

int bloom_shell_achievements_load(BloomShellAchievementIndex *index,
                                  const char *database_path, size_t capacity);
int bloom_shell_achievements_contains(const BloomShellAchievementIndex *index,
                                      const char *game_id);
void bloom_shell_achievements_destroy(BloomShellAchievementIndex *index);

#endif
