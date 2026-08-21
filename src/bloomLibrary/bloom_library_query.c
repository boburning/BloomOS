#include "bloom_library_query.h"

#include "../bloomGameId/bloom_game_id.h"

#include <string.h>

static int valid_system_id(const char *value)
{
    if (value == NULL)
        return 1;
    if (value[0] == '\0' || strlen(value) >= 64)
        return 0;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor)
        if (!((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= '0' && *cursor <= '9') ||
              *cursor == '_' || *cursor == '-'))
            return 0;
    return 1;
}

static int copy_column(sqlite3_stmt *statement, int column, char *output, size_t size,
                       int optional)
{
    if (sqlite3_column_type(statement, column) == SQLITE_NULL && optional) {
        output[0] = '\0';
        return SQLITE_OK;
    }
    const unsigned char *value = sqlite3_column_text(statement, column);
    size_t length = value == NULL ? 0 : strlen((const char *)value);
    if (length == 0 || length >= size)
        return SQLITE_CORRUPT;
    memcpy(output, value, length + 1);
    return SQLITE_OK;
}

static int cursor_exists(sqlite3 *database, const char *system_id, const char *cursor)
{
    if (cursor == NULL)
        return SQLITE_OK;
    sqlite3_stmt *statement = NULL;
    int sql = sqlite3_prepare_v2(
        database,
        "SELECT 1 FROM games WHERE bloom_game_id=?1 AND present=1 AND "
        "(?2 IS NULL OR system_id=?2)",
        -1, &statement, NULL);
    if (sql == SQLITE_OK)
        sql = sqlite3_bind_text(statement, 1, cursor, -1, SQLITE_STATIC);
    if (sql == SQLITE_OK) {
        if (system_id == NULL)
            sql = sqlite3_bind_null(statement, 2);
        else
            sql = sqlite3_bind_text(statement, 2, system_id, -1, SQLITE_STATIC);
    }
    if (sql == SQLITE_OK) {
        int step = sqlite3_step(statement);
        sql = step == SQLITE_ROW ? SQLITE_OK : step == SQLITE_DONE ? SQLITE_NOTFOUND
                                                                   : step;
    }
    sqlite3_finalize(statement);
    return sql;
}

