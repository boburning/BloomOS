#include "bloom_library_legacy.h"

#include <cjson/cJSON.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_LEGACY_FILE_SIZE (1024 * 1024)
#define MAX_LEGACY_ITEMS 2048
#define MAX_LEGACY_LINE 8192
#define TEXT_SIZE 512
#define GAME_ID_SIZE 79

typedef struct {
    char identity[TEXT_SIZE];
    char game_id[GAME_ID_SIZE];
    const char *status;
} LegacyItem;

typedef struct {
    LegacyItem *items;
    size_t count;
} LegacyList;

static void set_error(char *error, size_t size, const char *format, ...)
{
    if (error == NULL || size == 0)
        return;
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error, size, format, arguments);
    va_end(arguments);
}

static int regular_or_missing(const char *path, int *missing, char *error, size_t error_size)
{
    struct stat metadata;
    *missing = 0;
    if (lstat(path, &metadata) != 0) {
        if (errno == ENOENT) {
            *missing = 1;
            return SQLITE_OK;
        }
        set_error(error, error_size, "legacy list could not be inspected");
        return SQLITE_CANTOPEN;
    }
    if (!S_ISREG(metadata.st_mode) || S_ISLNK(metadata.st_mode) || metadata.st_size < 0 ||
        metadata.st_size > MAX_LEGACY_FILE_SIZE) {
        set_error(error, error_size, "legacy list is unavailable or unsafe");
        return SQLITE_CANTOPEN;
    }
    return SQLITE_OK;
}

static int known_game_type(const cJSON *value)
{
    return cJSON_IsNumber(value) && value->valuedouble == value->valueint &&
           (value->valueint == 0 || value->valueint == 1 || value->valueint == 5 ||
            value->valueint == 17);
}

static int object_has_unique_keys(const cJSON *object)
{
    for (const cJSON *item = object->child; item != NULL; item = item->next) {
        if (item->string == NULL)
            return 0;
        for (const cJSON *other = item->next; other != NULL; other = other->next)
            if (other->string != NULL && strcmp(item->string, other->string) == 0)
                return 0;
    }
    return 1;
}

static int safe_relative_path(const char *value)
{
    if (value == NULL || value[0] == '\0' || value[0] == '/' || strlen(value) >= TEXT_SIZE)
        return 0;
    const char *component = value;
    while (*component != '\0') {
        const char *separator = strchr(component, '/');
        size_t length = separator == NULL ? strlen(component) : (size_t)(separator - component);
        if (length == 0 || (length == 1 && component[0] == '.') ||
            (length == 2 && component[0] == '.' && component[1] == '.'))
            return 0;
        for (size_t index = 0; index < length; ++index) {
            unsigned char byte = (unsigned char)component[index];
            if (byte < 0x20 || byte == 0x7f || byte == '\\')
                return 0;
        }
        if (separator == NULL)
            return 1;
        component = separator + 1;
    }
    return 0;
}

static int normalize_rom_path(const char *rom_root, const char *input, char *output,
                              size_t output_size)
{
    if (rom_root == NULL || input == NULL || output == NULL || rom_root[0] != '/' ||
        strchr(rom_root, '\\') != NULL)
        return 0;
    size_t root_length = strlen(rom_root);
    while (root_length > 1 && rom_root[root_length - 1] == '/')
        --root_length;
    const char *path = input;
    const char *colon = strchr(input, ':');
    if (colon != NULL && strncmp(colon + 1, rom_root, root_length) == 0)
        path = colon + 1;
    if (strncmp(path, rom_root, root_length) != 0 || path[root_length] != '/')
        return 0;
    path += root_length + 1;
    size_t length = strlen(path);
    if (length == 0 || length >= output_size || !safe_relative_path(path))
        return 0;
    memcpy(output, path, length + 1);
    return 1;
}

static int lookup_game(sqlite3 *database, const char *relative_path, char *game_id,
                       size_t game_id_size)
{
    sqlite3_stmt *statement = NULL;
    int sql = sqlite3_prepare_v2(
        database,
        "SELECT bloom_game_id FROM games WHERE normalized_rom_path=?1 AND present=1", -1,
        &statement, NULL);
    if (sql == SQLITE_OK)
        sql = sqlite3_bind_text(statement, 1, relative_path, -1, SQLITE_STATIC);
    if (sql == SQLITE_OK) {
        int step = sqlite3_step(statement);
        if (step == SQLITE_ROW) {
            const char *value = (const char *)sqlite3_column_text(statement, 0);
            size_t length = value == NULL ? 0 : strlen(value);
            if (length == 0 || length >= game_id_size)
                sql = SQLITE_CORRUPT;
            else
                memcpy(game_id, value, length + 1);
        }
        else
            sql = step == SQLITE_DONE ? SQLITE_NOTFOUND : step;
    }
    sqlite3_finalize(statement);
    return sql;
}

