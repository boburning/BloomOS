#include "bloom_ra_scanner.h"

#include "../bloomGameId/bloom_game_id.h"
#include "bloom_ra.h"
#include "bloom_ra_catalog.h"

#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define BLOOM_RA_HASH_VERSION 2

static int unchanged(sqlite3 *database, const char *game_id, sqlite3_int64 size, sqlite3_int64 mtime,
                     sqlite3_int64 dependency_size, sqlite3_int64 dependency_mtime, int *is_unchanged,
                     const char **status)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database,
        "SELECT status FROM library_games WHERE bloom_game_id=?1 AND file_size=?2 AND file_mtime=?3 AND hash_version=?4 "
        "AND ((?5<0 AND dependency_size IS NULL AND dependency_mtime IS NULL) OR "
        "(dependency_size=?5 AND dependency_mtime=?6))",
        -1, &statement, NULL);
    if (result == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_STATIC);
        sqlite3_bind_int64(statement, 2, size);
        sqlite3_bind_int64(statement, 3, mtime);
        sqlite3_bind_int(statement, 4, BLOOM_RA_HASH_VERSION);
        sqlite3_bind_int64(statement, 5, dependency_size);
        sqlite3_bind_int64(statement, 6, dependency_mtime);
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

static int marker_exists(const char *path)
{
    struct stat status;
    return path != NULL && lstat(path, &status) == 0 && S_ISREG(status.st_mode);
}

static int session_active(const char *path)
{
    if (path == NULL)
        return 0;
    FILE *file = fopen(path, "r");
    if (file == NULL)
        return 0;
    char state[32] = {0};
    int active = fscanf(file, "%31s", state) == 1 &&
                 (strcmp(state, "PREPARING") == 0 || strcmp(state, "STARTING") == 0 ||
                  strcmp(state, "RUNNING") == 0 || strcmp(state, "STOP_REQUESTED") == 0 ||
                  strcmp(state, "FLUSHING") == 0);
    fclose(file);
    return active;
}

static int scan_interruption(const char *session_state_path, const char *cancel_path, BloomRaScanStats *stats)
{
    if (marker_exists(cancel_path)) {
        stats->canceled = 1;
        return SQLITE_INTERRUPT;
    }
    if (session_active(session_state_path)) {
        stats->paused = 1;
        return SQLITE_BUSY;
    }
    return SQLITE_OK;
}

#ifdef BLOOM_TEST
int bloom_ra_scan_interruption_for_test(const char *session_state_path, const char *cancel_path,
                                        BloomRaScanStats *stats)
{
    if (stats == NULL)
        return SQLITE_MISUSE;
    return scan_interruption(session_state_path, cancel_path, stats);
}
#endif

static int ignored_name(const char *name)
{
    return name[0] == '.' || strcmp(name, "Imgs") == 0 || strcmp(name, "images") == 0 ||
           strcmp(name, "Snaps") == 0;
}

static int supported_extension(const char *path)
{
    static const char *extensions[] = {
        "zip", "gb", "gbc", "gba", "nes", "fds", "sfc", "smc", "fig", "bs", "md", "gen", "bin",
        "cue", "chd", "pbp", "m3u", "iso", "gg", "sms", "sg", "pce", "ccd", "toc", "a26", "a78",
        "lnx", "ws", "wsc", "ngc", "vb", "col", "rom", "dsk", "tap", "cas", "adf", "ipf", "neo"};
    const char *dot = strrchr(path, '.');
    if (dot == NULL || dot[1] == '\0')
        return 0;
    dot++;
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++)
        if (strcasecmp(dot, extensions[i]) == 0)
            return 1;
    return 0;
}

static int scan_directory(sqlite3 *database, const char *system_id, const char *directory, int force,
                          const char *session_state_path, const char *cancel_path, BloomRaScanStats *stats)
{
    int interruption = scan_interruption(session_state_path, cancel_path, stats);
    if (interruption != SQLITE_OK)
        return interruption;
    DIR *handle = opendir(directory);
    if (handle == NULL)
        return SQLITE_CANTOPEN;
    int result = SQLITE_OK;
    struct dirent *entry;
    while ((entry = readdir(handle)) != NULL) {
        result = scan_interruption(session_state_path, cancel_path, stats);
        if (result != SQLITE_OK)
            break;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || ignored_name(entry->d_name))
            continue;
        char path[PATH_MAX];
        if (snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name) >= (int)sizeof(path)) {
            stats->errors++;
            continue;
        }
        struct stat metadata;
        if (lstat(path, &metadata) != 0 || S_ISLNK(metadata.st_mode)) {
            stats->errors++;
            continue;
        }
        if (S_ISDIR(metadata.st_mode)) {
            result = scan_directory(database, system_id, path, force, session_state_path, cancel_path, stats);
        }
        else if (S_ISREG(metadata.st_mode) && supported_extension(path)) {
            char game_id[BLOOM_GAME_ID_LENGTH + 1];
            char normalized[PATH_MAX];
            char error[128];
            if (bloom_game_id_create(system_id, path, game_id, sizeof(game_id), normalized, sizeof(normalized), error,
                                     sizeof(error)) != 0) {
                stats->errors++;
                continue;
            }
            BloomRaScanResult game_result;
            int scan = bloom_ra_scan_game(database, game_id, system_id, path, "/mnt/SDCARD/Roms", normalized, force,
                                          &game_result);
            stats->processed++;
            if (scan != SQLITE_OK)
                stats->errors++;
            else {
                stats->skipped += (unsigned long)game_result.skipped;
                stats->identified += (unsigned long)game_result.identified;
            }
        }
        if (result == SQLITE_INTERRUPT || result == SQLITE_BUSY)
            break;
    }
    closedir(handle);
    return result;
}

