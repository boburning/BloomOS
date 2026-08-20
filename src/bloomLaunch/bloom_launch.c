#include "bloom_launch.h"
#include "../bloomGameId/bloom_game_id.h"

#include <cjson/cJSON.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BLOOM_LAUNCH_MAX_REQUEST (64 * 1024)

static void set_error(char *error, size_t size, const char *format, ...)
{
    if (error == NULL || size == 0)
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, size, format, args);
    va_end(args);
}

static bool safe_text(const char *value)
{
    if (value == NULL || value[0] == '\0')
        return false;
    for (const unsigned char *p = (const unsigned char *)value; *p != '\0'; p++) {
        if (*p < 0x20 || *p == 0x7f)
            return false;
    }
    return strstr(value, "/../") == NULL && strcmp(value + (strlen(value) >= 3 ? strlen(value) - 3 : 0), "/..") != 0;
}

static bool path_under(const char *value, const char *prefix)
{
    return safe_text(value) && strncmp(value, prefix, strlen(prefix)) == 0;
}

static bool valid_system_id(const char *value)
{
    if (!safe_text(value))
        return false;
    for (const unsigned char *p = (const unsigned char *)value; *p != '\0'; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-'))
            return false;
    }
    return true;
}

static bool valid_core(const char *value)
{
    if (!safe_text(value))
        return false;
    size_t length = strlen(value);
    static const char suffix[] = "_libretro.so";
    if (length <= strlen(suffix) || strcmp(value + length - strlen(suffix), suffix) != 0)
        return false;
    for (const unsigned char *p = (const unsigned char *)value; *p != '\0'; p++) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' ||
              *p == '-' || *p == '.'))
            return false;
    }
    return true;
}

static cJSON *load_request(const char *path, char *error, size_t error_size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        set_error(error, error_size, "cannot open request: %s", strerror(errno));
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        set_error(error, error_size, "cannot size request");
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size <= 0 || size > BLOOM_LAUNCH_MAX_REQUEST) {
        set_error(error, error_size, "request size is invalid");
        fclose(file);
        return NULL;
    }
    rewind(file);
    char *buffer = calloc((size_t)size + 1, 1);
    if (buffer == NULL || fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        set_error(error, error_size, "cannot read request");
        free(buffer);
        fclose(file);
        return NULL;
    }
    fclose(file);
    cJSON *root = cJSON_ParseWithLengthOpts(buffer, (size_t)size + 1, NULL, true);
    free(buffer);
    if (root == NULL)
        set_error(error, error_size, "request is not valid JSON");
    return root;
}

static bool allowed_key(const char *key)
{
    static const char *allowed[] = {"schema", "game_id", "system_id", "rom_path",
                                    "launcher", "emulator_type", "core", "auto_load_state",
                                    "append_configs", "requested_resolution", "environment", "achievements"};
    for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++)
        if (strcmp(key, allowed[i]) == 0)
            return true;
    return false;
}

static bool unique_keys(cJSON *root, char *error, size_t error_size)
{
    for (cJSON *item = root->child; item != NULL; item = item->next) {
        if (item->string == NULL || !allowed_key(item->string)) {
            set_error(error, error_size, "unknown request field");
            return false;
        }
        for (cJSON *other = item->next; other != NULL; other = other->next) {
            if (other->string != NULL && strcmp(item->string, other->string) == 0) {
                set_error(error, error_size, "duplicate request field: %s", item->string);
                return false;
            }
        }
    }
    return true;
}

