#include "bloom_ra_scanner.h"

#include "../bloomGameId/bloom_game_id.h"
#include "bloom_ra.h"
#include "bloom_ra_catalog.h"

#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#define BLOOM_RA_HASH_VERSION 1

static int unchanged(sqlite3 *database, const char *game_id, sqlite3_int64 size, sqlite3_int64 mtime, int *is_unchanged,
                     const char **status)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database,
        "SELECT status FROM library_games WHERE bloom_game_id=?1 AND file_size=?2 AND file_mtime=?3 AND hash_version=?4",
        -1, &statement, NULL);
    if (result == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_STATIC);
        sqlite3_bind_int64(statement, 2, size);
        sqlite3_bind_int64(statement, 3, mtime);
        sqlite3_bind_int(statement, 4, BLOOM_RA_HASH_VERSION);
        int step = sqlite3_step(statement);
        *is_unchanged = step == SQLITE_ROW;
        if (step == SQLITE_ROW) {
            const unsigned char *value = sqlite3_column_text(statement, 0);
            *status = value != NULL && strcmp((const char *)value, "identified") == 0 ? "identified" : "unmatched";
        }
        else if (step != SQLITE_DONE) {
            result = step;
        }
    }
    sqlite3_finalize(statement);
    return result;
}

static int store(sqlite3 *database, const char *game_id, const char *system_id, const char *normalized_path,
                 sqlite3_int64 size, sqlite3_int64 mtime, int console_id, const char *hash, int ra_game_id,
                 int achievements, const char *status)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database,
        "INSERT INTO library_games(bloom_game_id,system_id,normalized_rom_path,file_size,file_mtime,ra_console_id,"
        "ra_content_hash,ra_game_id,official_set,achievement_count,indexed_at,catalog_generation,hash_version,status) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,strftime('%s','now'),"
        "(SELECT catalog_generation FROM catalog_state),?11,?12) "
        "ON CONFLICT(bloom_game_id) DO UPDATE SET system_id=excluded.system_id,normalized_rom_path=excluded.normalized_rom_path,"
        "file_size=excluded.file_size,file_mtime=excluded.file_mtime,ra_console_id=excluded.ra_console_id,"
        "ra_content_hash=excluded.ra_content_hash,ra_game_id=excluded.ra_game_id,official_set=excluded.official_set,"
        "achievement_count=excluded.achievement_count,indexed_at=excluded.indexed_at,"
        "catalog_generation=excluded.catalog_generation,hash_version=excluded.hash_version,status=excluded.status",
        -1, &statement, NULL);
    if (result == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, system_id, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 3, normalized_path, -1, SQLITE_STATIC);
        sqlite3_bind_int64(statement, 4, size);
        sqlite3_bind_int64(statement, 5, mtime);
        if (console_id > 0)
            sqlite3_bind_int(statement, 6, console_id);
        else
            sqlite3_bind_null(statement, 6);
        if (hash != NULL)
            sqlite3_bind_text(statement, 7, hash, -1, SQLITE_STATIC);
        else
            sqlite3_bind_null(statement, 7);
        if (ra_game_id > 0)
            sqlite3_bind_int(statement, 8, ra_game_id);
        else
            sqlite3_bind_null(statement, 8);
        if (ra_game_id > 0) {
            sqlite3_bind_int(statement, 9, 1);
            sqlite3_bind_int(statement, 10, achievements);
        }
        else {
            sqlite3_bind_null(statement, 9);
            sqlite3_bind_null(statement, 10);
        }
        sqlite3_bind_int(statement, 11, BLOOM_RA_HASH_VERSION);
        sqlite3_bind_text(statement, 12, status, -1, SQLITE_STATIC);
        int step = sqlite3_step(statement);
        result = step == SQLITE_DONE ? SQLITE_OK : step;
    }
    sqlite3_finalize(statement);
    return result;
}

int bloom_ra_scan_game(sqlite3 *database, const char *bloom_game_id, const char *system_id, const char *rom_path,
                       const char *rom_root, const char *normalized_rom_path, int force, BloomRaScanResult *scan_result)
{
    if (database == NULL || !bloom_game_id_valid(bloom_game_id) || system_id == NULL || rom_path == NULL ||
        rom_root == NULL || normalized_rom_path == NULL || scan_result == NULL)
        return SQLITE_MISUSE;
    struct stat metadata;
    if (stat(rom_path, &metadata) != 0 || !S_ISREG(metadata.st_mode))
        return SQLITE_CANTOPEN;
    scan_result->skipped = 0;
    scan_result->identified = 0;
    scan_result->status = "hash_error";
    if (!force) {
        int is_unchanged = 0;
        const char *status = NULL;
        int result = unchanged(database, bloom_game_id, metadata.st_size, metadata.st_mtime, &is_unchanged, &status);
        if (result != SQLITE_OK)
            return result;
        if (is_unchanged) {
            scan_result->skipped = 1;
            scan_result->identified = strcmp(status, "identified") == 0;
            scan_result->status = status;
            return SQLITE_OK;
        }
    }
    uint32_t console_id = 0;
    if (bloom_ra_console_id(system_id, &console_id) != 0) {
        scan_result->status = "unsupported_system";
        return store(database, bloom_game_id, system_id, normalized_rom_path, metadata.st_size, metadata.st_mtime, 0,
                     NULL, 0, 0, scan_result->status);
    }
    char hash[33] = {0};
    char error[128] = {0};
    if (bloom_ra_hash_file(system_id, rom_path, rom_root, hash, error, sizeof(error)) != 0)
        return store(database, bloom_game_id, system_id, normalized_rom_path, metadata.st_size, metadata.st_mtime,
                     (int)console_id, NULL, 0, 0, scan_result->status);
    int ra_game_id = 0;
    int achievements = 0;
    int resolve = bloom_ra_catalog_resolve(database, (int)console_id, hash, &ra_game_id, &achievements);
    if (resolve == SQLITE_OK) {
        scan_result->identified = 1;
        scan_result->status = "identified";
    }
    else if (resolve == SQLITE_NOTFOUND) {
        scan_result->status = "unmatched";
    }
    else {
        return resolve;
    }
    return store(database, bloom_game_id, system_id, normalized_rom_path, metadata.st_size, metadata.st_mtime,
                 (int)console_id, hash, ra_game_id, achievements, scan_result->status);
}
