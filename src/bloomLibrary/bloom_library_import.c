#include "bloom_library_import.h"

#include <cjson/cJSON.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_CONFIG_SIZE (64 * 1024)
#define MAX_SYSTEMS 128
#define MAX_APPS 256
#define ID_SIZE 64
#define TEXT_SIZE 512

typedef struct {
    char folder[ID_SIZE];
    char rom_folder[ID_SIZE];
    char system_id[ID_SIZE];
} SystemMapping;

static void set_error(char *error, size_t size, const char *format, ...)
{
    if (error == NULL || size == 0)
        return;
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error, size, format, arguments);
    va_end(arguments);
}

static int safe_text(const char *value, size_t maximum, int allow_space)
{
    if (value == NULL || value[0] == '\0' || strlen(value) >= maximum)
        return 0;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor) {
        if (*cursor < 0x20 || *cursor == 0x7f || *cursor == '\\' ||
            (!allow_space && *cursor == ' '))
            return 0;
    }
    return 1;
}

static int valid_folder(const char *value)
{
    if (!safe_text(value, ID_SIZE, 0))
        return 0;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor)
        if (!((*cursor >= 'A' && *cursor <= 'Z') || (*cursor >= '0' && *cursor <= '9') ||
              *cursor == '_'))
            return 0;
    return 1;
}

static int valid_id(const char *value)
{
    if (!safe_text(value, ID_SIZE, 0))
        return 0;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor)
        if (!((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= '0' && *cursor <= '9') ||
              *cursor == '_' || *cursor == '-'))
            return 0;
    return 1;
}

static int path_join(char *output, size_t size, const char *left, const char *right)
{
    int written = snprintf(output, size, "%s/%s", left, right);
    return written > 0 && (size_t)written < size ? 0 : -1;
}

static cJSON *load_object(const char *path, char *error, size_t error_size)
{
    struct stat metadata;
    if (lstat(path, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
        S_ISLNK(metadata.st_mode) || metadata.st_size <= 0 ||
        metadata.st_size > MAX_CONFIG_SIZE) {
        set_error(error, error_size, "library input is unavailable or unsafe");
        return NULL;
    }
    FILE *file = fopen(path, "rb");
    char *buffer = file == NULL ? NULL : calloc((size_t)metadata.st_size + 1, 1);
    if (buffer == NULL || fread(buffer, 1, (size_t)metadata.st_size, file) !=
                              (size_t)metadata.st_size) {
        set_error(error, error_size, "library input could not be read");
        free(buffer);
        if (file != NULL)
            fclose(file);
        return NULL;
    }
    fclose(file);
    cJSON *object = cJSON_ParseWithLengthOpts(buffer, (size_t)metadata.st_size + 1, NULL, 1);
    free(buffer);
    if (!cJSON_IsObject(object)) {
        cJSON_Delete(object);
        set_error(error, error_size, "library input is not a JSON object");
        return NULL;
    }
    return object;
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

static int read_string(const cJSON *object, const char *name, char *output, size_t size,
                       int required)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    if (value == NULL && !required) {
        output[0] = '\0';
        return 1;
    }
    if (!cJSON_IsString(value) || !safe_text(value->valuestring, size, 1))
        return 0;
    snprintf(output, size, "%s", value->valuestring);
    return 1;
}

static int load_mappings(const char *path, SystemMapping *mappings, size_t *count, char *error,
                         size_t error_size)
{
    cJSON *root = load_object(path, error, error_size);
    if (root == NULL)
        return -1;
    const cJSON *schema = cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON *entries = cJSON_GetObjectItemCaseSensitive(root, "entries");
    if (!object_has_unique_keys(root) || !cJSON_IsNumber(schema) || schema->valueint != 1 ||
        schema->valuedouble != schema->valueint || !cJSON_IsArray(entries)) {
        set_error(error, error_size, "system catalog is invalid");
        cJSON_Delete(root);
        return -1;
    }
    *count = 0;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, entries)
    {
        if (*count >= MAX_SYSTEMS || !cJSON_IsObject(entry) || !object_has_unique_keys(entry) ||
            !read_string(entry, "folder", mappings[*count].folder,
                         sizeof(mappings[*count].folder), 1) ||
            !read_string(entry, "rom_folder", mappings[*count].rom_folder,
                         sizeof(mappings[*count].rom_folder), 0) ||
            !read_string(entry, "system_id", mappings[*count].system_id,
                         sizeof(mappings[*count].system_id), 1) ||
            !valid_folder(mappings[*count].folder) ||
            (mappings[*count].rom_folder[0] != '\0' &&
             !valid_folder(mappings[*count].rom_folder)) ||
            !valid_id(mappings[*count].system_id)) {
            set_error(error, error_size, "system catalog entry is invalid");
            cJSON_Delete(root);
            return -1;
        }
        for (size_t previous = 0; previous < *count; ++previous)
            if (strcmp(mappings[previous].folder, mappings[*count].folder) == 0 ||
                strcmp(mappings[previous].system_id, mappings[*count].system_id) == 0) {
                set_error(error, error_size, "system catalog identity is duplicated");
                cJSON_Delete(root);
                return -1;
            }
        ++*count;
    }
    cJSON_Delete(root);
    if (*count == 0) {
        set_error(error, error_size, "system catalog is empty");
        return -1;
    }
    return 0;
}