int bloom_ra_scan_tree(sqlite3 *database, const char *system_id, const char *system_path, int force,
                       const char *session_state_path, const char *cancel_path, BloomRaScanStats *stats)
{
    if (database == NULL || system_id == NULL || system_path == NULL || stats == NULL)
        return SQLITE_MISUSE;
    memset(stats, 0, sizeof(*stats));
    char resolved[PATH_MAX];
    struct stat metadata;
    if (lstat(system_path, &metadata) != 0 || !S_ISDIR(metadata.st_mode) || S_ISLNK(metadata.st_mode) ||
        realpath(system_path, resolved) == NULL || strncmp(resolved, "/mnt/SDCARD/Roms/", 17) != 0)
        return SQLITE_CANTOPEN;
    return scan_directory(database, system_id, resolved, force, session_state_path, cancel_path, stats);
}

static int store(sqlite3 *database, const char *game_id, const char *system_id, const char *normalized_path,
                 sqlite3_int64 size, sqlite3_int64 mtime, sqlite3_int64 dependency_size,
                 sqlite3_int64 dependency_mtime, int console_id, const char *hash, int ra_game_id, int achievements,
                 const char *status)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database,
        "INSERT INTO library_games(bloom_game_id,system_id,normalized_rom_path,file_size,file_mtime,ra_console_id,"
        "ra_content_hash,ra_game_id,official_set,achievement_count,indexed_at,catalog_generation,hash_version,status,"
        "dependency_size,dependency_mtime) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,strftime('%s','now'),"
        "(SELECT catalog_generation FROM catalog_state),?11,?12,?13,?14) "
        "ON CONFLICT(bloom_game_id) DO UPDATE SET system_id=excluded.system_id,normalized_rom_path=excluded.normalized_rom_path,"
        "file_size=excluded.file_size,file_mtime=excluded.file_mtime,ra_console_id=excluded.ra_console_id,"
        "ra_content_hash=excluded.ra_content_hash,ra_game_id=excluded.ra_game_id,official_set=excluded.official_set,"
        "achievement_count=excluded.achievement_count,indexed_at=excluded.indexed_at,"
        "catalog_generation=excluded.catalog_generation,hash_version=excluded.hash_version,status=excluded.status,"
        "dependency_size=excluded.dependency_size,dependency_mtime=excluded.dependency_mtime",
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
        if (dependency_size >= 0) {
            sqlite3_bind_int64(statement, 13, dependency_size);
            sqlite3_bind_int64(statement, 14, dependency_mtime);
        }
        else {
            sqlite3_bind_null(statement, 13);
            sqlite3_bind_null(statement, 14);
        }
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
    const char *extension = strrchr(rom_path, '.');
    int composite_playlist = extension != NULL && strcasecmp(extension, ".m3u") == 0;
    sqlite3_int64 dependency_size = -1;
    sqlite3_int64 dependency_mtime = -1;
    if (composite_playlist) {
        int64_t size = 0;
        int64_t mtime = 0;
        char dependency_error[128] = {0};
        if (bloom_ra_playlist_dependency(rom_path, rom_root, &size, &mtime, dependency_error,
                                         sizeof(dependency_error)) == 0) {
            dependency_size = (sqlite3_int64)size;
            dependency_mtime = (sqlite3_int64)mtime;
        }
    }
    if (!force && (!composite_playlist || dependency_size >= 0)) {
        int is_unchanged = 0;
        const char *status = NULL;
        int result = unchanged(database, bloom_game_id, metadata.st_size, metadata.st_mtime, dependency_size,
                               dependency_mtime, &is_unchanged, &status);
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
        return store(database, bloom_game_id, system_id, normalized_rom_path, metadata.st_size, metadata.st_mtime,
                     dependency_size, dependency_mtime, 0, NULL, 0, 0, scan_result->status);
    }
    char hash[33] = {0};
    char error[128] = {0};
    if (bloom_ra_hash_file(system_id, rom_path, rom_root, hash, error, sizeof(error)) != 0)
        return store(database, bloom_game_id, system_id, normalized_rom_path, metadata.st_size, metadata.st_mtime,
                     dependency_size, dependency_mtime, (int)console_id, NULL, 0, 0, scan_result->status);
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
                 dependency_size, dependency_mtime, (int)console_id, hash, ra_game_id, achievements,
                 scan_result->status);
}
