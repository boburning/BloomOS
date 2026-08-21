#include "bloom_settings.h"

#include "cjson/cJSON.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#define BLOOM_SETTINGS_MAX_FILE (256U * 1024U)

static void set_error(char *error, size_t size, const char *message)
{
    if (error != NULL && size > 0)
        snprintf(error, size, "%s", message);
}

static int safe_identifier(const char *value, size_t maximum)
{
    if (value == NULL || value[0] == '\0' || strlen(value) >= maximum)
        return 0;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor; cursor++)
        if (!((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= '0' && *cursor <= '9') ||
              *cursor == '_' || *cursor == '-'))
            return 0;
    return 1;
}

static int read_regular_file(const char *path, char **content, size_t *length, int optional)
{
    struct stat metadata;
    if (lstat(path, &metadata) != 0)
        return optional && errno == ENOENT ? 1 : -1;
    if (!S_ISREG(metadata.st_mode) || S_ISLNK(metadata.st_mode) || metadata.st_size < 0 ||
        (size_t)metadata.st_size > BLOOM_SETTINGS_MAX_FILE)
        return -1;
    int descriptor = open(path, O_RDONLY | O_NOFOLLOW);
    if (descriptor < 0)
        return -1;
    size_t expected = (size_t)metadata.st_size;
    char *buffer = malloc(expected + 1);
    if (buffer == NULL) {
        close(descriptor);
        return -1;
    }
    size_t consumed = 0;
    while (consumed < expected) {
        ssize_t count = read(descriptor, buffer + consumed, expected - consumed);
        if (count <= 0) {
            free(buffer);
            close(descriptor);
            return -1;
        }
        consumed += (size_t)count;
    }
    char extra;
    ssize_t overflow = read(descriptor, &extra, 1);
    close(descriptor);
    if (overflow != 0) {
        free(buffer);
        return -1;
    }
    buffer[consumed] = '\0';
    *content = buffer;
    *length = consumed;
    return 0;
}

static int write_atomic(const char *path, const char *content, size_t length)
{
    char temporary[PATH_MAX];
    if (path == NULL || content == NULL ||
        snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", path, (long)getpid()) >=
            (int)sizeof(temporary))
        return -1;
    int descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (descriptor < 0)
        return -1;
    size_t written = 0;
    while (written < length) {
        ssize_t count = write(descriptor, content + written, length - written);
        if (count <= 0) {
            close(descriptor);
            unlink(temporary);
            return -1;
        }
        written += (size_t)count;
    }
    if (fsync(descriptor) != 0 || close(descriptor) != 0 || rename(temporary, path) != 0) {
        unlink(temporary);
        return -1;
    }
    chmod(path, 0600);
    return 0;
}

static int acquire_settings_lock(const char *settings_path, int *descriptor, char *error,
                                 size_t error_size)
{
    char lock_path[PATH_MAX];
    if (snprintf(lock_path, sizeof(lock_path), "%s.lock", settings_path) >= (int)sizeof(lock_path)) {
        set_error(error, error_size, "settings lock path is invalid");
        return -1;
    }
    int lock = open(lock_path, O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
    if (lock < 0) {
        set_error(error, error_size, "settings lock is unavailable");
        return -1;
    }
    struct stat metadata;
    if (fstat(lock, &metadata) != 0 || !S_ISREG(metadata.st_mode) || flock(lock, LOCK_EX) != 0) {
        close(lock);
        set_error(error, error_size, "settings lock is invalid");
        return -1;
    }
    *descriptor = lock;
    return 0;
}

static void release_settings_lock(int descriptor)
{
    if (descriptor >= 0) {
        flock(descriptor, LOCK_UN);
        close(descriptor);
    }
}

static cJSON *parse_optional_object(const char *path, char **raw, size_t *raw_length, int *missing)
{
    *raw = NULL;
    *raw_length = 0;
    *missing = 0;
    int result = read_regular_file(path, raw, raw_length, 1);
    if (result == 1) {
        *missing = 1;
        return cJSON_CreateObject();
    }
    if (result != 0)
        return NULL;
    cJSON *root = cJSON_ParseWithLength(*raw, *raw_length);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static int bounded_integer(const cJSON *object, const char *key, int fallback, int minimum,
                           int maximum)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsNumber(value) || value->valuedouble != value->valueint || value->valueint < minimum ||
        value->valueint > maximum)
        return fallback;
    return value->valueint;
}