static int normalize_sd_path(const char *value, const char *relative_prefix,
                             const char *absolute_prefix, char *output, size_t output_size)
{
    const char *relative = NULL;
    if (strncmp(value, relative_prefix, strlen(relative_prefix)) == 0)
        relative = value + strlen(relative_prefix);
    else if (strncmp(value, absolute_prefix, strlen(absolute_prefix)) == 0)
        relative = value + strlen(absolute_prefix);
    if (relative == NULL || !safe_text(relative, output_size, 1) || relative[0] == '/')
        return -1;
    const char *component = relative;
    while (*component != '\0') {
        const char *separator = strchr(component, '/');
        size_t length = separator == NULL ? strlen(component) : (size_t)(separator - component);
        if (length == 0 || (length == 1 && component[0] == '.') ||
            (length == 2 && component[0] == '.' && component[1] == '.'))
            return -1;
        if (separator == NULL)
            break;
        component = separator + 1;
    }
    if (*relative == '\0' || relative[strlen(relative) - 1] == '/')
        return -1;
    snprintf(output, output_size, "%s", relative);
    return 0;
}

static int bind_text(sqlite3_stmt *statement, int index, const char *value)
{
    return sqlite3_bind_text(statement, index, value, -1, SQLITE_TRANSIENT);
}