static int duplicate_game(const LegacyList *list, const char *game_id)
{
    for (size_t index = 0; index < list->count; ++index)
        if (list->items[index].game_id[0] != '\0' &&
            strcmp(list->items[index].game_id, game_id) == 0)
            return 1;
    return 0;
}

static int parse_line(sqlite3 *database, const char *rom_root, const char *line,
                      size_t position, LegacyList *list)
{
    LegacyItem *item = &list->items[list->count];
    memset(item, 0, sizeof(*item));
    snprintf(item->identity, sizeof(item->identity), "invalid:%lu", (unsigned long)position);
    item->status = "invalid";
    cJSON *root = cJSON_ParseWithOpts(line, NULL, 1);
    if (!cJSON_IsObject(root) || !object_has_unique_keys(root)) {
        cJSON_Delete(root);
        ++list->count;
        return SQLITE_OK;
    }
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    const cJSON *rompath = cJSON_GetObjectItemCaseSensitive(root, "rompath");
    char relative[TEXT_SIZE];
    if (known_game_type(type) && cJSON_IsString(rompath) && rompath->valuestring != NULL &&
        normalize_rom_path(rom_root, rompath->valuestring, relative, sizeof(relative))) {
        memcpy(item->identity, relative, strlen(relative) + 1);
        int sql = lookup_game(database, relative, item->game_id, sizeof(item->game_id));
        if (sql == SQLITE_OK) {
            if (duplicate_game(list, item->game_id)) {
                item->status = "duplicate";
                item->game_id[0] = '\0';
            }
            else
                item->status = "matched";
        }
        else if (sql == SQLITE_NOTFOUND) {
            item->status = "unmatched";
            item->game_id[0] = '\0';
        }
        else {
            cJSON_Delete(root);
            return sql;
        }
    }
    cJSON_Delete(root);
    ++list->count;
    return SQLITE_OK;
}

static int load_list(sqlite3 *database, const char *rom_root, const char *path, LegacyList *list,
                     char *error, size_t error_size)
{
    int missing = 0;
    int sql = regular_or_missing(path, &missing, error, error_size);
    if (sql != SQLITE_OK || missing)
        return sql;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        set_error(error, error_size, "legacy list could not be opened");
        return SQLITE_CANTOPEN;
    }
    char line[MAX_LEGACY_LINE];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (list->count >= MAX_LEGACY_ITEMS) {
            sql = SQLITE_TOOBIG;
            set_error(error, error_size, "legacy list has too many entries");
            break;
        }
        size_t length = strlen(line);
        if (length == sizeof(line) - 1 && line[length - 1] != '\n' && !feof(file)) {
            sql = SQLITE_TOOBIG;
            set_error(error, error_size, "legacy list line is too large");
            break;
        }
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
            line[--length] = '\0';
        if (length == 0)
            continue;
        sql = parse_line(database, rom_root, line, list->count, list);
        if (sql != SQLITE_OK)
            break;
    }
    if (sql == SQLITE_OK && ferror(file)) {
        sql = SQLITE_IOERR_READ;
        set_error(error, error_size, "legacy list could not be read");
    }
    fclose(file);
    return sql;
}

