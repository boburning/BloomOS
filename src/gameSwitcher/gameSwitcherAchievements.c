#include "gameSwitcherAchievements.h"

#include "../bloomGameId/bloom_game_id.h"

#include <sqlite3/sqlite3.h>
#include <stdio.h>
#include <string.h>

static sqlite3 *cached_database = NULL;
static char cached_database_path[4096];

void gameswitcher_achievements_close(void)
{
    if (cached_database != NULL)
        sqlite3_close(cached_database);
    cached_database = NULL;
    cached_database_path[0] = '\0';
}

static void clear_result(GameSwitcherAchievements *achievements)
{
    achievements->has_ra_badge = 0;
    achievements->ra_game_id = 0;
    achievements->achievement_count = 0;
}

int gameswitcher_achievements_lookup(const char *database_path, const char *game_id,
                                     GameSwitcherAchievements *achievements, char *error, size_t error_size)
{
    if (achievements == NULL || database_path == NULL || !bloom_game_id_valid(game_id)) {
        if (error != NULL && error_size > 0)
            snprintf(error, error_size, "invalid achievement lookup");
        return -1;
    }
    clear_result(achievements);

    if (cached_database != NULL && strcmp(database_path, cached_database_path) != 0)
        gameswitcher_achievements_close();
    if (cached_database == NULL) {
        int result = sqlite3_open_v2(database_path, &cached_database, SQLITE_OPEN_READONLY, NULL);
        if (result != SQLITE_OK) {
            gameswitcher_achievements_close();
            return 0;
        }
        snprintf(cached_database_path, sizeof(cached_database_path), "%s", database_path);
    }

    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        cached_database,
        "SELECT ra_game_id,achievement_count FROM library_games "
        "WHERE bloom_game_id=?1 AND official_set=1 AND achievement_count>0 AND ra_game_id>0",
        -1, &statement, NULL);
    if (result != SQLITE_OK) {
        if (error != NULL && error_size > 0)
            snprintf(error, error_size, "achievement index is unavailable");
        gameswitcher_achievements_close();
        return -1;
    }

    sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_STATIC);
    int step = sqlite3_step(statement);
    if (step == SQLITE_ROW) {
        achievements->has_ra_badge = 1;
        achievements->ra_game_id = sqlite3_column_int(statement, 0);
        achievements->achievement_count = (unsigned long)sqlite3_column_int64(statement, 1);
    }
    else if (step != SQLITE_DONE) {
        if (error != NULL && error_size > 0)
            snprintf(error, error_size, "achievement lookup failed");
        sqlite3_finalize(statement);
        return -1;
    }

    sqlite3_finalize(statement);
    return 0;
}