static int import_system(sqlite3 *database, const SystemMapping *mapping, const char *emu_root,
                         int *changed, int *count, char *error, size_t error_size)
{
    char directory[PATH_MAX];
    char config_path[PATH_MAX];
    if (path_join(directory, sizeof(directory), emu_root, mapping->folder) != 0 ||
        path_join(config_path, sizeof(config_path), directory, "config.json") != 0)
        return SQLITE_TOOBIG;
    struct stat directory_metadata;
    if (lstat(directory, &directory_metadata) != 0)
        return errno == ENOENT ? SQLITE_OK : SQLITE_CANTOPEN;
    if (!S_ISDIR(directory_metadata.st_mode) || S_ISLNK(directory_metadata.st_mode)) {
        set_error(error, error_size, "emulator directory is unsafe");
        return SQLITE_CANTOPEN;
    }
    struct stat config_metadata;
    if (lstat(config_path, &config_metadata) != 0 || !S_ISREG(config_metadata.st_mode) ||
        S_ISLNK(config_metadata.st_mode)) {
        set_error(error, error_size, "emulator config is unavailable or unsafe");
        return SQLITE_CANTOPEN;
    }
    cJSON *config = load_object(config_path, error, error_size);
    char label[TEXT_SIZE];
    char rom_raw[TEXT_SIZE];
    char image_raw[TEXT_SIZE];
    char launch_raw[TEXT_SIZE];
    char extensions[TEXT_SIZE];
    char rom_path[TEXT_SIZE];
    char image_path[TEXT_SIZE];
    char launch_path[TEXT_SIZE];
    const char *expected_rom_folder =
        mapping->rom_folder[0] == '\0' ? mapping->folder : mapping->rom_folder;
    if (config == NULL || !object_has_unique_keys(config) ||
        !read_string(config, "label", label, sizeof(label), 1) ||
        !read_string(config, "rompath", rom_raw, sizeof(rom_raw), 1) ||
        !read_string(config, "imgpath", image_raw, sizeof(image_raw), 0) ||
        !read_string(config, "launch", launch_raw, sizeof(launch_raw), 1) ||
        !read_string(config, "extlist", extensions, sizeof(extensions), 1) ||
        normalize_sd_path(rom_raw, "../../Roms/", "/mnt/SDCARD/Roms/", rom_path,
                          sizeof(rom_path)) != 0 ||
        (strcmp(rom_path, expected_rom_folder) != 0 &&
         !(strncmp(rom_path, expected_rom_folder, strlen(expected_rom_folder)) == 0 &&
           rom_path[strlen(expected_rom_folder)] == '/')) ||
        (image_raw[0] != '\0' &&
         normalize_sd_path(image_raw, "../../Roms/", "/mnt/SDCARD/Roms/", image_path,
                           sizeof(image_path)) != 0)) {
        cJSON_Delete(config);
        set_error(error, error_size, "emulator config fields are invalid");
        return SQLITE_CORRUPT;
    }
    if (strcmp(launch_raw, "launch.sh") == 0)
        snprintf(launch_path, sizeof(launch_path), "Emu/%s/launch.sh", mapping->folder);
    else if (normalize_sd_path(launch_raw, "../../Emu/", "/mnt/SDCARD/Emu/", launch_path,
                               sizeof(launch_path)) != 0) {
        cJSON_Delete(config);
        set_error(error, error_size, "emulator launch path is invalid");
        return SQLITE_CORRUPT;
    }
    cJSON_Delete(config);
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database,
        "INSERT INTO systems(system_id,label,rom_path,img_path,launch_path,extensions,config_size,"
        "config_mtime,present) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,1) "
        "ON CONFLICT(system_id) DO UPDATE SET label=excluded.label,rom_path=excluded.rom_path,"
        "img_path=excluded.img_path,launch_path=excluded.launch_path,extensions=excluded.extensions,"
        "config_size=excluded.config_size,config_mtime=excluded.config_mtime,present=1 "
        "WHERE label IS NOT excluded.label OR rom_path IS NOT excluded.rom_path OR "
        "img_path IS NOT excluded.img_path OR launch_path IS NOT excluded.launch_path OR "
        "extensions IS NOT excluded.extensions OR config_size IS NOT excluded.config_size OR "
        "config_mtime IS NOT excluded.config_mtime OR present<>1",
        -1, &statement, NULL);
    if (result == SQLITE_OK)
        result = bind_text(statement, 1, mapping->system_id);
    if (result == SQLITE_OK)
        result = bind_text(statement, 2, label);
    if (result == SQLITE_OK)
        result = bind_text(statement, 3, rom_path);
    if (result == SQLITE_OK)
        result = image_raw[0] == '\0' ? sqlite3_bind_null(statement, 4)
                                      : bind_text(statement, 4, image_path);
    if (result == SQLITE_OK)
        result = bind_text(statement, 5, launch_path);
    if (result == SQLITE_OK)
        result = bind_text(statement, 6, extensions);
    if (result == SQLITE_OK)
        result = sqlite3_bind_int64(statement, 7, config_metadata.st_size);
    if (result == SQLITE_OK)
        result = sqlite3_bind_int64(statement, 8, config_metadata.st_mtime);
    if (result == SQLITE_OK)
        result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(database);
    sqlite3_finalize(statement);
    if (result == SQLITE_OK) {
        *changed |= sqlite3_changes(database) > 0;
        ++*count;
        statement = NULL;
        result = sqlite3_prepare_v2(database, "INSERT INTO seen_systems VALUES(?1)", -1,
                                    &statement, NULL);
        if (result == SQLITE_OK)
            result = bind_text(statement, 1, mapping->system_id);
        if (result == SQLITE_OK)
            result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(database);
        sqlite3_finalize(statement);
    }
    return result;
}

