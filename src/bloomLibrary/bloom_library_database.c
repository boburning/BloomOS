#include "bloom_library_database.h"
#include "../common/database/sqlite_config.h"

#include <string.h>
#include <sys/stat.h>

static int table_exists(sqlite3 *database, const char *name, int *exists)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1", -1, &statement,
        NULL);
    if (result != SQLITE_OK)
        return result;
    sqlite3_bind_text(statement, 1, name, -1, SQLITE_STATIC);
    result = sqlite3_step(statement);
    *exists = result == SQLITE_ROW;
    if (result == SQLITE_ROW || result == SQLITE_DONE)
        result = SQLITE_OK;
    sqlite3_finalize(statement);
    return result;
}

static int database_version(sqlite3 *database, int *version)
{
    int exists = 0;
    int result = table_exists(database, "schema_version", &exists);
    if (result != SQLITE_OK || !exists) {
        if (result == SQLITE_OK)
            *version = 0;
        return result;
    }
    sqlite3_stmt *statement = NULL;
    result = sqlite3_prepare_v2(database, "SELECT version FROM schema_version", -1, &statement,
                                NULL);
    if (result != SQLITE_OK)
        return result;
    if (sqlite3_step(statement) != SQLITE_ROW ||
        sqlite3_column_type(statement, 0) != SQLITE_INTEGER)
        result = SQLITE_CORRUPT;
    else {
        *version = sqlite3_column_int(statement, 0);
        if (sqlite3_step(statement) != SQLITE_DONE)
            result = SQLITE_CORRUPT;
    }
    sqlite3_finalize(statement);
    return result;
}

static int index_exists(sqlite3 *database, const char *name)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database, "SELECT 1 FROM sqlite_master WHERE type='index' AND name=?1", -1, &statement,
        NULL);
    if (result == SQLITE_OK)
        result = sqlite3_bind_text(statement, 1, name, -1, SQLITE_STATIC);
    if (result == SQLITE_OK) {
        int step = sqlite3_step(statement);
        result = step == SQLITE_ROW ? SQLITE_OK : step == SQLITE_DONE ? SQLITE_CORRUPT
                                                                      : step;
    }
    sqlite3_finalize(statement);
    return result;
}

static int validate_schema(sqlite3 *database, int require_global_index)
{
    static const char *const tables[] = {"schema_version", "library_state", "systems", "games",
                                         "apps", "favorites", "legacy_items"};
    for (size_t index = 0; index < sizeof(tables) / sizeof(tables[0]); ++index) {
        int exists = 0;
        int result = table_exists(database, tables[index], &exists);
        if (result != SQLITE_OK || !exists)
            return result == SQLITE_OK ? SQLITE_CORRUPT : result;
    }
    static const char *const indexes[] = {"games_system_sort", "apps_sort"};
    for (size_t index = 0; index < sizeof(indexes) / sizeof(indexes[0]); ++index) {
        int result = index_exists(database, indexes[index]);
        if (result != SQLITE_OK)
            return result;
    }
    if (require_global_index) {
        int result = index_exists(database, "games_global_sort");
        if (result != SQLITE_OK)
            return result;
    }
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(database, "SELECT COUNT(*) FROM schema_version", -1,
                                    &statement, NULL);
    if (result != SQLITE_OK)
        return result;
    result = sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 1
                 ? SQLITE_OK
                 : SQLITE_CORRUPT;
    sqlite3_finalize(statement);
    if (result != SQLITE_OK)
        return result;
    statement = NULL;
    result = sqlite3_prepare_v2(database, "SELECT COUNT(*) FROM library_state WHERE id=1", -1,
                                &statement, NULL);
    if (result != SQLITE_OK)
        return result;
    result = sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 1
                 ? SQLITE_OK
                 : SQLITE_CORRUPT;
    sqlite3_finalize(statement);
    if (result != SQLITE_OK)
        return result;
    return SQLITE_OK;
}