static int publish_list(sqlite3 *database, const char *kind, const char *table,
                        const LegacyList *list, BloomLibraryLegacyResult *result)
{
    char delete_table[64];
    if (snprintf(delete_table, sizeof(delete_table), "DELETE FROM %s", table) >=
        (int)sizeof(delete_table))
        return SQLITE_MISUSE;
    int sql = sqlite3_exec(database, delete_table, NULL, NULL, NULL);
    sqlite3_stmt *legacy = NULL;
    sqlite3_stmt *canonical = NULL;
    if (sql == SQLITE_OK)
        sql = sqlite3_prepare_v2(database, "DELETE FROM legacy_items WHERE kind=?1", -1,
                                 &legacy, NULL);
    if (sql == SQLITE_OK)
        sql = sqlite3_bind_text(legacy, 1, kind, -1, SQLITE_STATIC);
    if (sql == SQLITE_OK)
        sql = sqlite3_step(legacy) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    sqlite3_finalize(legacy);
    legacy = NULL;
    char insert_table[128];
    if (sql == SQLITE_OK &&
        snprintf(insert_table, sizeof(insert_table),
                 "INSERT INTO %s(bloom_game_id,position) VALUES(?1,?2)", table) >=
            (int)sizeof(insert_table))
        sql = SQLITE_MISUSE;
    if (sql == SQLITE_OK)
        sql = sqlite3_prepare_v2(
            database,
            "INSERT INTO legacy_items(kind,position,legacy_identity,status,bloom_game_id) "
            "VALUES(?1,?2,?3,?4,?5)",
            -1, &legacy, NULL);
    if (sql == SQLITE_OK)
        sql = sqlite3_prepare_v2(database, insert_table, -1, &canonical, NULL);
    int canonical_position = 0;
    for (size_t index = 0; sql == SQLITE_OK && index < list->count; ++index) {
        const LegacyItem *item = &list->items[index];
        sqlite3_reset(legacy);
        sqlite3_clear_bindings(legacy);
        sql = sqlite3_bind_text(legacy, 1, kind, -1, SQLITE_STATIC);
        if (sql == SQLITE_OK)
            sql = sqlite3_bind_int(legacy, 2, (int)index);
        if (sql == SQLITE_OK)
            sql = sqlite3_bind_text(legacy, 3, item->identity, -1, SQLITE_STATIC);
        if (sql == SQLITE_OK)
            sql = sqlite3_bind_text(legacy, 4, item->status, -1, SQLITE_STATIC);
        if (sql == SQLITE_OK)
            sql = item->game_id[0] == '\0' ? sqlite3_bind_null(legacy, 5)
                                           : sqlite3_bind_text(legacy, 5, item->game_id, -1,
                                                               SQLITE_STATIC);
        if (sql == SQLITE_OK)
            sql = sqlite3_step(legacy) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
        if (strcmp(item->status, "matched") == 0) {
            ++result->matched;
            sqlite3_reset(canonical);
            sqlite3_clear_bindings(canonical);
            if (sql == SQLITE_OK)
                sql = sqlite3_bind_text(canonical, 1, item->game_id, -1, SQLITE_STATIC);
            if (sql == SQLITE_OK)
                sql = sqlite3_bind_int(canonical, 2, canonical_position++);
            if (sql == SQLITE_OK)
                sql = sqlite3_step(canonical) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
        }
        else if (strcmp(item->status, "unmatched") == 0)
            ++result->unmatched;
        else if (strcmp(item->status, "duplicate") == 0)
            ++result->duplicates;
        else
            ++result->invalid;
    }
    sqlite3_finalize(legacy);
    sqlite3_finalize(canonical);
    if (strcmp(kind, "favorite") == 0)
        result->favorites = canonical_position;
    else
        result->recents = canonical_position;
    return sql;
}

int bloom_library_import_legacy(sqlite3 *database, const char *rom_root,
                                const char *favorites_path, const char *recents_path,
                                BloomLibraryLegacyResult *result, char *error,
                                size_t error_size)
{
    if (database == NULL || rom_root == NULL || favorites_path == NULL || recents_path == NULL ||
        result == NULL)
        return SQLITE_MISUSE;
    memset(result, 0, sizeof(*result));
    LegacyList favorites = {calloc(MAX_LEGACY_ITEMS, sizeof(LegacyItem)), 0};
    LegacyList recents = {calloc(MAX_LEGACY_ITEMS, sizeof(LegacyItem)), 0};
    if (favorites.items == NULL || recents.items == NULL) {
        free(favorites.items);
        free(recents.items);
        return SQLITE_NOMEM;
    }
    int sql = load_list(database, rom_root, favorites_path, &favorites, error, error_size);
    if (sql == SQLITE_OK)
        sql = load_list(database, rom_root, recents_path, &recents, error, error_size);
    if (sql == SQLITE_OK)
        sql = sqlite3_exec(database, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    if (sql == SQLITE_OK)
        sql = publish_list(database, "favorite", "favorites", &favorites, result);
    if (sql == SQLITE_OK)
        sql = publish_list(database, "recent", "recents", &recents, result);
    if (sql == SQLITE_OK)
        sql = sqlite3_exec(database, "COMMIT", NULL, NULL, NULL);
    if (sql != SQLITE_OK)
        sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
    free(favorites.items);
    free(recents.items);
    if (sql != SQLITE_OK && error != NULL && error[0] == '\0')
        set_error(error, error_size, "legacy state could not be published");
    return sql;
}