static const char *bounded_string(const cJSON *object, const char *key, const char *fallback)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsString(value) || value->valuestring[0] == '\0' || strlen(value->valuestring) >= 256)
        return fallback;
    return value->valuestring;
}

static int path_join(char *output, size_t size, const char *root, const char *relative)
{
    if (root == NULL || relative == NULL || strstr(relative, "..") != NULL || relative[0] == '/')
        return -1;
    int separator = root[0] != '\0' && root[strlen(root) - 1] != '/';
    return snprintf(output, size, "%s%s%s", root, separator ? "/" : "", relative) < (int)size ? 0
                                                                                              : -1;
}

static int flag_exists(const char *root, const char *name)
{
    char path[PATH_MAX];
    struct stat metadata;
    return path_join(path, sizeof(path), root, name) == 0 && lstat(path, &metadata) == 0 &&
           S_ISREG(metadata.st_mode) && !S_ISLNK(metadata.st_mode);
}

static int read_config_integer(const char *root, const char *name, int fallback, int minimum,
                               int maximum)
{
    char path[PATH_MAX];
    char *content = NULL;
    size_t length = 0;
    if (path_join(path, sizeof(path), root, name) != 0 ||
        read_regular_file(path, &content, &length, 1) != 0)
        return fallback;
    char *end = NULL;
    errno = 0;
    long value = strtol(content, &end, 10);
    while (end != NULL && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t'))
        end++;
    int valid = errno == 0 && end != content && end != NULL && *end == '\0' && value >= minimum &&
                value <= maximum;
    free(content);
    return valid ? (int)value : fallback;
}

static void add_integer(cJSON *object, const char *name, int value)
{
    cJSON_AddNumberToObject(object, name, value);
}

static void add_boolean(cJSON *object, const char *name, int value)
{
    cJSON_AddBoolToObject(object, name, value != 0);
}

static cJSON *build_settings(const cJSON *system, const cJSON *keymap, const char *config_root,
                             int used_defaults)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *source = cJSON_CreateObject();
    cJSON *device = cJSON_CreateObject();
    cJSON *interface = cJSON_CreateObject();
    cJSON *behavior = cJSON_CreateObject();
    cJSON *compatibility = cJSON_CreateObject();
    if (root == NULL || source == NULL || device == NULL || interface == NULL || behavior == NULL ||
        compatibility == NULL)
        goto failure;

    cJSON_AddNumberToObject(root, "schema", BLOOM_SETTINGS_SCHEMA);
    cJSON_AddNumberToObject(root, "generation", 1);
    cJSON_AddStringToObject(root, "authority", "legacy");
    cJSON_AddStringToObject(source, "kind", "onion");
    cJSON_AddBoolToObject(source, "used_defaults", used_defaults != 0);
    cJSON_AddItemToObject(root, "source", source);
    source = NULL;

    add_integer(device, "volume", bounded_integer(system, "vol", 20, 0, 20));
    add_boolean(device, "mute", flag_exists(config_root, ".muteVolume") || bounded_integer(system, "mute", 0, 0, 1));
    add_integer(device, "brightness", bounded_integer(system, "brightness", 7, 0, 10));
    add_boolean(device, "wifi_enabled", bounded_integer(system, "wifi", 0, 0, 1));
    add_integer(device, "sleep_minutes", bounded_integer(system, "hibernate", 5, 0, 120));
    add_integer(device, "vibration", read_config_integer(config_root, "vibration", 2, 0, 3));
    cJSON_AddItemToObject(root, "device", device);
    device = NULL;

    cJSON_AddStringToObject(interface, "language",
                            bounded_string(system, "language", "en.lang"));
    cJSON_AddStringToObject(interface, "theme", bounded_string(system, "theme", "./"));
    add_integer(interface, "font_size", bounded_integer(system, "fontsize", 24, 8, 64));
    add_boolean(interface, "show_recents", flag_exists(config_root, ".showRecents"));
    add_boolean(interface, "show_expert", flag_exists(config_root, ".showExpert"));
    cJSON_AddItemToObject(root, "interface", interface);
    interface = NULL;

    add_boolean(behavior, "startup_auto_resume", !flag_exists(config_root, ".noAutoStart"));
    add_boolean(behavior, "menu_button_haptics", !flag_exists(config_root, ".noMenuHaptics"));
    add_boolean(behavior, "disable_standby", flag_exists(config_root, ".disableStandby"));
    add_boolean(behavior, "logging", flag_exists(config_root, ".logging"));
    add_integer(behavior, "low_battery_warn_at",
                read_config_integer(config_root, "battery/warnAt", 10, 0, 100));
    add_integer(behavior, "low_battery_autosave_at",
                read_config_integer(config_root, "battery/exitAt", 4, 0, 100));
    add_integer(behavior, "startup_tab", read_config_integer(config_root, "startup/tab", 0, 0, 20));
    add_integer(behavior, "startup_application",
                read_config_integer(config_root, "startup/app", 0, 0, 20));
    cJSON_AddItemToObject(root, "behavior", behavior);
    behavior = NULL;

    cJSON_AddItemToObject(compatibility, "onion_system", cJSON_Duplicate(system, 1));
    cJSON_AddItemToObject(compatibility, "onion_keymap", cJSON_Duplicate(keymap, 1));
    cJSON_AddItemToObject(root, "compatibility", compatibility);
    compatibility = NULL;
    return root;

failure:
    cJSON_Delete(root);
    cJSON_Delete(source);
    cJSON_Delete(device);
    cJSON_Delete(interface);
    cJSON_Delete(behavior);
    cJSON_Delete(compatibility);
    return NULL;
}