static int compare_names(const void *left, const void *right)
{
    return strcmp((const char *)left, (const char *)right);
}

static int collect_app_names(const char *app_root, char names[MAX_APPS][ID_SIZE], size_t *count,
                             char *error, size_t error_size)
{
    struct stat root_metadata;
    if (lstat(app_root, &root_metadata) != 0 || !S_ISDIR(root_metadata.st_mode) ||
        S_ISLNK(root_metadata.st_mode)) {
        set_error(error, error_size, "application root is unavailable or unsafe");
        return -1;
    }
    DIR *directory = opendir(app_root);
    if (directory == NULL) {
        set_error(error, error_size, "application root could not be opened");
        return -1;
    }
    *count = 0;
    const struct dirent *entry = NULL;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (!safe_text(entry->d_name, ID_SIZE, 1) || *count >= MAX_APPS) {
            set_error(error, error_size, "application directory name is invalid");
            closedir(directory);
            return -1;
        }
        char path[PATH_MAX];
        char config_path[PATH_MAX];
        struct stat metadata;
        if (path_join(path, sizeof(path), app_root, entry->d_name) != 0 ||
            path_join(config_path, sizeof(config_path), path, "config.json") != 0 ||
            lstat(path, &metadata) != 0) {
            set_error(error, error_size, "application path is invalid");
            closedir(directory);
            return -1;
        }
        if (!S_ISDIR(metadata.st_mode) || S_ISLNK(metadata.st_mode)) {
            set_error(error, error_size, "application directory is unsafe");
            closedir(directory);
            return -1;
        }
        if (lstat(config_path, &metadata) != 0) {
            if (errno == ENOENT)
                continue;
            set_error(error, error_size, "application config could not be inspected");
            closedir(directory);
            return -1;
        }
        if (!S_ISREG(metadata.st_mode) || S_ISLNK(metadata.st_mode)) {
            set_error(error, error_size, "application config is unsafe");
            closedir(directory);
            return -1;
        }
        memcpy(names[*count], entry->d_name, strlen(entry->d_name) + 1);
        ++*count;
    }
    closedir(directory);
    qsort(names, *count, ID_SIZE, compare_names);
    return 0;
}

