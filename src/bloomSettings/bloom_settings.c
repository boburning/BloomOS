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

static void add_config_string(cJSON *object, const char *field, const char *root, const char *name,
                              const char *fallback, size_t maximum)
{
    char path[PATH_MAX];
    char *content = NULL;
    size_t length = 0;
    if (path_join(path, sizeof(path), root, name) != 0 ||
        read_regular_file(path, &content, &length, 1) != 0) {
        cJSON_AddStringToObject(object, field, fallback);
        return;
    }
    while (length > 0 && (content[length - 1] == '\n' || content[length - 1] == '\r'))
        content[--length] = '\0';
    int valid = length > 0 && length < maximum;
    for (size_t index = 0; valid && index < length; ++index)
        valid = (unsigned char)content[index] >= 0x20 && (unsigned char)content[index] < 0x7f;
    cJSON_AddStringToObject(object, field, valid ? content : fallback);
    free(content);
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
    cJSON *controls = cJSON_CreateObject();
    cJSON *blue_light = cJSON_CreateObject();
    cJSON *recording = cJSON_CreateObject();
    cJSON *compatibility = cJSON_CreateObject();
    if (root == NULL || source == NULL || device == NULL || interface == NULL || behavior == NULL ||
        controls == NULL || blue_light == NULL || recording == NULL || compatibility == NULL)
        goto failure;

    cJSON_AddNumberToObject(root, "schema", BLOOM_SETTINGS_SCHEMA);
    cJSON_AddNumberToObject(root, "generation", 1);
    cJSON_AddStringToObject(root, "authority", "legacy");
    cJSON_AddStringToObject(source, "kind", "onion");
    cJSON_AddBoolToObject(source, "used_defaults", used_defaults != 0);
    cJSON_AddItemToObject(root, "source", source);
    source = NULL;

    add_integer(device, "volume", bounded_integer(system, "vol", 20, 0, 20));
    add_boolean(device, "mute", flag_exists(config_root, ".muteVolume"));
    add_integer(device, "background_music_volume",
                bounded_integer(system, "bgmvol", 20, 0, 20));
    add_integer(device, "brightness", bounded_integer(system, "brightness", 7, 0, 10));
    add_boolean(device, "wifi_enabled", bounded_integer(system, "wifi", 0, 0, 1));
    add_integer(device, "sleep_minutes", bounded_integer(system, "hibernate", 5, 0, 120));
    add_integer(device, "luminance", bounded_integer(system, "lumination", 7, 0, 20));
    add_integer(device, "hue", bounded_integer(system, "hue", 10, 0, 20));
    add_integer(device, "saturation", bounded_integer(system, "saturation", 10, 0, 20));
    add_integer(device, "contrast", bounded_integer(system, "contrast", 10, 0, 20));
    add_integer(device, "audio_fix", bounded_integer(system, "audiofix", 1, 0, 1));
    add_integer(device, "vibration",
                read_config_integer(config_root, "vibration",
                                    flag_exists(config_root, ".noVibration") ? 0 : 2, 0, 3));
    add_integer(device, "pwm_frequency",
                read_config_integer(config_root, "pwmfrequency", 7, 0, 20));
    cJSON_AddItemToObject(root, "device", device);
    device = NULL;

    cJSON_AddStringToObject(interface, "language",
                            bounded_string(system, "language", "en.lang"));
    const char *theme = bounded_string(system, "theme", "./");
    cJSON_AddStringToObject(interface, "theme",
                            strcmp(theme, "./") == 0
                                ? "/mnt/SDCARD/Themes/Silky by DiMo/"
                                : theme);
    add_integer(interface, "font_size", bounded_integer(system, "fontsize", 24, 8, 64));
    add_boolean(interface, "background_music_muted", flag_exists(config_root, ".bgmMute"));
    add_boolean(interface, "show_recents", flag_exists(config_root, ".showRecents"));
    add_boolean(interface, "show_expert", flag_exists(config_root, ".showExpert"));
    add_boolean(blue_light, "enabled", flag_exists(config_root, ".blfOn"));
    add_boolean(blue_light, "scheduled", flag_exists(config_root, ".blf"));
    add_integer(blue_light, "level",
                read_config_integer(config_root, "display/blueLightLevel", 0, 0, 20));
    add_integer(blue_light, "rgb",
                read_config_integer(config_root, "display/blueLightRGB", 8421504, 0, 16777215));
    add_config_string(blue_light, "start_time", config_root, "display/blueLightTime", "20:00",
                      16);
    add_config_string(blue_light, "end_time", config_root, "display/blueLightTimeOff", "08:00",
                      16);
    cJSON_AddItemToObject(interface, "blue_light", blue_light);
    blue_light = NULL;
    add_boolean(recording, "indicator", flag_exists(config_root, ".recIndicator"));
    add_boolean(recording, "hotkey", flag_exists(config_root, ".recHotkey"));
    add_integer(recording, "countdown",
                read_config_integer(config_root, "recCountdown", 0, 0, 10));
    cJSON_AddItemToObject(interface, "recording", recording);
    recording = NULL;
    cJSON_AddItemToObject(root, "interface", interface);
    interface = NULL;

    add_boolean(behavior, "startup_auto_resume", !flag_exists(config_root, ".noAutoStart"));
    add_boolean(behavior, "menu_button_haptics", !flag_exists(config_root, ".noMenuHaptics"));
    add_boolean(behavior, "disable_standby", flag_exists(config_root, ".disableStandby"));
    add_boolean(behavior, "logging", flag_exists(config_root, ".logging"));
    add_integer(behavior, "low_battery_warn_at",
                read_config_integer(config_root, "battery/warnAt",
                                    flag_exists(config_root, ".noBatteryWarning") ? 0 : 10, 0,
                                    100));
    add_integer(behavior, "low_battery_autosave_at",
                read_config_integer(config_root, "battery/exitAt",
                                    flag_exists(config_root, ".noLowBatteryAutoSave") ? 0 : 4, 0,
                                    100));
    add_integer(behavior, "startup_tab", read_config_integer(config_root, "startup/tab", 0, 0, 20));
    add_integer(behavior, "startup_application",
                read_config_integer(config_root, "startup/app", 0, 0, 20));
    add_integer(behavior, "time_skip_hours",
                read_config_integer(config_root, "startup/addHours", 4, 0, 24));
    cJSON_AddItemToObject(root, "behavior", behavior);
    behavior = NULL;

    int mainui_single = flag_exists(config_root, ".noGameSwitcher") ? 0 : 1;
    int ingame_single = flag_exists(config_root, ".menuInverted") ? 2 : 1;
    int ingame_long = flag_exists(config_root, ".noGameSwitcher")
                          ? 0
                          : (flag_exists(config_root, ".menuInverted") ? 1 : 2);
    cJSON_AddStringToObject(controls, "layout",
                            bounded_string(system, "keymap", "L2,L,R2,R,X,A,B,Y"));
    add_integer(controls, "mainui_single_press",
                bounded_integer(keymap, "mainui_single_press", mainui_single, 0, 20));
    add_integer(controls, "mainui_long_press",
                bounded_integer(keymap, "mainui_long_press", 0, 0, 20));
    add_integer(controls, "mainui_double_press",
                bounded_integer(keymap, "mainui_double_press", 2, 0, 20));
    add_integer(controls, "ingame_single_press",
                bounded_integer(keymap, "ingame_single_press", ingame_single, 0, 20));
    add_integer(controls, "ingame_long_press",
                bounded_integer(keymap, "ingame_long_press", ingame_long, 0, 20));
    add_integer(controls, "ingame_double_press",
                bounded_integer(keymap, "ingame_double_press", 3, 0, 20));
    cJSON_AddStringToObject(controls, "mainui_button_x",
                            bounded_string(keymap, "mainui_button_x", ""));
    cJSON_AddStringToObject(controls, "mainui_button_y",
                            bounded_string(keymap, "mainui_button_y", ""));
    cJSON_AddItemToObject(root, "controls", controls);
    controls = NULL;

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
    cJSON_Delete(controls);
    cJSON_Delete(blue_light);
    cJSON_Delete(recording);
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
    const cJSON *controls = root ? cJSON_GetObjectItemCaseSensitive(root, "controls") : NULL;
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
        !cJSON_IsObject(interface) || !cJSON_IsObject(behavior) ||
        (controls != NULL && !cJSON_IsObject(controls)) || !cJSON_IsObject(compatibility)) {
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

static int replace_section(cJSON *destination, const cJSON *source, const char *name)
{
    cJSON *replacement = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(source, name), 1);
    if (replacement == NULL)
        return 0;
    if (cJSON_GetObjectItemCaseSensitive(destination, name) == NULL) {
        if (cJSON_AddItemToObject(destination, name, replacement))
            return 1;
        cJSON_Delete(replacement);
        return 0;
    }
    if (cJSON_ReplaceItemInObjectCaseSensitive(destination, name, replacement))
        return 1;
    cJSON_Delete(replacement);
    return 0;
}

