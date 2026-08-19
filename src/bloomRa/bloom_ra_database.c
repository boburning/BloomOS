#include "bloom_ra_database.h"
#include "../common/database/sqlite_config.h"

#include <string.h>
#include <sys/stat.h>

int bloom_ra_database_open(const char *path, sqlite3 **database)
{
    if (path == NULL || database == NULL)
        return SQLITE_MISUSE;
    struct stat status;
    if (lstat(path, &status) == 0 && (!S_ISREG(status.st_mode) || S_ISLNK(status.st_mode)))
        return SQLITE_CANTOPEN;
    *database = NULL;
    int result = sqlite3_open_v2(path, database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (result == SQLITE_OK)
        result = bloom_sqlite_configure(*database);
    if (result == SQLITE_OK)
        result = sqlite3_exec(*database, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
    if (result == SQLITE_OK)
        result = bloom_ra_database_migrate(*database);
    if (result != SQLITE_OK && *database != NULL) {
        sqlite3_close(*database);
        *database = NULL;
    }
    return result;
}

static int table_exists(sqlite3 *database, const char *name, int *exists)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(database,
                                    "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1", -1, &statement, NULL);
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

int bloom_ra_database_version(sqlite3 *database, int *version)
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

static int validate_schema(sqlite3 *database)
{
    static const char *tables[] = {"schema_version", "catalog_state", "ra_games", "ra_hashes", "library_games"};
    for (size_t i = 0; i < sizeof(tables) / sizeof(tables[0]); i++) {
        int exists = 0;
        int result = table_exists(database, tables[i], &exists);
        if (result != SQLITE_OK || !exists)
            return result == SQLITE_OK ? SQLITE_CORRUPT : result;
    }
    return SQLITE_OK;
}

int bloom_ra_database_migrate(sqlite3 *database)
{
    int version = 0;
    int result = bloom_ra_database_version(database, &version);
    if (result != SQLITE_OK)
        return result;
    if (version == BLOOM_RA_DATABASE_SCHEMA_VERSION)
        return validate_schema(database);
    if (version < 0 || version > BLOOM_RA_DATABASE_SCHEMA_VERSION)
        return SQLITE_MISMATCH;
    result = sqlite3_exec(database, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    if (result != SQLITE_OK)
        return result;
    result = sqlite3_exec(
        database,
        "CREATE TABLE schema_version(version INTEGER NOT NULL);"
        "INSERT INTO schema_version VALUES(0);"
        "CREATE TABLE catalog_state("
        "catalog_generation INTEGER NOT NULL DEFAULT 0,provider TEXT,provider_revision TEXT,"
        "refreshed_at INTEGER,last_success_at INTEGER,status TEXT NOT NULL);"
        "INSERT INTO catalog_state(status) VALUES('unavailable');"
        "CREATE TABLE ra_games(ra_game_id INTEGER PRIMARY KEY,ra_console_id INTEGER NOT NULL,title TEXT NOT NULL,"
        "official_set INTEGER NOT NULL CHECK(official_set IN(0,1)),achievement_count INTEGER NOT NULL CHECK(achievement_count>=0),"
        "metadata_revision TEXT);"
        "CREATE TABLE ra_hashes(ra_console_id INTEGER NOT NULL,ra_content_hash TEXT NOT NULL,ra_game_id INTEGER NOT NULL,"
        "UNIQUE(ra_console_id,ra_content_hash),FOREIGN KEY(ra_game_id) REFERENCES ra_games(ra_game_id));"
        "CREATE TABLE library_games("
        "bloom_game_id TEXT PRIMARY KEY,system_id TEXT NOT NULL,normalized_rom_path TEXT NOT NULL,file_size INTEGER NOT NULL,"
        "file_mtime INTEGER NOT NULL,ra_console_id INTEGER,ra_content_hash TEXT,ra_game_id INTEGER,official_set INTEGER,"
        "achievement_count INTEGER,indexed_at INTEGER,catalog_generation INTEGER NOT NULL DEFAULT 0,hash_version INTEGER NOT NULL,"
        "status TEXT NOT NULL CHECK(status IN('identified','unmatched','unsupported_system','hash_error','deferred','stale')),"
        "FOREIGN KEY(ra_game_id) REFERENCES ra_games(ra_game_id));"
        "CREATE INDEX library_games_system_status ON library_games(system_id,status);"
        "UPDATE schema_version SET version=1;",
        NULL, NULL, NULL);
    if (result == SQLITE_OK)
        result = validate_schema(database);
    if (result == SQLITE_OK)
        result = sqlite3_exec(database, "COMMIT", NULL, NULL, NULL);
    if (result != SQLITE_OK)
        sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
    return result;
}

static int scalar(sqlite3 *database, const char *sql, int *value)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(database, sql, -1, &statement, NULL);
    if (result == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW)
        *value = sqlite3_column_int(statement, 0);
    else if (result == SQLITE_OK)
        result = SQLITE_CORRUPT;
    sqlite3_finalize(statement);
    return result;
}

int bloom_ra_database_health(sqlite3 *database, int *version, int *indexed_games, int *identified_games)
{
    if (database == NULL || version == NULL || indexed_games == NULL || identified_games == NULL)
        return SQLITE_MISUSE;
    int result = bloom_ra_database_version(database, version);
    if (result != SQLITE_OK || *version != BLOOM_RA_DATABASE_SCHEMA_VERSION)
        return result == SQLITE_OK ? SQLITE_MISMATCH : result;
    result = validate_schema(database);
    if (result == SQLITE_OK)
        result = scalar(database, "SELECT COUNT(*) FROM library_games", indexed_games);
    if (result == SQLITE_OK)
        result = scalar(database, "SELECT COUNT(*) FROM library_games WHERE status='identified'", identified_games);
    return result;
}

int bloom_ra_database_collection(sqlite3 *database, BloomRaCollectionVisitor visitor, void *context,
                                 unsigned long *count)
{
    if (database == NULL || visitor == NULL || count == NULL)
        return SQLITE_MISUSE;
    *count = 0;
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database,
        "SELECT bloom_game_id,system_id FROM library_games "
        "WHERE ra_game_id>0 AND official_set=1 AND achievement_count>0 "
        "ORDER BY system_id,bloom_game_id",
        -1, &statement, NULL);
    if (result != SQLITE_OK)
        return result;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        const char *game_id = (const char *)sqlite3_column_text(statement, 0);
        const char *system_id = (const char *)sqlite3_column_text(statement, 1);
        if (game_id == NULL || system_id == NULL || visitor(game_id, system_id, context) != 0) {
            sqlite3_finalize(statement);
            return SQLITE_ABORT;
        }
        (*count)++;
    }
    int final_result = result == SQLITE_DONE ? SQLITE_OK : result;
    sqlite3_finalize(statement);
    return final_result;
}