int bloom_library_query_games(sqlite3 *database, const char *system_id, const char *after_game_id,
                              size_t limit, BloomLibraryGame *games, size_t games_capacity,
                              BloomLibraryGamePage *page)
{
    if (database == NULL || !valid_system_id(system_id) ||
        (after_game_id != NULL && !bloom_game_id_valid(after_game_id)) || limit == 0 ||
        limit > BLOOM_LIBRARY_QUERY_LIMIT_MAX || games == NULL || games_capacity < limit ||
        page == NULL)
        return SQLITE_MISUSE;
    memset(page, 0, sizeof(*page));
    int sql = cursor_exists(database, system_id, after_game_id);
    if (sql != SQLITE_OK)
        return sql;
    sqlite3_stmt *statement = NULL;
    const char *global_sql =
        "SELECT bloom_game_id,system_id,normalized_rom_path,display_title,image_path,file_size,"
        "file_mtime,systems.launch_path FROM games JOIN systems USING(system_id) WHERE games.present=1 "
        "AND systems.present=1 AND (?1 IS NULL OR "
        "(sort_title,bloom_game_id)>(SELECT sort_title,bloom_game_id FROM games WHERE "
        "bloom_game_id=?1)) ORDER BY sort_title,bloom_game_id LIMIT ?2";
    const char *system_sql =
        "SELECT bloom_game_id,system_id,normalized_rom_path,display_title,image_path,file_size,"
        "file_mtime,systems.launch_path FROM games JOIN systems USING(system_id) WHERE system_id=?1 "
        "AND games.present=1 AND systems.present=1 AND (?2 IS NULL OR "
        "(sort_title,bloom_game_id)>(SELECT sort_title,bloom_game_id FROM games WHERE "
        "bloom_game_id=?2)) ORDER BY sort_title,bloom_game_id LIMIT ?3";
    sql = sqlite3_prepare_v2(database, system_id == NULL ? global_sql : system_sql, -1, &statement,
                             NULL);
    int cursor_parameter = system_id == NULL ? 1 : 2;
    int limit_parameter = system_id == NULL ? 2 : 3;
    if (sql == SQLITE_OK && system_id != NULL)
        sql = sqlite3_bind_text(statement, 1, system_id, -1, SQLITE_STATIC);
    if (sql == SQLITE_OK)
        sql = after_game_id == NULL ? sqlite3_bind_null(statement, cursor_parameter)
                                    : sqlite3_bind_text(statement, cursor_parameter, after_game_id,
                                                        -1, SQLITE_STATIC);
    if (sql == SQLITE_OK)
        sql = sqlite3_bind_int(statement, limit_parameter, (int)limit + 1);
    int step = SQLITE_DONE;
    while (sql == SQLITE_OK && (step = sqlite3_step(statement)) == SQLITE_ROW) {
        if (page->count == limit) {
            page->has_more = 1;
            break;
        }
        BloomLibraryGame *game = &games[page->count];
        memset(game, 0, sizeof(*game));
        if ((sql = copy_column(statement, 0, game->bloom_game_id,
                               sizeof(game->bloom_game_id), 0)) != SQLITE_OK ||
            (sql = copy_column(statement, 1, game->system_id, sizeof(game->system_id), 0)) !=
                SQLITE_OK ||
            (sql = copy_column(statement, 2, game->normalized_rom_path,
                               sizeof(game->normalized_rom_path), 0)) != SQLITE_OK ||
            (sql = copy_column(statement, 3, game->display_title,
                               sizeof(game->display_title), 0)) != SQLITE_OK ||
            (sql = copy_column(statement, 4, game->image_path, sizeof(game->image_path), 1)) !=
                SQLITE_OK)
            break;
        game->file_size = sqlite3_column_int64(statement, 5);
        game->file_mtime = sqlite3_column_int64(statement, 6);
        if ((sql = copy_column(statement, 7, game->launch_path, sizeof(game->launch_path), 0)) !=
            SQLITE_OK)
            break;
        ++page->count;
    }
    if (sql == SQLITE_OK && step != SQLITE_DONE && !page->has_more)
        sql = step;
    sqlite3_finalize(statement);
    if (sql == SQLITE_OK && page->has_more && page->count > 0)
        memcpy(page->next_cursor, games[page->count - 1].bloom_game_id,
               sizeof(page->next_cursor));
    return sql;
}