static int reconcile_onion_for_authority(const char *onion_system_path,
                                         const char *onion_config_root,
                                         const char *settings_path,
                                         const char *required_authority,
                                         BloomSettingsSyncResult *result, char *error,
                                         size_t error_size)
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
    if (required_authority != NULL && strcmp(authority, required_authority) != 0) {
        set_error(error, error_size,
                  strcmp(required_authority, "legacy") == 0
                      ? "legacy settings sync is disabled after Bloom cutover"
                      : "Bloom compatibility commit requires Bloom authority");
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
        !replace_section(current, fresh, "controls") ||
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

int bloom_settings_sync_onion(const char *onion_system_path, const char *onion_config_root,
                              const char *settings_path, BloomSettingsSyncResult *result,
                              char *error, size_t error_size)
{
    return reconcile_onion_for_authority(onion_system_path, onion_config_root, settings_path,
                                         "legacy", result, error, error_size);
}

int bloom_settings_reconcile_onion(const char *onion_system_path, const char *onion_config_root,
                                   const char *settings_path, BloomSettingsSyncResult *result,
                                   char *error, size_t error_size)
{
    return reconcile_onion_for_authority(onion_system_path, onion_config_root, settings_path, NULL,
                                         result, error, error_size);
}

static int required_integer(const cJSON *object, const char *name, int minimum, int maximum,
                            int *output)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsNumber(value) || value->valuedouble != value->valueint ||
        value->valueint < minimum || value->valueint > maximum)
        return 0;
    *output = value->valueint;
    return 1;
}

static int required_boolean(const cJSON *object, const char *name, int *output)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsBool(value))
        return 0;
    *output = cJSON_IsTrue(value);
    return 1;
}

static int required_string(const cJSON *object, const char *name, char *output, size_t size,
                           int allow_empty)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsString(value) || (!allow_empty && value->valuestring[0] == '\0') ||
        strlen(value->valuestring) >= size)
        return 0;
    snprintf(output, size, "%s", value->valuestring);
    return 1;
}

