#include "bloom_library_scan.h"

#include "../bloomGameId/bloom_game_id.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define MAX_SYSTEMS 128
#define ID_SIZE 64
#define TEXT_SIZE 512
#define MAX_SCAN_DEPTH 16
#define CANONICAL_ROM_ROOT "/mnt/SDCARD/Roms"

typedef struct {
    char system_id[ID_SIZE];
    char rom_path[TEXT_SIZE];
    char image_path[TEXT_SIZE];
    char extensions[TEXT_SIZE];
} ScanSystem;

static void set_error(char *error, size_t size, const char *format, ...)
{
    if (error == NULL || size == 0)
        return;
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error, size, format, arguments);
    va_end(arguments);
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

static int valid_system_id(const char *value)
{
    if (value == NULL || value[0] == '\0' || strlen(value) >= ID_SIZE)
        return 0;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor)
        if (!((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= '0' && *cursor <= '9') ||
              *cursor == '_' || *cursor == '-'))
            return 0;
    return 1;
}

static int valid_extensions(const char *value)
{
    if (value == NULL || value[0] == '\0' || strlen(value) >= TEXT_SIZE)
        return 0;
    const char *token = value;
    while (*token != '\0') {
        const char *separator = strchr(token, '|');
        size_t length = separator == NULL ? strlen(token) : (size_t)(separator - token);
        if (length == 0 || length > 16)
            return 0;
        for (size_t index = 0; index < length; ++index) {
            unsigned char byte = (unsigned char)token[index];
            if (!((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
                  (byte >= '0' && byte <= '9')))
                return 0;
        }
        if (separator == NULL)
            return 1;
        token = separator + 1;
    }
    return 0;
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

static int load_systems(sqlite3 *database, const char *only_system, ScanSystem *systems,
                        size_t *count)
{
    sqlite3_stmt *statement = NULL;
    int sql = sqlite3_prepare_v2(
        database,
        "SELECT system_id,rom_path,img_path,extensions FROM systems WHERE present=1 "
        "AND (?1 IS NULL OR system_id=?1) ORDER BY system_id",
        -1, &statement, NULL);
    if (sql == SQLITE_OK) {
        if (only_system == NULL)
            sql = sqlite3_bind_null(statement, 1);
        else
            sql = sqlite3_bind_text(statement, 1, only_system, -1, SQLITE_STATIC);
    }
    *count = 0;
    int step = SQLITE_DONE;
    while (sql == SQLITE_OK && (step = sqlite3_step(statement)) == SQLITE_ROW) {
        if (*count >= MAX_SYSTEMS ||
            (sql = copy_column(statement, 0, systems[*count].system_id,
                               sizeof(systems[*count].system_id), 0)) != SQLITE_OK ||
            (sql = copy_column(statement, 1, systems[*count].rom_path,
                               sizeof(systems[*count].rom_path), 0)) != SQLITE_OK ||
            (sql = copy_column(statement, 2, systems[*count].image_path,
                               sizeof(systems[*count].image_path), 1)) != SQLITE_OK ||
            (sql = copy_column(statement, 3, systems[*count].extensions,
                               sizeof(systems[*count].extensions), 0)) != SQLITE_OK ||
            !valid_system_id(systems[*count].system_id) ||
            !safe_relative_path(systems[*count].rom_path) ||
            !valid_extensions(systems[*count].extensions) ||
            (systems[*count].image_path[0] != '\0' &&
             !safe_relative_path(systems[*count].image_path))) {
            if (sql == SQLITE_OK)
                sql = SQLITE_CORRUPT;
            break;
        }
        ++*count;
    }
    if (sql == SQLITE_OK && step != SQLITE_DONE)
        sql = step;
    sqlite3_finalize(statement);
    if (sql == SQLITE_OK && *count == 0)
        sql = SQLITE_NOTFOUND;
    return sql;
}

static int path_join(char *output, size_t size, const char *left, const char *right)
{
    int written = snprintf(output, size, "%s/%s", left, right);
    return written > 0 && (size_t)written < size ? SQLITE_OK : SQLITE_TOOBIG;
}

static int extension_supported(const char *name, const char *extensions)
{
    const char *dot = strrchr(name, '.');
    if (dot == NULL || dot[1] == '\0')
        return 0;
    const char *wanted = dot + 1;
    const char *cursor = extensions;
    while (*cursor != '\0') {
        const char *separator = strchr(cursor, '|');
        size_t length = separator == NULL ? strlen(cursor) : (size_t)(separator - cursor);
        if (length == strlen(wanted) && strncasecmp(cursor, wanted, length) == 0)
            return 1;
        if (separator == NULL)
            break;
        cursor = separator + 1;
    }
    return 0;
}

static void game_titles(const char *name, char *display, size_t display_size, char *sort,
                        size_t sort_size)
{
    size_t length = strlen(name);
    const char *dot = strrchr(name, '.');
    if (dot != NULL && dot != name)
        length = (size_t)(dot - name);
    if (length >= display_size)
        length = display_size - 1;
    memcpy(display, name, length);
    display[length] = '\0';
    size_t sort_length = length < sort_size - 1 ? length : sort_size - 1;
    for (size_t index = 0; index < sort_length; ++index) {
        unsigned char byte = (unsigned char)display[index];
        sort[index] = byte >= 'A' && byte <= 'Z' ? (char)(byte + ('a' - 'A')) : (char)byte;
    }
    sort[sort_length] = '\0';
}

static int find_image(const ScanSystem *system, const char *rom_root, const char *name, char *image,
                      size_t image_size)
{
    image[0] = '\0';
    if (system->image_path[0] == '\0' || strcmp(system->image_path, system->rom_path) == 0)
        return SQLITE_OK;
    char display[TEXT_SIZE];
    char ignored[TEXT_SIZE];
    game_titles(name, display, sizeof(display), ignored, sizeof(ignored));
    char relative[TEXT_SIZE];
    if (snprintf(relative, sizeof(relative), "%s/%s.png", system->image_path, display) >=
        (int)sizeof(relative))
        return SQLITE_OK;
    char absolute[PATH_MAX];
    if (path_join(absolute, sizeof(absolute), rom_root, relative) != SQLITE_OK)
        return SQLITE_OK;
    struct stat metadata;
    if (lstat(absolute, &metadata) == 0 && S_ISREG(metadata.st_mode) &&
        !S_ISLNK(metadata.st_mode))
        snprintf(image, image_size, "%s", relative);
    return SQLITE_OK;
}

static int bind_text(sqlite3_stmt *statement, int index, const char *value)
{
    return sqlite3_bind_text(statement, index, value, -1, SQLITE_TRANSIENT);
}

static int store_game(sqlite3 *database, const ScanSystem *system, const char *rom_root,
                      const char *relative, const char *name, const struct stat *metadata,
                      int *changed)
{
    char canonical[PATH_MAX];
    if (path_join(canonical, sizeof(canonical), CANONICAL_ROM_ROOT, relative) != SQLITE_OK)
        return SQLITE_TOOBIG;
    char game_id[BLOOM_GAME_ID_LENGTH + 1];
    char normalized[PATH_MAX];
    char error[128];
    if (bloom_game_id_create(system->system_id, canonical, game_id, sizeof(game_id), normalized,
                             sizeof(normalized), error, sizeof(error)) != 0 ||
        strcmp(normalized, relative) != 0)
        return SQLITE_CORRUPT;
    char display[TEXT_SIZE];
    char sort[TEXT_SIZE];
    char image[TEXT_SIZE];
    game_titles(name, display, sizeof(display), sort, sizeof(sort));
    find_image(system, rom_root, name, image, sizeof(image));

    sqlite3_stmt *statement = NULL;
    int sql = sqlite3_prepare_v2(
        database,
        "INSERT INTO games(bloom_game_id,system_id,normalized_rom_path,display_title,sort_title,"
        "image_path,file_size,file_mtime,present) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,1) "
        "ON CONFLICT(bloom_game_id) DO UPDATE SET system_id=excluded.system_id,"
        "normalized_rom_path=excluded.normalized_rom_path,display_title=excluded.display_title,"
        "sort_title=excluded.sort_title,image_path=excluded.image_path,file_size=excluded.file_size,"
        "file_mtime=excluded.file_mtime,present=1 WHERE system_id IS NOT excluded.system_id OR "
        "normalized_rom_path IS NOT excluded.normalized_rom_path OR display_title IS NOT "
        "excluded.display_title OR sort_title IS NOT excluded.sort_title OR image_path IS NOT "
        "excluded.image_path OR file_size IS NOT excluded.file_size OR file_mtime IS NOT "
        "excluded.file_mtime OR present<>1",
        -1, &statement, NULL);
    if (sql == SQLITE_OK)
        sql = bind_text(statement, 1, game_id);
    if (sql == SQLITE_OK)
        sql = bind_text(statement, 2, system->system_id);
    if (sql == SQLITE_OK)
        sql = bind_text(statement, 3, relative);
    if (sql == SQLITE_OK)
        sql = bind_text(statement, 4, display);
    if (sql == SQLITE_OK)
        sql = bind_text(statement, 5, sort);
    if (sql == SQLITE_OK)
        sql = image[0] == '\0' ? sqlite3_bind_null(statement, 6) : bind_text(statement, 6, image);
    if (sql == SQLITE_OK)
        sql = sqlite3_bind_int64(statement, 7, metadata->st_size);
    if (sql == SQLITE_OK)
        sql = sqlite3_bind_int64(statement, 8, metadata->st_mtime);
    if (sql == SQLITE_OK)
        sql = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(database);
    sqlite3_finalize(statement);
    if (sql == SQLITE_OK) {
        *changed |= sqlite3_changes(database) > 0;
        statement = NULL;
        sql = sqlite3_prepare_v2(database, "INSERT INTO seen_games VALUES(?1)", -1, &statement,
                                 NULL);
        if (sql == SQLITE_OK)
            sql = bind_text(statement, 1, game_id);
        if (sql == SQLITE_OK)
            sql = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(database);
        sqlite3_finalize(statement);
    }
    return sql;
}

static int ignored_directory(const char *name)
{
    return name[0] == '.' || strcmp(name, "Imgs") == 0 || strcmp(name, "images") == 0 ||
           strcmp(name, "Snaps") == 0;
}

static int scan_directory(sqlite3 *database, const ScanSystem *system, const char *rom_root,
                          const char *absolute, const char *relative, unsigned int depth,
                          BloomLibraryScanResult *result)
{
    if (depth > MAX_SCAN_DEPTH)
        return SQLITE_TOOBIG;
    DIR *directory = opendir(absolute);
    if (directory == NULL)
        return errno == ENOENT ? SQLITE_OK : SQLITE_CANTOPEN;
    int sql = SQLITE_OK;
    const struct dirent *entry = NULL;
    while (sql == SQLITE_OK && (entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
        char child_absolute[PATH_MAX];
        char child_relative[PATH_MAX];
        if (path_join(child_absolute, sizeof(child_absolute), absolute, entry->d_name) != SQLITE_OK ||
            path_join(child_relative, sizeof(child_relative), relative, entry->d_name) != SQLITE_OK) {
            ++result->errors;
            continue;
        }
        struct stat metadata;
        if (lstat(child_absolute, &metadata) != 0 || S_ISLNK(metadata.st_mode)) {
            ++result->errors;
            continue;
        }
        if (S_ISDIR(metadata.st_mode)) {
            if (!ignored_directory(entry->d_name))
                sql = scan_directory(database, system, rom_root, child_absolute, child_relative,
                                     depth + 1, result);
        }
        else if (S_ISREG(metadata.st_mode) && extension_supported(entry->d_name, system->extensions)) {
            sql = store_game(database, system, rom_root, child_relative, entry->d_name, &metadata,
                             &result->changed);
            if (sql == SQLITE_OK)
                ++result->games;
        }
    }
    closedir(directory);
    return sql;
}

static int state_generation(sqlite3 *database, int *generation)
{
    sqlite3_stmt *statement = NULL;
    int sql = sqlite3_prepare_v2(database, "SELECT generation FROM library_state WHERE id=1", -1,
                                 &statement, NULL);
    if (sql == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW &&
        sqlite3_column_type(statement, 0) == SQLITE_INTEGER)
        *generation = sqlite3_column_int(statement, 0);
    else if (sql == SQLITE_OK)
        sql = SQLITE_CORRUPT;
    sqlite3_finalize(statement);
    return sql;
}

int bloom_library_scan_games(sqlite3 *database, const char *rom_root, const char *system_id,
                             BloomLibraryScanResult *result, char *error, size_t error_size)
{
    if (database == NULL || rom_root == NULL || result == NULL ||
        (system_id != NULL && !valid_system_id(system_id))) {
        set_error(error, error_size, "library scan request is invalid");
        return SQLITE_MISUSE;
    }
    memset(result, 0, sizeof(*result));
    struct stat root_metadata;
    if (lstat(rom_root, &root_metadata) != 0 || !S_ISDIR(root_metadata.st_mode) ||
        S_ISLNK(root_metadata.st_mode)) {
        set_error(error, error_size, "ROM root is unavailable or unsafe");
        return SQLITE_CANTOPEN;
    }
    ScanSystem systems[MAX_SYSTEMS];
    size_t system_count = 0;
    int sql = load_systems(database, system_id, systems, &system_count);
    if (sql != SQLITE_OK) {
        set_error(error, error_size, "library systems are unavailable");
        return sql;
    }
    sql = sqlite3_exec(database,
                       "BEGIN IMMEDIATE;DROP TABLE IF EXISTS temp.seen_games;"
                       "DROP TABLE IF EXISTS temp.scanned_systems;"
                       "CREATE TEMP TABLE seen_games(bloom_game_id TEXT PRIMARY KEY);"
                       "CREATE TEMP TABLE scanned_systems(system_id TEXT PRIMARY KEY);",
                       NULL, NULL, NULL);
    for (size_t index = 0; sql == SQLITE_OK && index < system_count; ++index) {
        char absolute[PATH_MAX];
        if (path_join(absolute, sizeof(absolute), rom_root, systems[index].rom_path) != SQLITE_OK) {
            sql = SQLITE_TOOBIG;
            break;
        }
        struct stat metadata;
        if (lstat(absolute, &metadata) != 0) {
            if (errno != ENOENT) {
                sql = SQLITE_CANTOPEN;
                break;
            }
        }
        else if (!S_ISDIR(metadata.st_mode) || S_ISLNK(metadata.st_mode)) {
            sql = SQLITE_CANTOPEN;
            break;
        }
        else {
            sql = scan_directory(database, &systems[index], rom_root, absolute,
                                 systems[index].rom_path, 0, result);
        }
        if (sql == SQLITE_OK) {
            sqlite3_stmt *statement = NULL;
            sql = sqlite3_prepare_v2(database, "INSERT INTO scanned_systems VALUES(?1)", -1,
                                     &statement, NULL);
            if (sql == SQLITE_OK)
                sql = bind_text(statement, 1, systems[index].system_id);
            if (sql == SQLITE_OK)
                sql = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK
                                                             : sqlite3_errcode(database);
            sqlite3_finalize(statement);
            ++result->systems;
        }
    }
    if (sql == SQLITE_OK) {
        int changes_before = sqlite3_total_changes(database);
        sql = sqlite3_exec(
            database,
            "UPDATE games SET present=0 WHERE present=1 AND EXISTS(SELECT 1 FROM scanned_systems "
            "s WHERE s.system_id=games.system_id) AND NOT EXISTS(SELECT 1 FROM seen_games g WHERE "
            "g.bloom_game_id=games.bloom_game_id)",
            NULL, NULL, NULL);
        result->changed |= sql == SQLITE_OK && sqlite3_total_changes(database) > changes_before;
    }
    if (sql == SQLITE_OK && result->changed)
        sql = sqlite3_exec(database, "UPDATE library_state SET generation=generation+1 WHERE id=1",
                           NULL, NULL, NULL);
    if (sql == SQLITE_OK)
        sql = state_generation(database, &result->generation);
    if (sql == SQLITE_OK)
        sql = sqlite3_exec(database, "COMMIT", NULL, NULL, NULL);
    if (sql != SQLITE_OK) {
        sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
        set_error(error, error_size, "library scan could not be published");
        return sql;
    }
    return SQLITE_OK;
}
