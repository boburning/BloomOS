#include "playActivityIdentity.h"

#include "../bloomGameId/bloom_game_id.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct candidate {
    sqlite3_int64 row_id;
    char game_id[BLOOM_GAME_ID_LENGTH + 1];
    int duplicate;
};

static const char *system_for_path(const char *path)
{
    static const struct {
        const char *folder;
        const char *system;
    } mappings[] = {{"GB", "gb"}, {"GBC", "gbc"}, {"GBA", "gba"}, {"FC", "nes"}, {"SFC", "snes"}, {"PS", "psx"}};
    const char *relative = path;
    static const char root[] = "/mnt/SDCARD/Roms/";
    if (strncmp(relative, root, strlen(root)) == 0)
        relative += strlen(root);
    const char *slash = strchr(relative, '/');
    if (slash == NULL)
        return NULL;
    size_t folder_length = (size_t)(slash - relative);
    for (size_t i = 0; i < sizeof(mappings) / sizeof(mappings[0]); i++)
        if (strlen(mappings[i].folder) == folder_length && strncmp(relative, mappings[i].folder, folder_length) == 0)
            return mappings[i].system;
    return NULL;
}

static int absolute_rom_path(const char *path, char *output, size_t size)
{
    static const char root[] = "/mnt/SDCARD/Roms/";
    int length;
    if (strncmp(path, root, strlen(root)) == 0)
        length = snprintf(output, size, "%s", path);
    else
        length = snprintf(output, size, "%s%s", root, path);
    return length >= 0 && (size_t)length < size ? SQLITE_OK : SQLITE_TOOBIG;
}

int play_activity_backfill_game_ids(sqlite3 *database, int dry_run, struct play_activity_identity_result *result)
{
    if (database == NULL || result == NULL || (dry_run != 0 && dry_run != 1))
        return SQLITE_MISUSE;
    memset(result, 0, sizeof(*result));
    sqlite3_stmt *select = NULL;
    int rc = sqlite3_prepare_v2(database, "SELECT id,file_path FROM rom WHERE game_id IS NULL ORDER BY id", -1,
                                &select, NULL);
    if (rc != SQLITE_OK)
        return rc;
    struct candidate *candidates = NULL;
    size_t count = 0;
    size_t capacity = 0;
    while ((rc = sqlite3_step(select)) == SQLITE_ROW) {
        const unsigned char *path_value = sqlite3_column_text(select, 1);
        const char *path = (const char *)path_value;
        const char *system = path == NULL ? NULL : system_for_path(path);
        if (system == NULL) {
            result->deferred++;
            continue;
        }
        if (count == capacity) {
            size_t next_capacity = capacity == 0 ? 16 : capacity * 2;
            struct candidate *next = realloc(candidates, next_capacity * sizeof(*next));
            if (next == NULL) {
                rc = SQLITE_NOMEM;
                break;
            }
            candidates = next;
            capacity = next_capacity;
        }
        char absolute[4096];
        char relative[4096];
        char error[128];
        if (absolute_rom_path(path, absolute, sizeof(absolute)) != SQLITE_OK ||
            bloom_game_id_create(system, absolute, candidates[count].game_id, sizeof(candidates[count].game_id),
                                 relative, sizeof(relative), error, sizeof(error)) != 0) {
            result->deferred++;
            continue;
        }
        candidates[count].row_id = sqlite3_column_int64(select, 0);
        candidates[count].duplicate = 0;
        count++;
    }
    if (rc == SQLITE_DONE)
        rc = SQLITE_OK;
    sqlite3_finalize(select);
    if (rc != SQLITE_OK) {
        free(candidates);
        return rc;
    }
    sqlite3_stmt *existing = NULL;
    rc = sqlite3_prepare_v2(database, "SELECT 1 FROM rom WHERE game_id=?1 LIMIT 1", -1, &existing, NULL);
    for (size_t i = 0; rc == SQLITE_OK && i < count; i++) {
        sqlite3_bind_text(existing, 1, candidates[i].game_id, -1, SQLITE_STATIC);
        int step = sqlite3_step(existing);
        if (step == SQLITE_ROW)
            candidates[i].duplicate = 1;
        else if (step != SQLITE_DONE)
            rc = step;
        sqlite3_reset(existing);
        sqlite3_clear_bindings(existing);
    }
    sqlite3_finalize(existing);
    if (rc != SQLITE_OK) {
        free(candidates);
        return rc;
    }
    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            if (strcmp(candidates[i].game_id, candidates[j].game_id) == 0) {
                candidates[i].duplicate = 1;
                candidates[j].duplicate = 1;
            }
        }
    }
    for (size_t i = 0; i < count; i++) {
        if (candidates[i].duplicate)
            result->deferred++;
        else
            result->updated++;
    }
    if (dry_run) {
        free(candidates);
        return SQLITE_OK;
    }
    rc = sqlite3_exec(database, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    sqlite3_stmt *update = NULL;
    if (rc == SQLITE_OK)
        rc = sqlite3_prepare_v2(database, "UPDATE rom SET game_id=?1 WHERE id=?2 AND game_id IS NULL", -1, &update,
                                NULL);
    for (size_t i = 0; rc == SQLITE_OK && i < count; i++) {
        if (candidates[i].duplicate)
            continue;
        sqlite3_bind_text(update, 1, candidates[i].game_id, -1, SQLITE_STATIC);
        sqlite3_bind_int64(update, 2, candidates[i].row_id);
        rc = sqlite3_step(update) == SQLITE_DONE && sqlite3_changes(database) == 1 ? SQLITE_OK : SQLITE_CONSTRAINT;
        sqlite3_reset(update);
        sqlite3_clear_bindings(update);
    }
    sqlite3_finalize(update);
    if (rc == SQLITE_OK)
        rc = sqlite3_exec(database, "COMMIT", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
    free(candidates);
    return rc;
}