static int parse_settings_values(const cJSON *root, BloomSettingsValues *values, char *error,
                                 size_t error_size)
{
    int schema = 0;
    char source[32] = {0};
    const cJSON *device = cJSON_GetObjectItemCaseSensitive(root, "device");
    const cJSON *interface = cJSON_GetObjectItemCaseSensitive(root, "interface");
    const cJSON *behavior = cJSON_GetObjectItemCaseSensitive(root, "behavior");
    const cJSON *controls = cJSON_GetObjectItemCaseSensitive(root, "controls");
    const cJSON *blue_light = cJSON_IsObject(interface)
                                  ? cJSON_GetObjectItemCaseSensitive(interface, "blue_light")
                                  : NULL;
    const cJSON *recording = cJSON_IsObject(interface)
                                 ? cJSON_GetObjectItemCaseSensitive(interface, "recording")
                                 : NULL;
    memset(values, 0, sizeof(*values));
    if (validate_settings_root(root, &schema, &values->generation, source, sizeof(source),
                               values->authority, sizeof(values->authority), error, error_size) !=
            0 ||
        !cJSON_IsObject(controls) || !cJSON_IsObject(blue_light) || !cJSON_IsObject(recording) ||
        !required_integer(device, "volume", 0, 20, &values->volume) ||
        !required_boolean(device, "mute", &values->mute) ||
        !required_integer(device, "background_music_volume", 0, 20,
                          &values->background_music_volume) ||
        !required_integer(device, "brightness", 0, 10, &values->brightness) ||
        !required_boolean(device, "wifi_enabled", &values->wifi_enabled) ||
        !required_integer(device, "sleep_minutes", 0, 120, &values->sleep_minutes) ||
        !required_integer(device, "luminance", 0, 20, &values->luminance) ||
        !required_integer(device, "hue", 0, 20, &values->hue) ||
        !required_integer(device, "saturation", 0, 20, &values->saturation) ||
        !required_integer(device, "contrast", 0, 20, &values->contrast) ||
        !required_integer(device, "audio_fix", 0, 1, &values->audio_fix) ||
        !required_integer(device, "vibration", 0, 3, &values->vibration) ||
        !required_integer(device, "pwm_frequency", 0, 20, &values->pwm_frequency) ||
        !required_string(interface, "language", values->language, sizeof(values->language), 0) ||
        !required_string(interface, "theme", values->theme, sizeof(values->theme), 0) ||
        !required_integer(interface, "font_size", 8, 64, &values->font_size) ||
        !required_boolean(interface, "background_music_muted",
                          &values->background_music_muted) ||
        !required_boolean(interface, "show_recents", &values->show_recents) ||
        !required_boolean(interface, "show_expert", &values->show_expert) ||
        !required_boolean(blue_light, "enabled", &values->blue_light_enabled) ||
        !required_boolean(blue_light, "scheduled", &values->blue_light_scheduled) ||
        !required_integer(blue_light, "level", 0, 20, &values->blue_light_level) ||
        !required_integer(blue_light, "rgb", 0, 16777215, &values->blue_light_rgb) ||
        !required_string(blue_light, "start_time", values->blue_light_start,
                         sizeof(values->blue_light_start), 0) ||
        !required_string(blue_light, "end_time", values->blue_light_end,
                         sizeof(values->blue_light_end), 0) ||
        !required_boolean(recording, "indicator", &values->recording_indicator) ||
        !required_boolean(recording, "hotkey", &values->recording_hotkey) ||
        !required_integer(recording, "countdown", 0, 10, &values->recording_countdown) ||
        !required_boolean(behavior, "startup_auto_resume", &values->startup_auto_resume) ||
        !required_boolean(behavior, "menu_button_haptics", &values->menu_button_haptics) ||
        !required_boolean(behavior, "disable_standby", &values->disable_standby) ||
        !required_boolean(behavior, "logging", &values->logging) ||
        !required_integer(behavior, "low_battery_warn_at", 0, 100,
                          &values->low_battery_warn_at) ||
        !required_integer(behavior, "low_battery_autosave_at", 0, 100,
                          &values->low_battery_autosave_at) ||
        !required_integer(behavior, "startup_tab", 0, 20, &values->startup_tab) ||
        !required_integer(behavior, "startup_application", 0, 20,
                          &values->startup_application) ||
        !required_integer(behavior, "time_skip_hours", 0, 24, &values->time_skip_hours) ||
        !required_string(controls, "layout", values->layout, sizeof(values->layout), 0) ||
        !required_integer(controls, "mainui_single_press", 0, 20,
                          &values->mainui_single_press) ||
        !required_integer(controls, "mainui_long_press", 0, 20,
                          &values->mainui_long_press) ||
        !required_integer(controls, "mainui_double_press", 0, 20,
                          &values->mainui_double_press) ||
        !required_integer(controls, "ingame_single_press", 0, 20,
                          &values->ingame_single_press) ||
        !required_integer(controls, "ingame_long_press", 0, 20,
                          &values->ingame_long_press) ||
        !required_integer(controls, "ingame_double_press", 0, 20,
                          &values->ingame_double_press) ||
        !required_string(controls, "mainui_button_x", values->mainui_button_x,
                         sizeof(values->mainui_button_x), 1) ||
        !required_string(controls, "mainui_button_y", values->mainui_button_y,
                         sizeof(values->mainui_button_y), 1)) {
        set_error(error, error_size, "canonical settings values are invalid");
        return -1;
    }
    return 0;
}

int bloom_settings_read_values(const char *settings_path, BloomSettingsValues *values, char *error,
                               size_t error_size)
{
    if (settings_path == NULL || values == NULL) {
        set_error(error, error_size, "invalid settings read request");
        return -1;
    }
    cJSON *root = load_settings_root(settings_path, error, error_size);
    int result = root == NULL ? -1 : parse_settings_values(root, values, error, error_size);
    cJSON_Delete(root);
    return result;
}