static bool valid_achievements(cJSON *achievements, char *error, size_t error_size)
{
    if (achievements == NULL)
        return true;
    if (!cJSON_IsObject(achievements)) {
        set_error(error, error_size, "achievement policy is invalid");
        return false;
    }
    static const char *allowed[] = {"enabled", "mode", "transport", "ra_game_id", "core_certification"};
    for (cJSON *item = achievements->child; item != NULL; item = item->next) {
        bool known = false;
        for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++)
            if (item->string != NULL && strcmp(item->string, allowed[i]) == 0)
                known = true;
        if (!known) {
            set_error(error, error_size, "unknown achievement policy field");
            return false;
        }
        for (cJSON *other = item->next; other != NULL; other = other->next)
            if (other->string != NULL && item->string != NULL && strcmp(item->string, other->string) == 0) {
                set_error(error, error_size, "duplicate achievement policy field");
                return false;
            }
    }
    cJSON *enabled = cJSON_GetObjectItemCaseSensitive(achievements, "enabled");
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(achievements, "mode");
    cJSON *transport = cJSON_GetObjectItemCaseSensitive(achievements, "transport");
    cJSON *game = cJSON_GetObjectItemCaseSensitive(achievements, "ra_game_id");
    cJSON *certification = cJSON_GetObjectItemCaseSensitive(achievements, "core_certification");
    if (!cJSON_IsBool(enabled) || !cJSON_IsString(mode) || !cJSON_IsString(transport) ||
        !(cJSON_IsNull(game) || (cJSON_IsNumber(game) && game->valuedouble == game->valueint && game->valueint > 0)) ||
        !cJSON_IsString(certification)) {
        set_error(error, error_size, "achievement policy field is invalid");
        return false;
    }
    bool valid_mode = strcmp(mode->valuestring, "disabled") == 0 || strcmp(mode->valuestring, "softcore") == 0 ||
                      strcmp(mode->valuestring, "hardcore") == 0;
    bool valid_transport = strcmp(transport->valuestring, "direct") == 0 ||
                           strcmp(transport->valuestring, "proxy") == 0 ||
                           strcmp(transport->valuestring, "unavailable") == 0;
    bool valid_certification = strcmp(certification->valuestring, "verified") == 0 ||
                               strcmp(certification->valuestring, "best_effort") == 0 ||
                               strcmp(certification->valuestring, "incompatible") == 0 ||
                               strcmp(certification->valuestring, "untested") == 0 ||
                               strcmp(certification->valuestring, "not_applicable") == 0;
    if (!valid_mode || !valid_transport || !valid_certification ||
        (cJSON_IsTrue(enabled) && (strcmp(mode->valuestring, "disabled") == 0 || !cJSON_IsNumber(game))) ||
        (!cJSON_IsTrue(enabled) && strcmp(mode->valuestring, "disabled") != 0) ||
        (strcmp(mode->valuestring, "hardcore") == 0 && strcmp(transport->valuestring, "direct") != 0)) {
        set_error(error, error_size, "achievement policy combination is invalid");
        return false;
    }
    return true;
}