static int validate_settings_root(const cJSON *root, int *schema, int *generation, char *source,
                                  size_t source_size, char *authority, size_t authority_size,
                                  char *error, size_t error_size)
{
    const cJSON *schema_node = root ? cJSON_GetObjectItemCaseSensitive(root, "schema") : NULL;
    const cJSON *generation_node = root ? cJSON_GetObjectItemCaseSensitive(root, "generation") : NULL;
    const cJSON *authority_node = root ? cJSON_GetObjectItemCaseSensitive(root, "authority") : NULL;
    const cJSON *source_node = root ? cJSON_GetObjectItemCaseSensitive(root, "source") : NULL;
    const cJSON *kind = source_node ? cJSON_GetObjectItemCaseSensitive(source_node, "kind") : NULL;
    const cJSON *device = root ? cJSON_GetObjectItemCaseSensitive(root, "device") : NULL;
    const cJSON *interface = root ? cJSON_GetObjectItemCaseSensitive(root, "interface") : NULL;
    const cJSON *behavior = root ? cJSON_GetObjectItemCaseSensitive(root, "behavior") : NULL;
    const cJSON *compatibility = root ? cJSON_GetObjectItemCaseSensitive(root, "compatibility") : NULL;
    if (schema == NULL || generation == NULL || source == NULL || source_size == 0 ||
        authority == NULL || authority_size == 0 || !cJSON_IsObject(root) ||
        !cJSON_IsNumber(schema_node) ||
        schema_node->valuedouble != schema_node->valueint || schema_node->valueint != BLOOM_SETTINGS_SCHEMA ||
        !cJSON_IsNumber(generation_node) || generation_node->valueint < 1 ||
        generation_node->valuedouble != generation_node->valueint || !cJSON_IsString(authority_node) ||
        (strcmp(authority_node->valuestring, "legacy") != 0 &&
         strcmp(authority_node->valuestring, "bloom") != 0) ||
        strlen(authority_node->valuestring) >= authority_size || !cJSON_IsObject(source_node) ||
        !cJSON_IsString(kind) || !safe_identifier(kind->valuestring, source_size) ||
        !cJSON_IsObject(device) ||
        !cJSON_IsObject(interface) || !cJSON_IsObject(behavior) || !cJSON_IsObject(compatibility)) {
        set_error(error, error_size, "settings schema is invalid or unsupported");
        return -1;
    }
    *schema = schema_node->valueint;
    *generation = generation_node->valueint;
    snprintf(source, source_size, "%s", kind->valuestring);
    snprintf(authority, authority_size, "%s", authority_node->valuestring);
    return 0;
}

static cJSON *load_settings_root(const char *settings_path, char *error, size_t error_size)
{
    char *raw = NULL;
    size_t length = 0;
    if (read_regular_file(settings_path, &raw, &length, 0) != 0) {
        set_error(error, error_size, "settings are unavailable");
        return NULL;
    }
    cJSON *root = cJSON_ParseWithLength(raw, length);
    free(raw);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        set_error(error, error_size, "settings schema is invalid or unsupported");
        return NULL;
    }
    return root;
}