static int replace_number(cJSON *object, const char *name, int value)
{
    cJSON *replacement = cJSON_CreateNumber(value);
    if (replacement == NULL)
        return 0;
    if (cJSON_GetObjectItemCaseSensitive(object, name) == NULL) {
        if (cJSON_AddItemToObject(object, name, replacement))
            return 1;
    }
    else if (cJSON_ReplaceItemInObjectCaseSensitive(object, name, replacement))
        return 1;
    cJSON_Delete(replacement);
    return 0;
}

static int replace_string(cJSON *object, const char *name, const char *value)
{
    cJSON *replacement = cJSON_CreateString(value);
    if (replacement == NULL)
        return 0;
    if (cJSON_GetObjectItemCaseSensitive(object, name) == NULL) {
        if (cJSON_AddItemToObject(object, name, replacement))
            return 1;
    }
    else if (cJSON_ReplaceItemInObjectCaseSensitive(object, name, replacement))
        return 1;
    cJSON_Delete(replacement);
    return 0;
}

static int regular_or_missing(const char *path)
{
    struct stat metadata;
    if (lstat(path, &metadata) != 0)
        return errno == ENOENT;
    return S_ISREG(metadata.st_mode) && !S_ISLNK(metadata.st_mode);
}

static int write_derived(const char *path, const char *content)
{
    return regular_or_missing(path) && write_atomic(path, content, strlen(content)) == 0 ? 0 : -1;
}

static int remove_derived(const char *path)
{
    struct stat metadata;
    if (lstat(path, &metadata) != 0)
        return errno == ENOENT ? 0 : -1;
    return S_ISREG(metadata.st_mode) && !S_ISLNK(metadata.st_mode) && unlink(path) == 0 ? 0 : -1;
}

static int materialize_flag(const char *root, const char *name, int enabled)
{
    char positive[PATH_MAX];
    char inverse_name[128];
    char inverse[PATH_MAX];
    if (snprintf(inverse_name, sizeof(inverse_name), "%s_", name) >= (int)sizeof(inverse_name) ||
        path_join(positive, sizeof(positive), root, name) != 0 ||
        path_join(inverse, sizeof(inverse), root, inverse_name) != 0)
        return -1;
    return enabled ? (write_derived(positive, "") == 0 && remove_derived(inverse) == 0 ? 0 : -1)
                   : (write_derived(inverse, "") == 0 && remove_derived(positive) == 0 ? 0 : -1);
}

static int remove_flag_pair(const char *root, const char *name)
{
    char positive[PATH_MAX];
    char inverse_name[128];
    char inverse[PATH_MAX];
    if (snprintf(inverse_name, sizeof(inverse_name), "%s_", name) >= (int)sizeof(inverse_name) ||
        path_join(positive, sizeof(positive), root, name) != 0 ||
        path_join(inverse, sizeof(inverse), root, inverse_name) != 0)
        return -1;
    return remove_derived(positive) == 0 && remove_derived(inverse) == 0 ? 0 : -1;
}

static int ensure_derived_directory(const char *root, const char *name)
{
    char path[PATH_MAX];
    struct stat metadata;
    if (path_join(path, sizeof(path), root, name) != 0)
        return -1;
    if (lstat(path, &metadata) == 0)
        return S_ISDIR(metadata.st_mode) && !S_ISLNK(metadata.st_mode) ? 0 : -1;
    return errno == ENOENT && mkdir(path, 0700) == 0 ? 0 : -1;
}

static int write_derived_integer(const char *root, const char *name, int value)
{
    char path[PATH_MAX];
    char text[32];
    if (path_join(path, sizeof(path), root, name) != 0 ||
        snprintf(text, sizeof(text), "%d", value) >= (int)sizeof(text))
        return -1;
    return write_derived(path, text);
}

static int write_derived_string(const char *root, const char *name, const char *value)
{
    char path[PATH_MAX];
    return path_join(path, sizeof(path), root, name) == 0 ? write_derived(path, value) : -1;
}

static int preflight_derived_paths(const char *root, const char *const *names, size_t count)
{
    struct stat metadata;
    if (lstat(root, &metadata) != 0 || !S_ISDIR(metadata.st_mode) || S_ISLNK(metadata.st_mode))
        return -1;
    for (size_t index = 0; index < count; ++index) {
        char path[PATH_MAX];
        if (path_join(path, sizeof(path), root, names[index]) != 0 || !regular_or_missing(path))
            return -1;
    }
    return 0;
}