static bool valid_request(cJSON *root, char *error, size_t error_size)
{
    if (!cJSON_IsObject(root) || !unique_keys(root, error, error_size))
        return false;

    cJSON *schema = cJSON_GetObjectItemCaseSensitive(root, "schema");
    cJSON *game_id = cJSON_GetObjectItemCaseSensitive(root, "game_id");
    cJSON *system_id = cJSON_GetObjectItemCaseSensitive(root, "system_id");
    cJSON *rom_path = cJSON_GetObjectItemCaseSensitive(root, "rom_path");
    cJSON *launcher = cJSON_GetObjectItemCaseSensitive(root, "launcher");
    cJSON *emulator = cJSON_GetObjectItemCaseSensitive(root, "emulator_type");
    cJSON *core = cJSON_GetObjectItemCaseSensitive(root, "core");
    cJSON *autoload = cJSON_GetObjectItemCaseSensitive(root, "auto_load_state");
    cJSON *configs = cJSON_GetObjectItemCaseSensitive(root, "append_configs");
    cJSON *resolution = cJSON_GetObjectItemCaseSensitive(root, "requested_resolution");
    cJSON *environment = cJSON_GetObjectItemCaseSensitive(root, "environment");
    cJSON *achievements = cJSON_GetObjectItemCaseSensitive(root, "achievements");

    if (!cJSON_IsNumber(schema) || schema->valuedouble != 1.0 || schema->valueint != 1 ||
        !cJSON_IsString(game_id) || !bloom_game_id_valid(game_id->valuestring) || !cJSON_IsString(system_id) ||
        !valid_system_id(system_id->valuestring) || !cJSON_IsString(rom_path) ||
        !path_under(rom_path->valuestring, "/mnt/SDCARD/Roms/") || !cJSON_IsString(launcher) ||
        !path_under(launcher->valuestring, "/mnt/SDCARD/Emu/") || strlen(launcher->valuestring) < strlen("/launch.sh") ||
        strcmp(launcher->valuestring + strlen(launcher->valuestring) - strlen("/launch.sh"), "/launch.sh") != 0 ||
        !cJSON_IsString(emulator) ||
        (strcmp(emulator->valuestring, "retroarch") != 0 && strcmp(emulator->valuestring, "standalone") != 0) ||
        !(cJSON_IsString(core) || cJSON_IsNull(core)) || !cJSON_IsBool(autoload) || !cJSON_IsArray(configs) ||
        !cJSON_IsNull(resolution) || !cJSON_IsObject(environment)) {
        set_error(error, error_size, "request schema or field value is invalid");
        return false;
    }
    if ((cJSON_IsString(core) && !valid_core(core->valuestring)) || !valid_achievements(achievements, error, error_size) ||
        (strcmp(emulator->valuestring, "retroarch") == 0 && !cJSON_IsString(core)) || environment->child != NULL) {
        set_error(error, error_size, "request launch configuration is invalid");
        return false;
    }
    if (achievements != NULL) {
        cJSON *achievement_mode = cJSON_GetObjectItemCaseSensitive(achievements, "mode");
        if (cJSON_IsString(achievement_mode) && strcmp(achievement_mode->valuestring, "hardcore") == 0 &&
            cJSON_IsTrue(autoload)) {
            set_error(error, error_size, "Hardcore cannot auto-load state");
            return false;
        }
    }
    char expected_game_id[BLOOM_GAME_ID_LENGTH + 1];
    char relative_path[4096];
    char game_id_error[128];
    if (bloom_game_id_create(system_id->valuestring, rom_path->valuestring, expected_game_id, sizeof(expected_game_id),
                             relative_path, sizeof(relative_path), game_id_error, sizeof(game_id_error)) != 0 ||
        strcmp(game_id->valuestring, expected_game_id) != 0) {
        set_error(error, error_size, "request GameID does not match its system and ROM path");
        return false;
    }
    cJSON *config = NULL;
    cJSON_ArrayForEach(config, configs)
    {
        if (!cJSON_IsString(config) ||
            !(path_under(config->valuestring, "/mnt/SDCARD/") || path_under(config->valuestring, "/tmp/bloom-session/"))) {
            set_error(error, error_size, "append config is invalid");
            return false;
        }
    }
    return true;
}

static int load_valid(const char *path, cJSON **request, char *error, size_t error_size)
{
    *request = load_request(path, error, error_size);
    if (*request == NULL)
        return -1;
    if (!valid_request(*request, error, error_size)) {
        cJSON_Delete(*request);
        *request = NULL;
        return -1;
    }
    return 0;
}

int bloom_launch_get_string(const char *request_path, const char *field, char *value, size_t value_size, char *error,
                            size_t error_size)
{
    static const char *readable[] = {"game_id", "system_id", "rom_path", "launcher", "emulator_type", "core"};
    bool allowed = false;
    for (size_t i = 0; i < sizeof(readable) / sizeof(readable[0]); i++)
        if (strcmp(field, readable[i]) == 0)
            allowed = true;
    if (!allowed || value == NULL || value_size == 0) {
        set_error(error, error_size, "request field is not readable");
        return -1;
    }
    cJSON *request = NULL;
    if (load_valid(request_path, &request, error, error_size) != 0)
        return -1;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(request, field);
    if (!cJSON_IsString(item) || strlen(item->valuestring) >= value_size) {
        set_error(error, error_size, "request field is unavailable or too long");
        cJSON_Delete(request);
        return -1;
    }
    strcpy(value, item->valuestring);
    cJSON_Delete(request);
    return 0;
}