static int import_app(sqlite3 *database, const char *name, const char *app_root, int *changed,
                      int *count, char *error, size_t error_size)
{
    char directory[PATH_MAX];
    char config_path[PATH_MAX];
    struct stat config_metadata;
    if (path_join(directory, sizeof(directory), app_root, name) != 0 ||
        path_join(config_path, sizeof(config_path), directory, "config.json") != 0 ||
        lstat(config_path, &config_metadata) != 0 || !S_ISREG(config_metadata.st_mode) ||
        S_ISLNK(config_metadata.st_mode))
        return SQLITE_CANTOPEN;
    cJSON *config = load_object(config_path, error, error_size);
    char label[TEXT_SIZE];
    char launch_raw[TEXT_SIZE];
    char icon_raw[TEXT_SIZE];
    char launch_path[TEXT_SIZE];
    char icon_path[TEXT_SIZE];
    char app_id[ID_SIZE + 16];
    if (config == NULL || !object_has_unique_keys(config) ||
        !read_string(config, "label", label, sizeof(label), 1) ||
        !read_string(config, "launch", launch_raw, sizeof(launch_raw), 1) ||
        !read_string(config, "icon", icon_raw, sizeof(icon_raw), 0) ||
        snprintf(app_id, sizeof(app_id), "bloom-app-v1:%s", name) >= (int)sizeof(app_id)) {
        cJSON_Delete(config);
        set_error(error, error_size, "application config fields are invalid");
        return SQLITE_CORRUPT;
    }
    if (strcmp(launch_raw, "launch.sh") == 0)
        snprintf(launch_path, sizeof(launch_path), "App/%s/launch.sh", name);
    else if (normalize_sd_path(launch_raw, "../../App/", "/mnt/SDCARD/App/", launch_path,
                               sizeof(launch_path)) != 0) {
        cJSON_Delete(config);
        set_error(error, error_size, "application launch path is invalid");
        return SQLITE_CORRUPT;
    }
    if (icon_raw[0] != '\0') {
        char relative_icon[TEXT_SIZE];
        if (normalize_sd_path(icon_raw, "../../Icons/", "/mnt/SDCARD/Icons/", relative_icon,
                              sizeof(relative_icon)) != 0 ||
            snprintf(icon_path, sizeof(icon_path), "Icons/%s", relative_icon) >=
                (int)sizeof(icon_path)) {
            cJSON_Delete(config);
            set_error(error, error_size, "application icon path is invalid");
            return SQLITE_CORRUPT;
        }
    }
    cJSON_Delete(config);
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database,
        "INSERT INTO apps(app_id,label,launch_path,icon_path,config_size,config_mtime,present) "
        "VALUES(?1,?2,?3,?4,?5,?6,1) ON CONFLICT(app_id) DO UPDATE SET label=excluded.label,"
        "launch_path=excluded.launch_path,icon_path=excluded.icon_path,config_size=excluded.config_size,"
        "config_mtime=excluded.config_mtime,present=1 WHERE label IS NOT excluded.label OR "
        "launch_path IS NOT excluded.launch_path OR icon_path IS NOT excluded.icon_path OR "
        "config_size IS NOT excluded.config_size OR config_mtime IS NOT excluded.config_mtime OR present<>1",
        -1, &statement, NULL);
    if (result == SQLITE_OK)
        result = bind_text(statement, 1, app_id);
    if (result == SQLITE_OK)
        result = bind_text(statement, 2, label);
    if (result == SQLITE_OK)
        result = bind_text(statement, 3, launch_path);
    if (result == SQLITE_OK)
        result = icon_raw[0] == '\0' ? sqlite3_bind_null(statement, 4)
                                     : bind_text(statement, 4, icon_path);
    if (result == SQLITE_OK)
        result = sqlite3_bind_int64(statement, 5, config_metadata.st_size);
    if (result == SQLITE_OK)
        result = sqlite3_bind_int64(statement, 6, config_metadata.st_mtime);
    if (result == SQLITE_OK)
        result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(database);
    sqlite3_finalize(statement);
    if (result == SQLITE_OK) {
        *changed |= sqlite3_changes(database) > 0;
        ++*count;
        statement = NULL;
        result = sqlite3_prepare_v2(database, "INSERT INTO seen_apps VALUES(?1)", -1, &statement,
                                    NULL);
        if (result == SQLITE_OK)
            result = bind_text(statement, 1, app_id);
        if (result == SQLITE_OK)
            result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(database);
        sqlite3_finalize(statement);
    }
    return result;
}

static int state_generation(sqlite3 *database, int *generation)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(database, "SELECT generation FROM library_state WHERE id=1",
                                    -1, &statement, NULL);
    if (result == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW &&
        sqlite3_column_type(statement, 0) == SQLITE_INTEGER)
        *generation = sqlite3_column_int(statement, 0);
    else if (result == SQLITE_OK)
        result = SQLITE_CORRUPT;
    sqlite3_finalize(statement);
    return result;
}