int bloom_settings_materialize_onion(const char *settings_path, const char *onion_system_path,
                                     const char *onion_config_root, char *error,
                                     size_t error_size)
{
    if (settings_path == NULL || onion_system_path == NULL || onion_config_root == NULL) {
        set_error(error, error_size, "invalid settings materialization request");
        return -1;
    }
    int lock = -1;
    if (acquire_settings_lock(settings_path, &lock, error, error_size) != 0)
        return -1;
    cJSON *root = load_settings_root(settings_path, error, error_size);
    BloomSettingsValues values;
    cJSON *system = NULL;
    cJSON *keymap = NULL;
    char *system_text = NULL;
    char *keymap_text = NULL;
    if (root == NULL || parse_settings_values(root, &values, error, error_size) != 0)
        goto failure;
    if (strcmp(values.authority, "bloom") != 0) {
        set_error(error, error_size, "materialization requires Bloom settings authority");
        goto failure;
    }
    const cJSON *compatibility = cJSON_GetObjectItemCaseSensitive(root, "compatibility");
    system = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(compatibility, "onion_system"), 1);
    keymap = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(compatibility, "onion_keymap"), 1);
    if (!cJSON_IsObject(system) || !cJSON_IsObject(keymap) ||
        !replace_number(system, "vol", values.volume) ||
        !replace_number(system, "mute", values.mute) ||
        !replace_number(system, "bgmvol", values.background_music_volume) ||
        !replace_number(system, "brightness", values.brightness) ||
        !replace_number(system, "wifi", values.wifi_enabled) ||
        !replace_number(system, "hibernate", values.sleep_minutes) ||
        !replace_number(system, "lumination", values.luminance) ||
        !replace_number(system, "hue", values.hue) ||
        !replace_number(system, "saturation", values.saturation) ||
        !replace_number(system, "contrast", values.contrast) ||
        !replace_number(system, "audiofix", values.audio_fix) ||
        !replace_string(system, "language", values.language) ||
        !replace_string(system, "theme", values.theme) ||
        !replace_number(system, "fontsize", values.font_size) ||
        !replace_string(system, "keymap", values.layout) ||
        !replace_number(keymap, "mainui_single_press", values.mainui_single_press) ||
        !replace_number(keymap, "mainui_long_press", values.mainui_long_press) ||
        !replace_number(keymap, "mainui_double_press", values.mainui_double_press) ||
        !replace_number(keymap, "ingame_single_press", values.ingame_single_press) ||
        !replace_number(keymap, "ingame_long_press", values.ingame_long_press) ||
        !replace_number(keymap, "ingame_double_press", values.ingame_double_press) ||
        !replace_string(keymap, "mainui_button_x", values.mainui_button_x) ||
        !replace_string(keymap, "mainui_button_y", values.mainui_button_y)) {
        set_error(error, error_size, "derived Onion settings could not be created");
        goto failure;
    }
    system_text = cJSON_PrintUnformatted(system);
    keymap_text = cJSON_PrintUnformatted(keymap);
    char keymap_path[PATH_MAX];
    static const char *const derived_paths[] = {
        "keymap.json",
        ".muteVolume", ".muteVolume_", ".bgmMute", ".bgmMute_",
        ".showRecents", ".showRecents_", ".showExpert", ".showExpert_",
        ".noAutoStart", ".noAutoStart_", ".noMenuHaptics", ".noMenuHaptics_",
        ".disableStandby", ".disableStandby_", ".logging", ".logging_",
        ".blfOn", ".blfOn_", ".blf", ".blf_",
        ".recIndicator", ".recIndicator_", ".recHotkey", ".recHotkey_",
        ".noBatteryWarning", ".noBatteryWarning_",
        ".noLowBatteryAutoSave", ".noLowBatteryAutoSave_",
        ".noVibration", ".noVibration_", ".menuInverted", ".menuInverted_",
        ".noGameSwitcher", ".noGameSwitcher_",
        "battery/warnAt", "battery/exitAt", "startup/tab", "startup/app",
        "startup/addHours", "vibration", "pwmfrequency", "display/blueLightLevel",
        "display/blueLightRGB", "display/blueLightTime", "display/blueLightTimeOff",
        "recCountdown"};
    if (system_text == NULL || keymap_text == NULL ||
        path_join(keymap_path, sizeof(keymap_path), onion_config_root, "keymap.json") != 0 ||
        ensure_derived_directory(onion_config_root, "battery") != 0 ||
        ensure_derived_directory(onion_config_root, "startup") != 0 ||
        ensure_derived_directory(onion_config_root, "display") != 0 ||
        !regular_or_missing(onion_system_path) ||
        preflight_derived_paths(onion_config_root, derived_paths,
                                sizeof(derived_paths) / sizeof(derived_paths[0])) != 0 ||
        write_derived(onion_system_path, system_text) != 0 ||
        write_derived(keymap_path, keymap_text) != 0 ||
        materialize_flag(onion_config_root, ".muteVolume", values.mute) != 0 ||
        materialize_flag(onion_config_root, ".bgmMute", values.background_music_muted) != 0 ||
        materialize_flag(onion_config_root, ".showRecents", values.show_recents) != 0 ||
        materialize_flag(onion_config_root, ".showExpert", values.show_expert) != 0 ||
        materialize_flag(onion_config_root, ".noAutoStart", !values.startup_auto_resume) != 0 ||
        materialize_flag(onion_config_root, ".noMenuHaptics", !values.menu_button_haptics) != 0 ||
        materialize_flag(onion_config_root, ".disableStandby", values.disable_standby) != 0 ||
        materialize_flag(onion_config_root, ".logging", values.logging) != 0 ||
        materialize_flag(onion_config_root, ".blfOn", values.blue_light_enabled) != 0 ||
        materialize_flag(onion_config_root, ".blf", values.blue_light_scheduled) != 0 ||
        materialize_flag(onion_config_root, ".recIndicator", values.recording_indicator) != 0 ||
        materialize_flag(onion_config_root, ".recHotkey", values.recording_hotkey) != 0 ||
        write_derived_integer(onion_config_root, "battery/warnAt",
                              values.low_battery_warn_at) != 0 ||
        write_derived_integer(onion_config_root, "battery/exitAt",
                              values.low_battery_autosave_at) != 0 ||
        write_derived_integer(onion_config_root, "startup/tab", values.startup_tab) != 0 ||
        write_derived_integer(onion_config_root, "startup/app", values.startup_application) != 0 ||
        write_derived_integer(onion_config_root, "startup/addHours", values.time_skip_hours) != 0 ||
        write_derived_integer(onion_config_root, "vibration", values.vibration) != 0 ||
        write_derived_integer(onion_config_root, "pwmfrequency", values.pwm_frequency) != 0 ||
        write_derived_integer(onion_config_root, "display/blueLightLevel",
                              values.blue_light_level) != 0 ||
        write_derived_integer(onion_config_root, "display/blueLightRGB", values.blue_light_rgb) !=
            0 ||
        write_derived_string(onion_config_root, "display/blueLightTime",
                             values.blue_light_start) != 0 ||
        write_derived_string(onion_config_root, "display/blueLightTimeOff",
                             values.blue_light_end) != 0 ||
        write_derived_integer(onion_config_root, "recCountdown", values.recording_countdown) != 0 ||
        remove_flag_pair(onion_config_root, ".noBatteryWarning") != 0 ||
        remove_flag_pair(onion_config_root, ".noLowBatteryAutoSave") != 0 ||
        remove_flag_pair(onion_config_root, ".noVibration") != 0 ||
        remove_flag_pair(onion_config_root, ".menuInverted") != 0 ||
        remove_flag_pair(onion_config_root, ".noGameSwitcher") != 0) {
        set_error(error, error_size, "derived Onion settings could not be published");
        goto failure;
    }
    cJSON_free(system_text);
    cJSON_free(keymap_text);
    cJSON_Delete(system);
    cJSON_Delete(keymap);
    cJSON_Delete(root);
    release_settings_lock(lock);
    return 0;