int bloom_launch_validate_file(const char *request_path, char *error, size_t error_size)
{
    cJSON *request = NULL;
    if (load_valid(request_path, &request, error, error_size) != 0)
        return -1;
    cJSON_Delete(request);
    return 0;
}

static int fsync_parent_directory(const char *path)
{
    char *parent = strdup(path);
    if (parent == NULL)
        return -1;
    char *separator = strrchr(parent, '/');
    if (separator == NULL) {
        strcpy(parent, ".");
    }
    else if (separator == parent) {
        separator[1] = '\0';
    }
    else {
        *separator = '\0';
    }

    int directory = open(parent, O_RDONLY | O_DIRECTORY);
    free(parent);
    if (directory < 0)
        return -1;
    int result = fsync(directory);
    int saved_errno = errno;
    if (close(directory) != 0 && result == 0) {
        result = -1;
        saved_errno = errno;
    }
    if (result != 0 && (saved_errno == EINVAL || saved_errno == ENOTSUP))
        result = 0;
    errno = saved_errno;
    return result;
}

int bloom_launch_resolve_achievement_transport(const char *request_path, int enabled, const char *mode,
                                               int offline_casual, int proxy_ready, int ra_game_id,
                                               const char *core_certification, char *error, size_t error_size)
{
    if (!enabled)
        return bloom_launch_set_achievements(request_path, 0, "disabled", "unavailable", 0, "not_applicable",
                                             error, error_size);
    if (mode == NULL || core_certification == NULL) {
        set_error(error, error_size, "achievement policy field is invalid");
        return -1;
    }
    if (strcmp(mode, "hardcore") == 0)
        return bloom_launch_set_achievements(request_path, 1, "hardcore", "direct", ra_game_id,
                                             core_certification, error, error_size);
    if (strcmp(mode, "softcore") != 0) {
        set_error(error, error_size, "achievement mode is invalid");
        return -1;
    }
    if (offline_casual && !proxy_ready) {
        set_error(error, error_size, "offline casual proxy is unavailable");
        return -1;
    }
    return bloom_launch_set_achievements(request_path, 1, "softcore", offline_casual ? "proxy" : "direct",
                                         ra_game_id, core_certification, error, error_size);
}

static int atomic_write_bytes(const char *path, const char *data, size_t length, mode_t mode, char *error,
                              size_t error_size)
{
    size_t temp_size = strlen(path) + 32;
    char *temp_path = malloc(temp_size);
    if (temp_path == NULL) {
        set_error(error, error_size, "out of memory");
        return -1;
    }
    snprintf(temp_path, temp_size, "%s.tmp.%ld", path, (long)getpid());
    int fd = open(temp_path, O_WRONLY | O_CREAT | O_EXCL, mode);
    if (fd < 0) {
        set_error(error, error_size, "cannot create temporary file: %s", strerror(errno));
        free(temp_path);
        return -1;
    }
    size_t written = 0;
    while (written < length) {
        ssize_t result = write(fd, data + written, length - written);
        if (result <= 0)
            break;
        written += (size_t)result;
    }
    int failed = written != length;
    if (fsync(fd) != 0)
        failed = 1;
    if (fchmod(fd, mode) != 0)
        failed = 1;
    if (close(fd) != 0)
        failed = 1;
    if (!failed) {
        failed = rename(temp_path, path) != 0;
        if (!failed)
            failed = fsync_parent_directory(path) != 0;
    }
    if (failed) {
        set_error(error, error_size, "cannot write atomic file: %s", strerror(errno));
        unlink(temp_path);
    }
    free(temp_path);
    return failed ? -1 : 0;
}

