#include "gameSwitcherLibrary.h"

#include "../bloomLaunch/bloom_launch.h"
#include "../bloomLibrary/bloom_library_query.h"

#include <sqlite3/sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int absolute_sd_path(char *output, size_t size, const char *prefix, const char *relative)
{
    if (relative == NULL || relative[0] == '\0' || relative[0] == '/' ||
        strstr(relative, "../") != NULL || strcmp(relative, "..") == 0 ||
        strstr(relative, "/..") != NULL)
        return -1;
    int length = snprintf(output, size, "%s%s", prefix, relative);
    return length > 0 && (size_t)length < size ? 0 : -1;
}

int gameswitcher_library_read_recents(const char *database_path, size_t limit,
                                      GameSwitcherLibraryRecent *recents, size_t capacity,
                                      size_t *count)
{
    if (database_path == NULL || database_path[0] == '\0' || limit == 0 ||
        limit > BLOOM_LIBRARY_QUERY_LIMIT_MAX || recents == NULL || capacity < limit ||
        count == NULL)
        return -1;
    *count = 0;
    sqlite3 *database = NULL;
    if (sqlite3_open_v2(database_path, &database, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK ||
        sqlite3_exec(database, "PRAGMA query_only=ON", NULL, NULL, NULL) != SQLITE_OK) {
        if (database != NULL)
            sqlite3_close(database);
        return -1;
    }
    BloomLibraryGame *games = calloc(limit, sizeof(*games));
    if (games == NULL) {
        sqlite3_close(database);
        return -1;
    }
    size_t game_count = 0;
    int sql = bloom_library_query_recents(database, NULL, limit, games, limit, &game_count);
    for (size_t index = 0; sql == SQLITE_OK && index < game_count; ++index) {
        GameSwitcherLibraryRecent *recent = &recents[index];
        memset(recent, 0, sizeof(*recent));
        if (snprintf(recent->game_id, sizeof(recent->game_id), "%s", games[index].bloom_game_id) >=
                (int)sizeof(recent->game_id) ||
            snprintf(recent->system_id, sizeof(recent->system_id), "%s", games[index].system_id) >=
                (int)sizeof(recent->system_id) ||
            snprintf(recent->label, sizeof(recent->label), "%s", games[index].display_title) >=
                (int)sizeof(recent->label) ||
            absolute_sd_path(recent->rom_path, sizeof(recent->rom_path), "/mnt/SDCARD/Roms/",
                             games[index].normalized_rom_path) != 0 ||
            absolute_sd_path(recent->launcher, sizeof(recent->launcher), "/mnt/SDCARD/",
                             games[index].launch_path) != 0 ||
            (games[index].image_path[0] != '\0' &&
             absolute_sd_path(recent->image_path, sizeof(recent->image_path), "/mnt/SDCARD/Roms/",
                              games[index].image_path) != 0))
            sql = SQLITE_CORRUPT;
        else
            ++*count;
    }
    free(games);
    sqlite3_close(database);
    return sql == SQLITE_OK ? 0 : -1;
}

static int promote_recent(const char *database_path, const char *game_id)
{
    sqlite3 *database = NULL;
    if (sqlite3_open_v2(database_path, &database, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        if (database != NULL)
            sqlite3_close(database);
        return -1;
    }
    sqlite3_busy_timeout(database, 5000);
    sqlite3_stmt *statement = NULL;
    int sql = sqlite3_exec(database, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    if (sql == SQLITE_OK)
        sql = sqlite3_prepare_v2(database,
                                 "UPDATE recents SET position=-position-2 WHERE bloom_game_id<>?1;",
                                 -1, &statement, NULL);
    if (sql == SQLITE_OK)
        sql = sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_STATIC);
    if (sql == SQLITE_OK)
        sql = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    sqlite3_finalize(statement);
    statement = NULL;
    if (sql == SQLITE_OK)
        sql = sqlite3_prepare_v2(database, "UPDATE recents SET position=0 WHERE bloom_game_id=?1;",
                                 -1, &statement, NULL);
    if (sql == SQLITE_OK)
        sql = sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_STATIC);
    if (sql == SQLITE_OK)
        sql = sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(database) == 1
                  ? SQLITE_OK
                  : SQLITE_NOTFOUND;
    sqlite3_finalize(statement);
    if (sql == SQLITE_OK)
        sql = sqlite3_exec(database,
                           "UPDATE recents SET position=-position-1 WHERE position<0;",
                           NULL, NULL, NULL);
    if (sql == SQLITE_OK)
        sql = sqlite3_exec(database, "COMMIT", NULL, NULL, NULL);
    else
        sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_close(database);
    return sql == SQLITE_OK ? 0 : -1;
}

int gameswitcher_library_stage_recent(const char *database_path,
                                      const GameSwitcherLibraryRecent *recent,
                                      const char *request_path, const char *command_path,
                                      char *error, size_t error_size)
{
    if (database_path == NULL || recent == NULL || request_path == NULL || command_path == NULL ||
        !bloom_game_id_valid(recent->game_id))
        return -1;
    unlink(request_path);
    if (bloom_launch_create_file(request_path, recent->game_id, recent->system_id,
                                 recent->rom_path, recent->launcher, "standalone", NULL, 1,
                                 error, error_size) != 0 ||
        bloom_launch_write_legacy(request_path, command_path, error, error_size) != 0) {
        unlink(request_path);
        return -1;
    }
    unlink(request_path);
    if (promote_recent(database_path, recent->game_id) != 0) {
        unlink(command_path);
        if (error != NULL && error_size > 0)
            snprintf(error, error_size, "cannot promote canonical recent");
        return -1;
    }
    return 0;
}

int gameswitcher_library_remove_recent(const char *database_path, const char *game_id)
{
    if (database_path == NULL || database_path[0] == '\0' || !bloom_game_id_valid(game_id))
        return -1;
    sqlite3 *database = NULL;
    if (sqlite3_open_v2(database_path, &database, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        if (database != NULL)
            sqlite3_close(database);
        return -1;
    }
    sqlite3_busy_timeout(database, 5000);
    sqlite3_stmt *statement = NULL;
    int sql = sqlite3_exec(database, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    if (sql == SQLITE_OK)
        sql = sqlite3_prepare_v2(database, "SELECT position FROM recents WHERE bloom_game_id=?1",
                                 -1, &statement, NULL);
    if (sql == SQLITE_OK)
        sql = sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_STATIC);
    int position = -1;
    if (sql == SQLITE_OK) {
        int step = sqlite3_step(statement);
        if (step == SQLITE_ROW)
            position = sqlite3_column_int(statement, 0);
        else
            sql = step == SQLITE_DONE ? SQLITE_NOTFOUND : step;
    }
    sqlite3_finalize(statement);
    if (sql == SQLITE_OK) {
        char *message = NULL;
        char query[512];
        snprintf(query, sizeof(query),
                 "DELETE FROM recents WHERE bloom_game_id='%s';"
                 "UPDATE recents SET position=-position-1 WHERE position>%d;"
                 "UPDATE recents SET position=-position-2 WHERE position<0;",
                 game_id, position);
        sql = sqlite3_exec(database, query, NULL, NULL, &message);
        sqlite3_free(message);
    }
    if (sql == SQLITE_OK)
        sql = sqlite3_exec(database, "COMMIT", NULL, NULL, NULL);
    else
        sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_close(database);
    return sql == SQLITE_OK ? 0 : -1;
}