int bloom_library_database_migrate(sqlite3 *database)
{
    if (database == NULL)
        return SQLITE_MISUSE;
    int version = 0;
    int result = database_version(database, &version);
    if (result != SQLITE_OK)
        return result;
    if (version == BLOOM_LIBRARY_DATABASE_SCHEMA_VERSION)
        return validate_schema(database, 1);
    if (version < 0 || version > BLOOM_LIBRARY_DATABASE_SCHEMA_VERSION)
        return SQLITE_MISMATCH;
    if (version == 1) {
        result = validate_schema(database, 0);
        if (result != SQLITE_OK)
            return result;
        result = sqlite3_exec(database, "BEGIN IMMEDIATE", NULL, NULL, NULL);
        if (result == SQLITE_OK)
            result = sqlite3_exec(
                database,
                "CREATE INDEX games_global_sort ON games(present,sort_title,bloom_game_id);"
                "UPDATE schema_version SET version=2",
                NULL, NULL, NULL);
        if (result == SQLITE_OK)
            result = validate_schema(database, 1);
        if (result == SQLITE_OK)
            result = sqlite3_exec(database, "COMMIT", NULL, NULL, NULL);
        if (result != SQLITE_OK)
            sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
        return result;
    }
    result = sqlite3_exec(database, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    if (result != SQLITE_OK)
        return result;
    result = sqlite3_exec(
        database,
        "CREATE TABLE schema_version(version INTEGER NOT NULL);"
        "INSERT INTO schema_version VALUES(2);"
        "CREATE TABLE library_state("
        "id INTEGER PRIMARY KEY CHECK(id=1),generation INTEGER NOT NULL DEFAULT 0,status TEXT NOT NULL "
        "CHECK(status IN('empty','ready','scanning','stale','error')),source TEXT NOT NULL);"
        "INSERT INTO library_state(id,generation,status,source) VALUES(1,0,'empty','onion');"
        "CREATE TABLE systems("
        "system_id TEXT PRIMARY KEY,label TEXT NOT NULL,rom_path TEXT NOT NULL,img_path TEXT,"
        "launch_path TEXT NOT NULL,extensions TEXT NOT NULL,config_size INTEGER NOT NULL,"
        "config_mtime INTEGER NOT NULL,present INTEGER NOT NULL CHECK(present IN(0,1)));"
        "CREATE TABLE games("
        "bloom_game_id TEXT PRIMARY KEY,system_id TEXT NOT NULL,normalized_rom_path TEXT NOT NULL,"
        "display_title TEXT NOT NULL,sort_title TEXT NOT NULL,image_path TEXT,file_size INTEGER NOT NULL,"
        "file_mtime INTEGER NOT NULL,present INTEGER NOT NULL CHECK(present IN(0,1)),"
        "UNIQUE(system_id,normalized_rom_path),FOREIGN KEY(system_id) REFERENCES systems(system_id));"
        "CREATE INDEX games_system_sort ON games(system_id,present,sort_title,bloom_game_id);"
        "CREATE INDEX games_global_sort ON games(present,sort_title,bloom_game_id);"
        "CREATE TABLE apps("
        "app_id TEXT PRIMARY KEY,label TEXT NOT NULL,launch_path TEXT NOT NULL,icon_path TEXT,"
        "config_size INTEGER NOT NULL,config_mtime INTEGER NOT NULL,present INTEGER NOT NULL "
        "CHECK(present IN(0,1)));"
        "CREATE INDEX apps_sort ON apps(present,label,app_id);"
        "CREATE TABLE favorites("
        "bloom_game_id TEXT PRIMARY KEY,position INTEGER NOT NULL UNIQUE,"
        "FOREIGN KEY(bloom_game_id) REFERENCES games(bloom_game_id));"
        "CREATE TABLE legacy_items("
        "kind TEXT NOT NULL CHECK(kind IN('favorite','recent')),position INTEGER NOT NULL,"
        "legacy_identity TEXT NOT NULL,status TEXT NOT NULL "
        "CHECK(status IN('matched','unmatched','duplicate','invalid')),bloom_game_id TEXT,"
        "PRIMARY KEY(kind,position),FOREIGN KEY(bloom_game_id) REFERENCES games(bloom_game_id));",
        NULL, NULL, NULL);
    if (result == SQLITE_OK)
        result = validate_schema(database, 1);
    if (result == SQLITE_OK)
        result = sqlite3_exec(database, "COMMIT", NULL, NULL, NULL);
    if (result != SQLITE_OK)
        sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
    return result;
}

int bloom_library_database_open(const char *path, sqlite3 **database)
{
    if (path == NULL || database == NULL)
        return SQLITE_MISUSE;
    struct stat metadata;
    if (lstat(path, &metadata) == 0 &&
        (!S_ISREG(metadata.st_mode) || S_ISLNK(metadata.st_mode)))
        return SQLITE_CANTOPEN;
    *database = NULL;
    int result = sqlite3_open_v2(path, database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (result == SQLITE_OK)
        result = bloom_sqlite_configure(*database);
    if (result == SQLITE_OK)
        result = sqlite3_exec(*database, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
    if (result == SQLITE_OK)
        result = bloom_library_database_migrate(*database);
    if (result != SQLITE_OK && *database != NULL) {
        sqlite3_close(*database);
        *database = NULL;
    }
    return result;
}

static int scalar(sqlite3 *database, const char *sql, int *value)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(database, sql, -1, &statement, NULL);
    if (result == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW &&
        sqlite3_column_type(statement, 0) == SQLITE_INTEGER)
        *value = sqlite3_column_int(statement, 0);
    else if (result == SQLITE_OK)
        result = SQLITE_CORRUPT;
    sqlite3_finalize(statement);
    return result;
}

static int state_status(sqlite3 *database, char *status, size_t status_size)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(database, "SELECT status FROM library_state WHERE id=1", -1,
                                    &statement, NULL);
    if (result == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW) {
        const char *value = (const char *)sqlite3_column_text(statement, 0);
        size_t length = value == NULL ? 0 : strlen(value);
        if (length == 0 || length >= status_size)
            result = SQLITE_CORRUPT;
        else
            memcpy(status, value, length + 1);
    }
    else if (result == SQLITE_OK)
        result = SQLITE_CORRUPT;
    sqlite3_finalize(statement);
    return result;
}

int bloom_library_database_health(sqlite3 *database, BloomLibraryHealth *health)
{
    if (database == NULL || health == NULL)
        return SQLITE_MISUSE;
    memset(health, 0, sizeof(*health));
    int result = database_version(database, &health->schema_version);
    if (result != SQLITE_OK || health->schema_version != BLOOM_LIBRARY_DATABASE_SCHEMA_VERSION)
        return result == SQLITE_OK ? SQLITE_MISMATCH : result;
    if ((result = validate_schema(database, 1)) != SQLITE_OK ||
        (result = scalar(database, "SELECT generation FROM library_state WHERE id=1",
                         &health->generation)) !=
            SQLITE_OK ||
        (result = state_status(database, health->status, sizeof(health->status))) != SQLITE_OK ||
        (result = scalar(database, "SELECT COUNT(*) FROM systems WHERE present=1", &health->systems)) !=
            SQLITE_OK ||
        (result = scalar(database, "SELECT COUNT(*) FROM games WHERE present=1", &health->games)) !=
            SQLITE_OK ||
        (result = scalar(database, "SELECT COUNT(*) FROM apps WHERE present=1", &health->apps)) !=
            SQLITE_OK ||
        (result = scalar(database, "SELECT COUNT(*) FROM favorites", &health->favorites)) !=
            SQLITE_OK)
        return result;
    return SQLITE_OK;
}