int bloom_settings_status(const char *settings_path, int *schema, char *source, size_t source_size,
                          char *authority, size_t authority_size, char *error, size_t error_size)
{
    if (settings_path == NULL) {
        set_error(error, error_size, "settings are unavailable");
        return -1;
    }
    cJSON *root = load_settings_root(settings_path, error, error_size);
    int generation = 0;
    int result = root == NULL ? -1 : validate_settings_root(root, schema, &generation, source, source_size, authority, authority_size, error, error_size);
    cJSON_Delete(root);
    return result;
}

static int import_onion_unlocked(const char *onion_system_path, const char *onion_config_root,
                                 const char *settings_path, const char *snapshot_path,
                                 BloomSettingsImportResult *result, char *error, size_t error_size)
{
    if (onion_system_path == NULL || onion_config_root == NULL || settings_path == NULL ||
        snapshot_path == NULL || result == NULL) {
        set_error(error, error_size, "invalid settings import request");
        return -1;
    }
    memset(result, 0, sizeof(*result));
    result->schema = BLOOM_SETTINGS_SCHEMA;
    struct stat existing;
    if (lstat(settings_path, &existing) == 0) {
        int schema = 0;
        char source[32];
        char authority[16];
        if (bloom_settings_status(settings_path, &schema, source, sizeof(source), authority,
                                  sizeof(authority), error, error_size) != 0)
            return -1;
        return 0;
    }
    if (errno != ENOENT) {
        set_error(error, error_size, "settings destination is unavailable");
        return -1;
    }

    char *system_raw = NULL;
    size_t system_length = 0;
    int system_missing = 0;
    cJSON *system = parse_optional_object(onion_system_path, &system_raw, &system_length, &system_missing);
    char keymap_path[PATH_MAX];
    if (system == NULL || path_join(keymap_path, sizeof(keymap_path), onion_config_root, "keymap.json") != 0) {
        cJSON_Delete(system);
        free(system_raw);
        set_error(error, error_size, "legacy settings are invalid");
        return -1;
    }
    char *keymap_raw = NULL;
    size_t keymap_length = 0;
    int keymap_missing = 0;
    cJSON *keymap = parse_optional_object(keymap_path, &keymap_raw, &keymap_length, &keymap_missing);
    if (keymap == NULL) {
        cJSON_Delete(system);
        free(system_raw);
        free(keymap_raw);
        set_error(error, error_size, "legacy keymap is invalid");
        return -1;
    }
    result->used_defaults = system_missing || keymap_missing;
    cJSON *root = build_settings(system, keymap, onion_config_root, result->used_defaults);
    char *serialized = root ? cJSON_PrintUnformatted(root) : NULL;
    if (root == NULL || serialized == NULL) {
        set_error(error, error_size, "settings import could not be created");
        goto failure;
    }

    if (!system_missing) {
        struct stat snapshot;
        if (lstat(snapshot_path, &snapshot) == 0) {
            if (!S_ISREG(snapshot.st_mode) || S_ISLNK(snapshot.st_mode)) {
                set_error(error, error_size, "legacy snapshot is invalid");
                goto failure;
            }
        }
        else if (errno != ENOENT || write_atomic(snapshot_path, system_raw, system_length) != 0) {
            set_error(error, error_size, "legacy snapshot could not be stored");
            goto failure;
        }
        else {
            result->legacy_snapshot_written = 1;
        }
    }
    if (write_atomic(settings_path, serialized, strlen(serialized)) != 0) {
        set_error(error, error_size, "settings could not be published");
        goto failure;
    }
    result->imported = 1;
    cJSON_free(serialized);
    cJSON_Delete(root);
    cJSON_Delete(system);
    cJSON_Delete(keymap);
    free(system_raw);
    free(keymap_raw);
    return 0;

failure:
    cJSON_free(serialized);
    cJSON_Delete(root);
    cJSON_Delete(system);
    cJSON_Delete(keymap);
    free(system_raw);
    free(keymap_raw);
    return -1;
}

int bloom_settings_import_onion(const char *onion_system_path, const char *onion_config_root,
                                const char *settings_path, const char *snapshot_path,
                                BloomSettingsImportResult *result, char *error, size_t error_size)
{
    if (settings_path == NULL) {
        set_error(error, error_size, "invalid settings import request");
        return -1;
    }
    int lock = -1;
    if (acquire_settings_lock(settings_path, &lock, error, error_size) != 0)
        return -1;
    int outcome = import_onion_unlocked(onion_system_path, onion_config_root, settings_path,
                                        snapshot_path, result, error, error_size);
    release_settings_lock(lock);
    return outcome;
}