static int query_ordered(sqlite3 *database, const char *table, const char *system_id,
                         size_t limit, BloomLibraryGame *games, size_t games_capacity,
                         size_t *count)
{
    if (database == NULL || !valid_system_id(system_id) || limit == 0 ||
        limit > BLOOM_LIBRARY_QUERY_LIMIT_MAX || games == NULL || games_capacity < limit ||
        count == NULL)
        return SQLITE_MISUSE;
    *count = 0;
    sqlite3_stmt *statement = NULL;
    const char *recents_sql =
        "SELECT games.bloom_game_id,games.system_id,games.normalized_rom_path,"
        "games.display_title,games.image_path,games.file_size,games.file_mtime,systems.launch_path "
        "FROM recents JOIN games USING(bloom_game_id) JOIN systems USING(system_id) "
        "WHERE games.present=1 AND systems.present=1 AND (?1 IS NULL OR games.system_id=?1) "
        "ORDER BY recents.position LIMIT ?2";
    const char *favorites_sql =
        "SELECT games.bloom_game_id,games.system_id,games.normalized_rom_path,"
        "games.display_title,games.image_path,games.file_size,games.file_mtime,systems.launch_path "
        "FROM favorites JOIN games USING(bloom_game_id) JOIN systems USING(system_id) "
        "WHERE games.present=1 AND systems.present=1 AND (?1 IS NULL OR games.system_id=?1) "
        "ORDER BY favorites.position LIMIT ?2";
    int sql = sqlite3_prepare_v2(database, strcmp(table, "recents") == 0 ? recents_sql : favorites_sql,
                                 -1, &statement, NULL);
    if (sql == SQLITE_OK)
        sql = system_id == NULL ? sqlite3_bind_null(statement, 1)
                                : sqlite3_bind_text(statement, 1, system_id, -1, SQLITE_STATIC);
    if (sql == SQLITE_OK)
        sql = sqlite3_bind_int(statement, 2, (int)limit);
    int step = SQLITE_DONE;
    while (sql == SQLITE_OK && (step = sqlite3_step(statement)) == SQLITE_ROW) {
        BloomLibraryGame *game = &games[*count];
        memset(game, 0, sizeof(*game));
        if ((sql = copy_column(statement, 0, game->bloom_game_id,
                               sizeof(game->bloom_game_id), 0)) != SQLITE_OK ||
            (sql = copy_column(statement, 1, game->system_id, sizeof(game->system_id), 0)) !=
                SQLITE_OK ||
            (sql = copy_column(statement, 2, game->normalized_rom_path,
                               sizeof(game->normalized_rom_path), 0)) != SQLITE_OK ||
            (sql = copy_column(statement, 3, game->display_title,
                               sizeof(game->display_title), 0)) != SQLITE_OK ||
            (sql = copy_column(statement, 4, game->image_path, sizeof(game->image_path), 1)) !=
                SQLITE_OK)
            break;
        game->file_size = sqlite3_column_int64(statement, 5);
        game->file_mtime = sqlite3_column_int64(statement, 6);
        if ((sql = copy_column(statement, 7, game->launch_path, sizeof(game->launch_path), 0)) !=
            SQLITE_OK)
            break;
        ++*count;
    }
    if (sql == SQLITE_OK && step != SQLITE_DONE)
        sql = step;
    sqlite3_finalize(statement);
    return sql;
}

int bloom_library_query_recents(sqlite3 *database, const char *system_id, size_t limit,
                                BloomLibraryGame *games, size_t games_capacity, size_t *count)
{
    return query_ordered(database, "recents", system_id, limit, games, games_capacity, count);
}

int bloom_library_query_favorites(sqlite3 *database, const char *system_id, size_t limit,
                                  BloomLibraryGame *games, size_t games_capacity, size_t *count)
{
    return query_ordered(database, "favorites", system_id, limit, games, games_capacity, count);
}

int bloom_library_query_apps(sqlite3 *database, size_t limit, BloomLibraryApp *apps,
                             size_t apps_capacity, size_t *count)
{
    if (database == NULL || limit == 0 || limit > BLOOM_LIBRARY_QUERY_LIMIT_MAX || apps == NULL ||
        apps_capacity < limit || count == NULL)
        return SQLITE_MISUSE;
    *count = 0;
    sqlite3_stmt *statement = NULL;
    int sql = sqlite3_prepare_v2(
        database,
        "SELECT app_id,label,launch_path,icon_path,compatibility FROM apps WHERE present=1 "
        "ORDER BY label,app_id LIMIT ?1",
        -1, &statement, NULL);
    if (sql == SQLITE_OK)
        sql = sqlite3_bind_int(statement, 1, (int)limit);
    int step = SQLITE_DONE;
    while (sql == SQLITE_OK && (step = sqlite3_step(statement)) == SQLITE_ROW) {
        BloomLibraryApp *app = &apps[*count];
        memset(app, 0, sizeof(*app));
        if ((sql = copy_column(statement, 0, app->app_id, sizeof(app->app_id), 0)) != SQLITE_OK ||
            (sql = copy_column(statement, 1, app->label, sizeof(app->label), 0)) != SQLITE_OK ||
            (sql = copy_column(statement, 2, app->launch_path, sizeof(app->launch_path), 0)) !=
                SQLITE_OK ||
            (sql = copy_column(statement, 3, app->icon_path, sizeof(app->icon_path), 1)) != SQLITE_OK ||
            (sql = copy_column(statement, 4, app->compatibility, sizeof(app->compatibility), 0)) != SQLITE_OK)
            break;
        ++*count;
    }
    if (sql == SQLITE_OK && step != SQLITE_DONE)
        sql = step;
    sqlite3_finalize(statement);
    return sql;
}