int bloom_launch_create_file(const char *request_path, const char *game_id, const char *system_id, const char *rom_path,
                             const char *launcher, const char *emulator_type, const char *core, int auto_load_state,
                             char *error, size_t error_size)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *achievements = NULL;
    if (root == NULL || cJSON_AddNumberToObject(root, "schema", 1) == NULL ||
        cJSON_AddStringToObject(root, "game_id", game_id) == NULL ||
        cJSON_AddStringToObject(root, "system_id", system_id) == NULL ||
        cJSON_AddStringToObject(root, "rom_path", rom_path) == NULL ||
        cJSON_AddStringToObject(root, "launcher", launcher) == NULL ||
        cJSON_AddStringToObject(root, "emulator_type", emulator_type) == NULL ||
        (core == NULL ? cJSON_AddNullToObject(root, "core") : cJSON_AddStringToObject(root, "core", core)) == NULL ||
        cJSON_AddBoolToObject(root, "auto_load_state", auto_load_state != 0) == NULL ||
        cJSON_AddArrayToObject(root, "append_configs") == NULL || cJSON_AddNullToObject(root, "requested_resolution") == NULL ||
        cJSON_AddObjectToObject(root, "environment") == NULL ||
        (achievements = cJSON_AddObjectToObject(root, "achievements")) == NULL ||
        cJSON_AddBoolToObject(achievements, "enabled", false) == NULL ||
        cJSON_AddStringToObject(achievements, "mode", "disabled") == NULL ||
        cJSON_AddStringToObject(achievements, "transport", "unavailable") == NULL ||
        cJSON_AddNullToObject(achievements, "ra_game_id") == NULL ||
        cJSON_AddStringToObject(achievements, "core_certification", "not_applicable") == NULL) {
        cJSON_Delete(root);
        set_error(error, error_size, "cannot allocate request");
        return -1;
    }
    if (!valid_request(root, error, error_size)) {
        cJSON_Delete(root);
        return -1;
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        set_error(error, error_size, "cannot serialize request");
        return -1;
    }
    size_t length = strlen(json);
    char *with_newline = realloc(json, length + 2);
    if (with_newline == NULL) {
        free(json);
        set_error(error, error_size, "out of memory");
        return -1;
    }
    with_newline[length] = '\n';
    with_newline[length + 1] = '\0';
    int result = atomic_write_bytes(request_path, with_newline, length + 1, 0644, error, error_size);
    free(with_newline);
    return result;
}

static int write_request_json(const char *path, cJSON *root, char *error, size_t error_size)
{
    char *json = cJSON_PrintUnformatted(root);
    if (json == NULL) {
        set_error(error, error_size, "cannot serialize request");
        return -1;
    }
    size_t length = strlen(json);
    char *with_newline = realloc(json, length + 2);
    if (with_newline == NULL) {
        free(json);
        set_error(error, error_size, "out of memory");
        return -1;
    }
    with_newline[length] = '\n';
    with_newline[length + 1] = '\0';
    int result = atomic_write_bytes(path, with_newline, length + 1, 0644, error, error_size);
    free(with_newline);
    return result;
}

