#include "bloom_shell_achievements.h"

#include <sqlite3/sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compare_game_ids(const void *left, const void *right)
{
    return strcmp((const char *)left, (const char *)right);
}

void bloom_shell_achievements_destroy(BloomShellAchievementIndex *index)
{
    if (index == NULL)
        return;
    free(index->game_ids);
    memset(index, 0, sizeof(*index));
}

int bloom_shell_achievements_load(BloomShellAchievementIndex *index,
                                  const char *database_path, size_t capacity)
{
    if (index == NULL || database_path == NULL || capacity == 0)
        return -1;
    bloom_shell_achievements_destroy(index);

    sqlite3 *database = NULL;
    if (sqlite3_open_v2(database_path, &database, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (database != NULL)
            sqlite3_close(database);
        return 0;
    }
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database,
        "SELECT bloom_game_id FROM library_games "
        "WHERE status='identified' AND official_set=1 AND achievement_count>0 AND ra_game_id>0 "
        "ORDER BY bloom_game_id LIMIT ?1",
        -1, &statement, NULL);
    if (result != SQLITE_OK) {
        sqlite3_close(database);
        return 0;
    }
    sqlite3_bind_int64(statement, 1, (sqlite3_int64)capacity);
    index->game_ids = calloc(capacity, sizeof(*index->game_ids));
    if (index->game_ids == NULL) {
        sqlite3_finalize(statement);
        sqlite3_close(database);
        return -1;
    }
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        if (index->count >= capacity)
            continue;
        const unsigned char *game_id = sqlite3_column_text(statement, 0);
        if (game_id != NULL && bloom_game_id_valid((const char *)game_id)) {
            snprintf(index->game_ids[index->count], sizeof(index->game_ids[index->count]), "%s",
                     (const char *)game_id);
            index->count++;
        }
    }
    sqlite3_finalize(statement);
    sqlite3_close(database);
    if (result != SQLITE_DONE) {
        bloom_shell_achievements_destroy(index);
        return 0;
    }
    qsort(index->game_ids, index->count, sizeof(*index->game_ids), compare_game_ids);
    return 0;
}

int bloom_shell_achievements_contains(const BloomShellAchievementIndex *index,
                                      const char *game_id)
{
    if (index == NULL || game_id == NULL || !bloom_game_id_valid(game_id) || index->count == 0)
        return 0;
    return bsearch(game_id, index->game_ids, index->count, sizeof(*index->game_ids),
                   compare_game_ids) != NULL;
}