failure:
    cJSON_free(system_text);
    cJSON_free(keymap_text);
    cJSON_Delete(system);
    cJSON_Delete(keymap);
    cJSON_Delete(root);
    release_settings_lock(lock);
    return -1;
}

static int transition_authority(const char *settings_path, const char *expected_authority,
                                const char *new_authority, int expected_generation,
                                int *published_generation, char *error, size_t error_size)
{
    int lock = -1;
    if (acquire_settings_lock(settings_path, &lock, error, error_size) != 0)
        return -1;
    cJSON *root = load_settings_root(settings_path, error, error_size);
    int schema = 0;
    int generation = 0;
    char source[32] = {0};
    char authority[16] = {0};
    char *published = NULL;
    if (root == NULL ||
        validate_settings_root(root, &schema, &generation, source, sizeof(source), authority,
                               sizeof(authority), error, error_size) != 0 ||
        strcmp(authority, expected_authority) != 0 ||
        (expected_generation > 0 && generation != expected_generation)) {
        set_error(error, error_size, "settings authority transition is no longer valid");
        goto failure;
    }
    cJSON *authority_node = cJSON_GetObjectItemCaseSensitive(root, "authority");
    cJSON *generation_node = cJSON_GetObjectItemCaseSensitive(root, "generation");
    if (!cJSON_SetValuestring(authority_node, new_authority)) {
        set_error(error, error_size, "settings authority transition could not be created");
        goto failure;
    }
    cJSON_SetNumberValue(generation_node, generation + 1);
    published = cJSON_PrintUnformatted(root);
    if (published == NULL || write_atomic(settings_path, published, strlen(published)) != 0) {
        set_error(error, error_size, "settings authority transition could not be published");
        goto failure;
    }
    *published_generation = generation + 1;
    cJSON_free(published);
    cJSON_Delete(root);
    release_settings_lock(lock);
    return 0;

failure:
    cJSON_free(published);
    cJSON_Delete(root);
    release_settings_lock(lock);
    return -1;
}

int bloom_settings_activate(const char *settings_path, const char *onion_system_path,
                            const char *onion_config_root, BloomSettingsAuthorityResult *result,
                            char *error, size_t error_size)
{
    if (settings_path == NULL || onion_system_path == NULL || onion_config_root == NULL ||
        result == NULL) {
        set_error(error, error_size, "invalid settings activation request");
        return -1;
    }
    memset(result, 0, sizeof(*result));
    int schema = 0;
    char source[32] = {0};
    char authority[16] = {0};
    if (bloom_settings_status(settings_path, &schema, source, sizeof(source), authority,
                              sizeof(authority), error, error_size) != 0)
        return -1;
    if (strcmp(authority, "bloom") == 0) {
        BloomSettingsValues values;
        if (bloom_settings_read_values(settings_path, &values, error, error_size) != 0 ||
            bloom_settings_materialize_onion(settings_path, onion_system_path, onion_config_root,
                                             error, error_size) != 0)
            return -1;
        result->generation = values.generation;
        return 0;
    }
    BloomSettingsSyncResult synchronized;
    if (bloom_settings_sync_onion(onion_system_path, onion_config_root, settings_path,
                                  &synchronized, error, error_size) != 0)
        return -1;
    int bloom_generation = 0;
    if (transition_authority(settings_path, "legacy", "bloom", synchronized.generation,
                             &bloom_generation, error, error_size) != 0)
        return -1;
    result->generation = bloom_generation;
    if (bloom_settings_materialize_onion(settings_path, onion_system_path, onion_config_root, error,
                                         error_size) != 0) {
        int rollback_generation = 0;
        if (transition_authority(settings_path, "bloom", "legacy", bloom_generation,
                                 &rollback_generation, NULL, 0) == 0) {
            result->rolled_back = 1;
            result->generation = rollback_generation;
        }
        return -1;
    }
    result->changed = 1;
    return 0;
}

int bloom_settings_rollback_authority(const char *settings_path,
                                      BloomSettingsAuthorityResult *result, char *error,
                                      size_t error_size)
{
    if (settings_path == NULL || result == NULL) {
        set_error(error, error_size, "invalid settings authority rollback request");
        return -1;
    }
    memset(result, 0, sizeof(*result));
    int schema = 0;
    char source[32] = {0};
    char authority[16] = {0};
    if (bloom_settings_status(settings_path, &schema, source, sizeof(source), authority,
                              sizeof(authority), error, error_size) != 0)
        return -1;
    if (strcmp(authority, "legacy") == 0) {
        BloomSettingsValues values;
        if (bloom_settings_read_values(settings_path, &values, error, error_size) != 0)
            return -1;
        result->generation = values.generation;
        return 0;
    }
    if (transition_authority(settings_path, "bloom", "legacy", 0, &result->generation, error,
                             error_size) != 0)
        return -1;
    result->changed = 1;
    return 0;
}