int bloom_launch_set_achievements(const char *request_path, int enabled, const char *mode, const char *transport,
                                  int ra_game_id, const char *core_certification, char *error, size_t error_size)
{
    if (mode == NULL || transport == NULL || core_certification == NULL) {
        set_error(error, error_size, "achievement policy field is invalid");
        return -1;
    }
    cJSON *request = NULL;
    if (load_valid(request_path, &request, error, error_size) != 0)
        return -1;
    cJSON *configs = cJSON_GetObjectItemCaseSensitive(request, "append_configs");
    if (cJSON_IsArray(configs) && cJSON_GetArraySize(configs) != 0) {
        cJSON_Delete(request);
        set_error(error, error_size, "achievement session policy is immutable after config generation");
        return -1;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(request, "achievements");
    cJSON *achievements = cJSON_AddObjectToObject(request, "achievements");
    if (achievements == NULL || cJSON_AddBoolToObject(achievements, "enabled", enabled != 0) == NULL ||
        cJSON_AddStringToObject(achievements, "mode", mode) == NULL ||
        cJSON_AddStringToObject(achievements, "transport", transport) == NULL ||
        (ra_game_id > 0 ? cJSON_AddNumberToObject(achievements, "ra_game_id", ra_game_id)
                        : cJSON_AddNullToObject(achievements, "ra_game_id")) == NULL ||
        cJSON_AddStringToObject(achievements, "core_certification", core_certification) == NULL ||
        !valid_request(request, error, error_size)) {
        cJSON_Delete(request);
        if (error != NULL && error[0] == '\0')
            set_error(error, error_size, "cannot create achievement policy");
        return -1;
    }
    int result = write_request_json(request_path, request, error, error_size);
    cJSON_Delete(request);
    return result;
}

static bool safe_config_value(const char *value)
{
    if (value == NULL || value[0] == '\0')
        return false;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor; cursor++)
        if (*cursor < 0x21 || *cursor > 0x7e || *cursor == '"' || *cursor == '\\')
            return false;
    return true;
}

int bloom_launch_write_ra_config(const char *request_path, const char *config_path, const char *username,
                                 const char *token, const char *proxy_host, char *error, size_t error_size)
{
    if (!path_under(config_path, "/tmp/bloom-session/")) {
        set_error(error, error_size, "achievement config path is invalid");
        return -1;
    }
    cJSON *request = NULL;
    if (load_valid(request_path, &request, error, error_size) != 0)
        return -1;
    cJSON *policy = cJSON_GetObjectItemCaseSensitive(request, "achievements");
    cJSON *enabled = policy ? cJSON_GetObjectItemCaseSensitive(policy, "enabled") : NULL;
    cJSON *mode = policy ? cJSON_GetObjectItemCaseSensitive(policy, "mode") : NULL;
    cJSON *transport = policy ? cJSON_GetObjectItemCaseSensitive(policy, "transport") : NULL;
    if (!cJSON_IsTrue(enabled) || !safe_config_value(username) || !safe_config_value(token)) {
        cJSON_Delete(request);
        set_error(error, error_size, "enabled authenticated achievement policy is required");
        return -1;
    }
    bool proxy = strcmp(transport->valuestring, "proxy") == 0;
    if (proxy && !safe_config_value(proxy_host)) {
        cJSON_Delete(request);
        set_error(error, error_size, "proxy host is invalid");
        return -1;
    }
    char config[1024];
    int length = snprintf(config, sizeof(config),
                          "cheevos_enable = \"true\"\ncheevos_username = \"%s\"\ncheevos_token = \"%s\"\n"
                          "cheevos_hardcore_mode_enable = \"%s\"\ncheevos_richpresence_enable = \"true\"\n"
                          "cheevos_leaderboards_enable = \"true\"\n",
                          username, token, strcmp(mode->valuestring, "hardcore") == 0 ? "true" : "false");
    if (strcmp(mode->valuestring, "hardcore") == 0)
        length += snprintf(config + length, sizeof(config) - (size_t)length,
                           "savestate_auto_load = \"false\"\nrewind_enable = \"false\"\n"
                           "run_ahead_enabled = \"false\"\npreemptive_frames = \"0\"\n"
                           "input_load_state = \"nul\"\ninput_rewind = \"nul\"\n"
                           "input_frame_advance = \"nul\"\ninput_slowmotion = \"nul\"\n"
                           "input_cheat_index_plus = \"nul\"\ninput_cheat_index_minus = \"nul\"\n"
                           "input_cheat_toggle = \"nul\"\n");
    if (proxy)
        length += snprintf(config + length, sizeof(config) - (size_t)length, "cheevos_custom_host = \"%s\"\n",
                           proxy_host);
    if (length <= 0 || (size_t)length >= sizeof(config)) {
        cJSON_Delete(request);
        set_error(error, error_size, "achievement config is too large");
        return -1;
    }
    if (atomic_write_bytes(config_path, config, (size_t)length, 0600, error, error_size) != 0) {
        cJSON_Delete(request);
        return -1;
    }
    cJSON *configs = cJSON_GetObjectItemCaseSensitive(request, "append_configs");
    if (cJSON_AddItemToArray(configs, cJSON_CreateString(config_path)) == 0 ||
        write_request_json(request_path, request, error, error_size) != 0) {
        cJSON_Delete(request);
        unlink(config_path);
        return -1;
    }
    cJSON_Delete(request);
    return 0;
}