int bloom_library_import_onion(sqlite3 *database, const char *system_catalog_path,
                               const char *emu_root, const char *app_root,
                               BloomLibraryImportResult *result, char *error, size_t error_size)
{
    if (database == NULL || system_catalog_path == NULL || emu_root == NULL || app_root == NULL ||
        result == NULL) {
        set_error(error, error_size, "library import request is invalid");
        return SQLITE_MISUSE;
    }
    memset(result, 0, sizeof(*result));
    struct stat metadata;
    if (lstat(emu_root, &metadata) != 0 || !S_ISDIR(metadata.st_mode) ||
        S_ISLNK(metadata.st_mode)) {
        set_error(error, error_size, "emulator root is unavailable or unsafe");
        return SQLITE_CANTOPEN;
    }
    SystemMapping mappings[MAX_SYSTEMS];
    size_t mapping_count = 0;
    if (load_mappings(system_catalog_path, mappings, &mapping_count, error, error_size) != 0)
        return SQLITE_CORRUPT;
    char app_names[MAX_APPS][ID_SIZE];
    size_t app_count = 0;
    if (collect_app_names(app_root, app_names, &app_count, error, error_size) != 0)
        return SQLITE_CANTOPEN;
    int sql = sqlite3_exec(database,
                           "BEGIN IMMEDIATE;DROP TABLE IF EXISTS temp.seen_systems;"
                           "DROP TABLE IF EXISTS temp.seen_apps;"
                           "CREATE TEMP TABLE seen_systems(system_id TEXT PRIMARY KEY);"
                           "CREATE TEMP TABLE seen_apps(app_id TEXT PRIMARY KEY);",
                           NULL, NULL, NULL);
    if (sql != SQLITE_OK)
        return sql;
    int changed = 0;
    for (size_t index = 0; sql == SQLITE_OK && index < mapping_count; ++index)
        sql = import_system(database, &mappings[index], emu_root, &changed, &result->systems,
                            error, error_size);
    for (size_t index = 0; sql == SQLITE_OK && index < app_count; ++index)
        sql = import_app(database, app_names[index], app_root, &changed, &result->apps, error,
                         error_size);
    if (sql == SQLITE_OK && (result->systems == 0 || result->apps == 0)) {
        set_error(error, error_size, "library import cannot publish an empty catalog");
        sql = SQLITE_CORRUPT;
    }
    if (sql == SQLITE_OK) {
        int changes_before = sqlite3_total_changes(database);
        sql = sqlite3_exec(
            database,
            "UPDATE systems SET present=0 WHERE present=1 AND NOT EXISTS("
            "SELECT 1 FROM seen_systems s WHERE s.system_id=systems.system_id);"
            "UPDATE apps SET present=0 WHERE present=1 AND NOT EXISTS("
            "SELECT 1 FROM seen_apps a WHERE a.app_id=apps.app_id);",
            NULL, NULL, NULL);
        changed |= sql == SQLITE_OK && sqlite3_total_changes(database) > changes_before;
    }
    if (sql == SQLITE_OK) {
        sql = changed ? sqlite3_exec(database,
                                     "UPDATE library_state SET generation=generation+1,"
                                     "status='ready' WHERE id=1",
                                     NULL, NULL, NULL)
                      : sqlite3_exec(database,
                                     "UPDATE library_state SET status='ready' WHERE id=1 AND "
                                     "status<>'ready'",
                                     NULL, NULL, NULL);
    }
    if (sql == SQLITE_OK)
        sql = state_generation(database, &result->generation);
    if (sql == SQLITE_OK)
        sql = sqlite3_exec(database, "COMMIT", NULL, NULL, NULL);
    if (sql != SQLITE_OK) {
        sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
        if (error != NULL && error[0] == '\0')
            set_error(error, error_size, "library import could not be published");
        return sql;
    }
    result->changed = changed;
    return SQLITE_OK;
}