typedef enum {
    BLOOM_FIELD_INTEGER,
    BLOOM_FIELD_BOOLEAN,
    BLOOM_FIELD_STRING
} BloomSettingsFieldType;

typedef struct {
    const char *name;
    const char *section;
    const char *group;
    const char *key;
    BloomSettingsFieldType type;
    int minimum;
    int maximum;
    size_t text_size;
    int allow_empty;
} BloomSettingsFieldPolicy;

#define INTEGER_FIELD(name, section, group, key, minimum, maximum) \
    {name, section, group, key, BLOOM_FIELD_INTEGER, minimum, maximum, 0, 0}
#define BOOLEAN_FIELD(name, section, group, key) \
    {name, section, group, key, BLOOM_FIELD_BOOLEAN, 0, 1, 0, 0}
#define STRING_FIELD(name, section, group, key, size, allow_empty) \
    {name, section, group, key, BLOOM_FIELD_STRING, 0, 0, size, allow_empty}

static const BloomSettingsFieldPolicy SETTINGS_FIELDS[] = {
    INTEGER_FIELD("device.volume", "device", NULL, "volume", 0, 20),
    BOOLEAN_FIELD("device.mute", "device", NULL, "mute"),
    INTEGER_FIELD("device.background_music_volume", "device", NULL,
                  "background_music_volume", 0, 20),
    INTEGER_FIELD("device.brightness", "device", NULL, "brightness", 0, 10),
    BOOLEAN_FIELD("device.wifi_enabled", "device", NULL, "wifi_enabled"),
    INTEGER_FIELD("device.sleep_minutes", "device", NULL, "sleep_minutes", 0, 120),
    INTEGER_FIELD("device.luminance", "device", NULL, "luminance", 0, 20),
    INTEGER_FIELD("device.hue", "device", NULL, "hue", 0, 20),
    INTEGER_FIELD("device.saturation", "device", NULL, "saturation", 0, 20),
    INTEGER_FIELD("device.contrast", "device", NULL, "contrast", 0, 20),
    INTEGER_FIELD("device.audio_fix", "device", NULL, "audio_fix", 0, 1),
    INTEGER_FIELD("device.vibration", "device", NULL, "vibration", 0, 3),
    INTEGER_FIELD("device.pwm_frequency", "device", NULL, "pwm_frequency", 0, 20),
    STRING_FIELD("interface.language", "interface", NULL, "language", BLOOM_SETTINGS_TEXT_MAX, 0),
    STRING_FIELD("interface.theme", "interface", NULL, "theme", BLOOM_SETTINGS_TEXT_MAX, 0),
    INTEGER_FIELD("interface.font_size", "interface", NULL, "font_size", 8, 64),
    BOOLEAN_FIELD("interface.background_music_muted", "interface", NULL,
                  "background_music_muted"),
    BOOLEAN_FIELD("interface.show_recents", "interface", NULL, "show_recents"),
    BOOLEAN_FIELD("interface.show_expert", "interface", NULL, "show_expert"),
    BOOLEAN_FIELD("interface.blue_light.enabled", "interface", "blue_light", "enabled"),
    BOOLEAN_FIELD("interface.blue_light.scheduled", "interface", "blue_light", "scheduled"),
    INTEGER_FIELD("interface.blue_light.level", "interface", "blue_light", "level", 0, 20),
    INTEGER_FIELD("interface.blue_light.rgb", "interface", "blue_light", "rgb", 0, 16777215),
    STRING_FIELD("interface.blue_light.start_time", "interface", "blue_light", "start_time", 16,
                 0),
    STRING_FIELD("interface.blue_light.end_time", "interface", "blue_light", "end_time", 16, 0),
    BOOLEAN_FIELD("interface.recording.indicator", "interface", "recording", "indicator"),
    BOOLEAN_FIELD("interface.recording.hotkey", "interface", "recording", "hotkey"),
    INTEGER_FIELD("interface.recording.countdown", "interface", "recording", "countdown", 0, 10),
    BOOLEAN_FIELD("behavior.startup_auto_resume", "behavior", NULL, "startup_auto_resume"),
    BOOLEAN_FIELD("behavior.menu_button_haptics", "behavior", NULL, "menu_button_haptics"),
    BOOLEAN_FIELD("behavior.disable_standby", "behavior", NULL, "disable_standby"),
    BOOLEAN_FIELD("behavior.logging", "behavior", NULL, "logging"),
    INTEGER_FIELD("behavior.low_battery_warn_at", "behavior", NULL, "low_battery_warn_at", 0,
                  100),
    INTEGER_FIELD("behavior.low_battery_autosave_at", "behavior", NULL,
                  "low_battery_autosave_at", 0, 100),
    INTEGER_FIELD("behavior.startup_tab", "behavior", NULL, "startup_tab", 0, 20),
    INTEGER_FIELD("behavior.startup_application", "behavior", NULL, "startup_application", 0,
                  20),
    INTEGER_FIELD("behavior.time_skip_hours", "behavior", NULL, "time_skip_hours", 0, 24),
    STRING_FIELD("controls.layout", "controls", NULL, "layout", BLOOM_SETTINGS_TEXT_MAX, 0),
    INTEGER_FIELD("controls.mainui_single_press", "controls", NULL, "mainui_single_press", 0, 20),
    INTEGER_FIELD("controls.mainui_long_press", "controls", NULL, "mainui_long_press", 0, 20),
    INTEGER_FIELD("controls.mainui_double_press", "controls", NULL, "mainui_double_press", 0, 20),
    INTEGER_FIELD("controls.ingame_single_press", "controls", NULL, "ingame_single_press", 0, 20),
    INTEGER_FIELD("controls.ingame_long_press", "controls", NULL, "ingame_long_press", 0, 20),
    INTEGER_FIELD("controls.ingame_double_press", "controls", NULL, "ingame_double_press", 0, 20),
    STRING_FIELD("controls.mainui_button_x", "controls", NULL, "mainui_button_x",
                 BLOOM_SETTINGS_TEXT_MAX, 1),
    STRING_FIELD("controls.mainui_button_y", "controls", NULL, "mainui_button_y",
                 BLOOM_SETTINGS_TEXT_MAX, 1)};