static bool legacy_representable(const char *value)
{
    return strpbrk(value, "\"`\\") == NULL;
}

static int write_legacy_quoted(FILE *file, const char *value)
{
    if (fputc('"', file) == EOF)
        return -1;
    for (const char *p = value; *p != '\0'; p++) {
        if (fputc(*p, file) == EOF) {
            return -1;
        }
    }
    return fputc('"', file) == EOF ? -1 : 0;
}

int bloom_launch_write_legacy(const char *request_path, const char *command_path, char *error, size_t error_size)
{
    cJSON *request = NULL;
    if (load_valid(request_path, &request, error, error_size) != 0)
        return -1;
    cJSON *launcher = cJSON_GetObjectItemCaseSensitive(request, "launcher");
    cJSON *rom_path = cJSON_GetObjectItemCaseSensitive(request, "rom_path");
    cJSON *configs = cJSON_GetObjectItemCaseSensitive(request, "append_configs");
    if (!legacy_representable(launcher->valuestring) || !legacy_representable(rom_path->valuestring)) {
        set_error(error, error_size, "path cannot cross the legacy MainUI boundary");
        cJSON_Delete(request);
        return -1;
    }

    size_t temp_size = strlen(command_path) + 32;
    char *temp_path = malloc(temp_size);
    if (temp_path == NULL) {
        cJSON_Delete(request);
        set_error(error, error_size, "out of memory");
        return -1;
    }
    snprintf(temp_path, temp_size, "%s.tmp.%ld", command_path, (long)getpid());
    int fd = open(temp_path, O_WRONLY | O_CREAT | O_EXCL, 0755);
    if (fd < 0) {
        set_error(error, error_size, "cannot create temporary command: %s", strerror(errno));
        free(temp_path);
        cJSON_Delete(request);
        return -1;
    }
    FILE *file = fdopen(fd, "w");
    int failed = file == NULL || fputs("LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so ", file) == EOF;
    if (!failed && cJSON_GetArraySize(configs) > 0) {
        if (!path_under(request_path, "/tmp/bloom-session/") || !legacy_representable(request_path)) {
            failed = 1;
            set_error(error, error_size, "session request cannot cross the legacy MainUI boundary");
        }
        else {
            failed = fputs("\"/mnt/SDCARD/.tmp_update/bin/bloom-launch-run\" ", file) == EOF ||
                     write_legacy_quoted(file, request_path) != 0 || fputc('\n', file) == EOF;
        }
    }
    else if (!failed) {
        failed = write_legacy_quoted(file, launcher->valuestring) != 0 || fputc(' ', file) == EOF ||
                 write_legacy_quoted(file, rom_path->valuestring) != 0 || fputc('\n', file) == EOF;
    }
    if (!failed)
        failed = fflush(file) != 0 || fsync(fd) != 0 || fchmod(fd, 0755) != 0;
    if (file != NULL)
        failed = fclose(file) != 0 || failed;
    else
        close(fd);
    if (!failed) {
        failed = rename(temp_path, command_path) != 0;
        if (!failed)
            failed = fsync_parent_directory(command_path) != 0;
    }
    if (failed) {
        set_error(error, error_size, "cannot write legacy command: %s", strerror(errno));
        unlink(temp_path);
    }
    free(temp_path);
    cJSON_Delete(request);
    return failed ? -1 : 0;
}
