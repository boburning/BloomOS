#include "bloom_library_mutation.h"

#include "../bloomGameId/bloom_game_id.h"

static int execute(sqlite3 *database, const char *sql)
{
    return sqlite3_exec(database, sql, NULL, NULL, NULL);
}

int bloom_library_favorite_set(sqlite3 *database, const char *game_id, int favorite,
                               int *changed)
{
    if (database == NULL || game_id == NULL || changed == NULL ||
        (favorite != 0 && favorite != 1) || !bloom_game_id_valid(game_id))
        return SQLITE_MISUSE;
    *changed = 0;
    int result = execute(database, "BEGIN IMMEDIATE");
    sqlite3_stmt *statement = NULL;
    if (result == SQLITE_OK)
        result = sqlite3_prepare_v2(database,
                                    "SELECT EXISTS(SELECT 1 FROM games WHERE bloom_game_id=?1 "
                                    "AND present=1)",
                                    -1, &statement, NULL);
    if (result == SQLITE_OK)
        result = sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_STATIC);
    if (result == SQLITE_OK)
        result = sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 1
                     ? SQLITE_OK
                     : SQLITE_NOTFOUND;
    sqlite3_finalize(statement);
    statement = NULL;
    if (result == SQLITE_OK && favorite) {
        result = sqlite3_prepare_v2(
            database,
            "INSERT OR IGNORE INTO favorites(bloom_game_id,position) "
            "VALUES(?1,COALESCE((SELECT MAX(position)+1 FROM favorites),0))",
            -1, &statement, NULL);
        if (result == SQLITE_OK)
            result = sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_STATIC);
        if (result == SQLITE_OK)
            result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(database);
        if (result == SQLITE_OK)
            *changed = sqlite3_changes(database) != 0;
    }
    else if (result == SQLITE_OK) {
        result = sqlite3_prepare_v2(database, "SELECT position FROM favorites WHERE bloom_game_id=?1",
                                    -1, &statement, NULL);
        if (result == SQLITE_OK)
            result = sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_STATIC);
        int step = result == SQLITE_OK ? sqlite3_step(statement) : result;
        int position = step == SQLITE_ROW ? sqlite3_column_int(statement, 0) : -1;
        if (result == SQLITE_OK && step != SQLITE_ROW && step != SQLITE_DONE)
            result = sqlite3_errcode(database);
        sqlite3_finalize(statement);
        statement = NULL;
        if (result == SQLITE_OK && position >= 0) {
            result = sqlite3_prepare_v2(database, "DELETE FROM favorites WHERE bloom_game_id=?1", -1,
                                        &statement, NULL);
            if (result == SQLITE_OK)
                result = sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_STATIC);
            if (result == SQLITE_OK)
                result =
                    sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(database);
            if (result == SQLITE_OK)
                *changed = sqlite3_changes(database) != 0;
            sqlite3_finalize(statement);
            statement = NULL;
            if (result == SQLITE_OK && *changed) {
                char sql[192];
                sqlite3_snprintf(sizeof(sql), sql,
                                 "UPDATE favorites SET position=-position-1 WHERE position>%d",
                                 position);
                result = execute(database, sql);
                if (result == SQLITE_OK)
                    result = execute(database,
                                     "UPDATE favorites SET position=-position-2 WHERE position<0");
            }
        }
    }
    sqlite3_finalize(statement);
    if (result == SQLITE_OK)
        result = execute(database, "COMMIT");
    if (result != SQLITE_OK) {
        execute(database, "ROLLBACK");
        *changed = 0;
    }
    return result;
}