static const BloomSettingsFieldPolicy *find_field_policy(const char *field)
{
    for (size_t index = 0; index < sizeof(SETTINGS_FIELDS) / sizeof(SETTINGS_FIELDS[0]); ++index)
        if (strcmp(SETTINGS_FIELDS[index].name, field) == 0)
            return &SETTINGS_FIELDS[index];
    return NULL;
}

static cJSON *create_field_value(const BloomSettingsFieldPolicy *policy, const char *value,
                                 char *error, size_t error_size)
{
    if (policy->type == BLOOM_FIELD_BOOLEAN) {
        if (strcmp(value, "true") == 0)
            return cJSON_CreateTrue();
        if (strcmp(value, "false") == 0)
            return cJSON_CreateFalse();
        set_error(error, error_size, "settings boolean value is invalid");
        return NULL;
    }
    if (policy->type == BLOOM_FIELD_INTEGER) {
        char *end = NULL;
        errno = 0;
        long number = strtol(value, &end, 10);
        if (errno != 0 || end == value || *end != '\0' || number < policy->minimum ||
            number > policy->maximum) {
            set_error(error, error_size, "settings integer value is invalid");
            return NULL;
        }
        return cJSON_CreateNumber((double)number);
    }
    size_t length = strlen(value);
    if ((!policy->allow_empty && length == 0) || length >= policy->text_size) {
        set_error(error, error_size, "settings text value is invalid");
        return NULL;
    }
    for (size_t index = 0; index < length; ++index)
        if ((unsigned char)value[index] < 0x20 || (unsigned char)value[index] == 0x7f) {
            set_error(error, error_size, "settings text value is invalid");
            return NULL;
        }
    return cJSON_CreateString(value);
}

static int field_values_equal(const cJSON *current, const cJSON *replacement,
                              BloomSettingsFieldType type)
{
    if (type == BLOOM_FIELD_BOOLEAN)
        return cJSON_IsBool(current) && cJSON_IsTrue(current) == cJSON_IsTrue(replacement);
    if (type == BLOOM_FIELD_INTEGER)
        return cJSON_IsNumber(current) && current->valuedouble == replacement->valuedouble;
    return cJSON_IsString(current) && strcmp(current->valuestring, replacement->valuestring) == 0;
}

int bloom_settings_set(const char *settings_path, const char *onion_system_path,
                       const char *onion_config_root, const char *field, const char *value,
                       BloomSettingsMutationResult *result, char *error, size_t error_size)
{
    if (settings_path == NULL || onion_system_path == NULL || onion_config_root == NULL ||
        field == NULL || value == NULL || result == NULL) {
        set_error(error, error_size, "invalid settings mutation request");
        return -1;
    }
    memset(result, 0, sizeof(*result));
    const BloomSettingsFieldPolicy *policy = find_field_policy(field);
    if (policy == NULL) {
        set_error(error, error_size, "settings field is unsupported");
        return -1;
    }
    cJSON *replacement = create_field_value(policy, value, error, error_size);
    if (replacement == NULL)
        return -1;
    int lock = -1;
    if (acquire_settings_lock(settings_path, &lock, error, error_size) != 0) {
        cJSON_Delete(replacement);
        return -1;
    }
    cJSON *root = load_settings_root(settings_path, error, error_size);
    BloomSettingsValues current_values;
    char *published = NULL;
    if (root == NULL || parse_settings_values(root, &current_values, error, error_size) != 0)
        goto failure;
    if (strcmp(current_values.authority, "bloom") != 0) {
        set_error(error, error_size, "settings mutation requires Bloom authority");
        goto failure;
    }
    cJSON *target = cJSON_GetObjectItemCaseSensitive(root, policy->section);
    if (policy->group != NULL)
        target = cJSON_IsObject(target) ? cJSON_GetObjectItemCaseSensitive(target, policy->group)
                                        : NULL;
    const cJSON *current =
        cJSON_IsObject(target) ? cJSON_GetObjectItemCaseSensitive(target, policy->key) : NULL;
    if (current == NULL) {
        set_error(error, error_size, "canonical settings field is unavailable");
        goto failure;
    }
    if (field_values_equal(current, replacement, policy->type)) {
        result->generation = current_values.generation;
        result->materialized = 1;
        cJSON_Delete(replacement);
        cJSON_Delete(root);
        release_settings_lock(lock);
        return 0;
    }
    if (!cJSON_ReplaceItemInObjectCaseSensitive(target, policy->key, replacement)) {
        set_error(error, error_size, "canonical settings mutation could not be created");
        goto failure;
    }
    replacement = NULL;
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(root, "generation"),
                         current_values.generation + 1);
    published = cJSON_PrintUnformatted(root);
    if (published == NULL || write_atomic(settings_path, published, strlen(published)) != 0) {
        set_error(error, error_size, "canonical settings mutation could not be published");
        goto failure;
    }
    result->changed = 1;
    result->generation = current_values.generation + 1;
    cJSON_free(published);
    cJSON_Delete(root);
    release_settings_lock(lock);
    if (bloom_settings_materialize_onion(settings_path, onion_system_path, onion_config_root, error,
                                         error_size) != 0)
        return -1;
    result->materialized = 1;
    return 0;

failure:
    cJSON_Delete(replacement);
    cJSON_free(published);
    cJSON_Delete(root);
    release_settings_lock(lock);
    return -1;
}
