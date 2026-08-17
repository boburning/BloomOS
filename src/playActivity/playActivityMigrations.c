#include "playActivityMigrations.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int table_exists(sqlite3 *database, const char *table, int *exists)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1", -1, &statement, NULL);
    if (result != SQLITE_OK)
        return result;
    sqlite3_bind_text(statement, 1, table, -1, SQLITE_STATIC);
    result = sqlite3_step(statement);
    *exists = result == SQLITE_ROW;
    if (result == SQLITE_ROW || result == SQLITE_DONE)
        result = SQLITE_OK;
    sqlite3_finalize(statement);
    return result;
}

static int column_exists(sqlite3 *database, const char *table, const char *column, int *exists)
{
    char sql[128];
    if (snprintf(sql, sizeof(sql), "PRAGMA table_info(%s)", table) >= (int)sizeof(sql))
        return SQLITE_TOOBIG;
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(database, sql, -1, &statement, NULL);
    if (result != SQLITE_OK)
        return result;
    *exists = 0;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(statement, 1);
        if (name != NULL && strcmp((const char *)name, column) == 0)
            *exists = 1;
    }
    if (result == SQLITE_DONE)
        result = SQLITE_OK;
    sqlite3_finalize(statement);
    return result;
}

int play_activity_schema_version(sqlite3 *database, int *version)
{
    if (database == NULL || version == NULL)
        return SQLITE_MISUSE;
    int exists = 0;
    int result = table_exists(database, "schema_version", &exists);
    if (result != SQLITE_OK || !exists) {
        if (result == SQLITE_OK)
            *version = 0;
        return result;
    }
    sqlite3_stmt *statement = NULL;
    result = sqlite3_prepare_v2(database, "SELECT version FROM schema_version", -1, &statement, NULL);
    if (result != SQLITE_OK)
        return result;
    if (sqlite3_step(statement) != SQLITE_ROW || sqlite3_column_type(statement, 0) != SQLITE_INTEGER) {
        sqlite3_finalize(statement);
        return SQLITE_CORRUPT;
    }
    *version = sqlite3_column_int(statement, 0);
    if (sqlite3_step(statement) != SQLITE_DONE)
        result = SQLITE_CORRUPT;
    sqlite3_finalize(statement);
    return result;
}

static int backup_database(sqlite3 *database, const char *path)
{
    struct stat status;
    if (path == NULL)
        return SQLITE_OK;
    if (stat(path, &status) == 0)
        return S_ISREG(status.st_mode) ? SQLITE_OK : SQLITE_CANTOPEN;
    sqlite3 *backup_database = NULL;
    int result = sqlite3_open_v2(path, &backup_database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (result != SQLITE_OK) {
        if (backup_database != NULL)
            sqlite3_close(backup_database);
        return result;
    }
    sqlite3_backup *backup = sqlite3_backup_init(backup_database, "main", database, "main");
    if (backup == NULL) {
        result = sqlite3_errcode(backup_database);
    }
    else {
        result = sqlite3_backup_step(backup, -1);
        if (result == SQLITE_DONE)
            result = SQLITE_OK;
        int finish = sqlite3_backup_finish(backup);
        if (result == SQLITE_OK)
            result = finish;
    }
    int close_result = sqlite3_close(backup_database);
    return result == SQLITE_OK ? close_result : result;
}

int play_activity_schema_migrate(sqlite3 *database, const char *backup_path)
{
    int version = 0;
    int result = play_activity_schema_version(database, &version);
    if (result != SQLITE_OK)
        return result;
    if (version == PLAY_ACTIVITY_SCHEMA_VERSION) {
        int game_id_exists = 0;
        result = column_exists(database, "rom", "game_id", &game_id_exists);
        if (result != SQLITE_OK || !game_id_exists)
            return result == SQLITE_OK ? SQLITE_CORRUPT : result;
        sqlite3_stmt *statement = NULL;
        result = sqlite3_prepare_v2(database, "SELECT COUNT(*) FROM migration_history WHERE version=1", -1,
                                    &statement, NULL);
        if (result == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 1)
            result = SQLITE_OK;
        else if (result == SQLITE_OK)
            result = SQLITE_CORRUPT;
        sqlite3_finalize(statement);
        return result;
    }
    if (version < 0 || version > PLAY_ACTIVITY_SCHEMA_VERSION)
        return SQLITE_MISMATCH;
    result = backup_database(database, backup_path);
    if (result != SQLITE_OK)
        return result;
    result = sqlite3_exec(database, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    if (result != SQLITE_OK)
        return result;
    result = sqlite3_exec(database,
                          "CREATE TABLE IF NOT EXISTS schema_version(version INTEGER NOT NULL);"
                          "INSERT INTO schema_version(version) SELECT 0 WHERE NOT EXISTS(SELECT 1 FROM schema_version);"
                          "CREATE TABLE IF NOT EXISTS migration_history("
                          "version INTEGER PRIMARY KEY, name TEXT NOT NULL, "
                          "applied_at INTEGER NOT NULL DEFAULT (strftime('%s','now')));",
                          NULL, NULL, NULL);
    int game_id_exists = 0;
    if (result == SQLITE_OK)
        result = column_exists(database, "rom", "game_id", &game_id_exists);
    if (result == SQLITE_OK && !game_id_exists)
        result = sqlite3_exec(database, "ALTER TABLE rom ADD COLUMN game_id TEXT", NULL, NULL, NULL);
    if (result == SQLITE_OK)
        result = sqlite3_exec(database,
                              "CREATE UNIQUE INDEX IF NOT EXISTS rom_game_id_index ON rom(game_id) WHERE game_id IS NOT NULL;"
                              "UPDATE schema_version SET version=1;"
                              "INSERT OR IGNORE INTO migration_history(version,name) VALUES(1,'canonical_game_id_column');",
                              NULL, NULL, NULL);
    if (result == SQLITE_OK)
        result = sqlite3_exec(database, "COMMIT", NULL, NULL, NULL);
    if (result != SQLITE_OK)
        sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
    return result;
}
