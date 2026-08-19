#ifndef GAME_SWITCHER_ACHIEVEMENTS_H
#define GAME_SWITCHER_ACHIEVEMENTS_H

#include <stddef.h>

typedef struct {
    int has_ra_badge;
    int ra_game_id;
    unsigned long achievement_count;
} GameSwitcherAchievements;

int gameswitcher_achievements_lookup(const char *database_path, const char *game_id,
                                     GameSwitcherAchievements *achievements, char *error, size_t error_size);
void gameswitcher_achievements_close(void);

#endif