static int replace_section(cJSON *destination, cJSON *source, const char *name)
{
    cJSON *replacement = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(source, name), 1);
    if (replacement == NULL)
        return 0;
    if (!cJSON_ReplaceItemInObjectCaseSensitive(destination, name, replacement)) {
        cJSON_Delete(replacement);
        return 0;
    }
    return 1;
}

int bloom_settings_sync_onion(const char *onion_system_path, const char *onion_config_root,
                              const char *settings_path, BloomSettingsSyncResult *result,
                              char *error, size_t error_size)
{
    if (onion_system_path == NULL || onion_config_root == NULL || settings_path == NULL ||
        result == NULL) {
        set_error(error, error_size, "invalid settings sync request");
        return -1;
    }
    memset(result, 0, sizeof(*result));
    int lock = -1;
    if (acquire_settings_lock(settings_path, &lock, error, error_size) != 0)
        return -1;
    cJSON *current = load_settings_root(settings_path, error, error_size);
    char *before = NULL;
    char *normalized_after = NULL;
    char *published = NULL;
    int schema = 0;
    int generation = 0;
    char source_name[32] = {0};
    char authority[16] = {0};
    if (current == NULL ||
        validate_settings_root(current, &schema, &generation, source_name, sizeof(source_name),
                               authority, sizeof(authority), error, error_size) != 0)
        goto failure;
    if (strcmp(authority, "legacy") != 0) {
        set_error(error, error_size, "legacy settings sync is disabled after Bloom cutover");
        goto failure;
    }
    before = cJSON_PrintUnformatted(current);
    if (before == NULL) {
        set_error(error, error_size, "legacy settings sync could not be created");
        goto failure;
    }

    char *system_raw = NULL;
    size_t system_length = 0;
    int system_missing = 0;
    cJSON *system = parse_optional_object(onion_system_path, &system_raw, &system_length,
                                          &system_missing);
    char keymap_path[PATH_MAX];
    char *keymap_raw = NULL;
    size_t keymap_length = 0;
    int keymap_missing = 0;
    cJSON *keymap = NULL;
    if (system != NULL && path_join(keymap_path, sizeof(keymap_path), onion_config_root,
                                    "keymap.json") == 0)
        keymap = parse_optional_object(keymap_path, &keymap_raw, &keymap_length, &keymap_missing);
    if (system == NULL || keymap == NULL) {
        set_error(error, error_size, "legacy settings are invalid");
        cJSON_Delete(system);
        cJSON_Delete(keymap);
        free(system_raw);
        free(keymap_raw);
        goto failure;
    }
    cJSON *fresh = build_settings(system, keymap, onion_config_root,
                                  system_missing || keymap_missing);
    cJSON_Delete(system);
    cJSON_Delete(keymap);
    free(system_raw);
    free(keymap_raw);
    if (fresh == NULL || !replace_section(current, fresh, "source") ||
        !replace_section(current, fresh, "device") || !replace_section(current, fresh, "interface") ||
        !replace_section(current, fresh, "behavior") ||
        !replace_section(current, fresh, "compatibility")) {
        cJSON_Delete(fresh);
        set_error(error, error_size, "legacy settings sync could not be created");
        goto failure;
    }
    cJSON_Delete(fresh);

    cJSON *generation_node = cJSON_GetObjectItemCaseSensitive(current, "generation");
    normalized_after = cJSON_PrintUnformatted(current);
    if (normalized_after != NULL && strcmp(before, normalized_after) == 0) {
        result->generation = generation;
        cJSON_free(before);
        cJSON_free(normalized_after);
        cJSON_Delete(current);
        release_settings_lock(lock);
        return 0;
    }
    if (normalized_after == NULL) {
        set_error(error, error_size, "legacy settings sync could not be created");
        goto failure;
    }
    cJSON_SetNumberValue(generation_node, generation + 1);
    published = cJSON_PrintUnformatted(current);
    if (published == NULL || write_atomic(settings_path, published, strlen(published)) != 0) {
        set_error(error, error_size, "legacy settings sync could not be published");
        goto failure;
    }
    cJSON_free(before);
    cJSON_free(normalized_after);
    cJSON_free(published);
    result->changed = 1;
    result->generation = generation + 1;
    cJSON_Delete(current);
    release_settings_lock(lock);
    return 0;

failure:
    cJSON_free(before);
    cJSON_free(normalized_after);
    cJSON_free(published);
    cJSON_Delete(current);
    release_settings_lock(lock);
    return -1;
}
