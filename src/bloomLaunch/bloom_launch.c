#include "bloom_launch.h"

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
                                    "append_configs", "requested_resolution", "environment"};
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

    if (!cJSON_IsNumber(schema) || schema->valuedouble != 1.0 || schema->valueint != 1 ||
        !cJSON_IsString(game_id) || !safe_text(game_id->valuestring) || !cJSON_IsString(system_id) ||
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
    if ((cJSON_IsString(core) && !valid_core(core->valuestring)) ||
        (strcmp(emulator->valuestring, "retroarch") == 0 && !cJSON_IsString(core)) || environment->child != NULL) {
        set_error(error, error_size, "request launch configuration is invalid");
        return false;
    }
    cJSON *config = NULL;
    cJSON_ArrayForEach(config, configs)
    {
        if (!cJSON_IsString(config) || !path_under(config->valuestring, "/mnt/SDCARD/")) {
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
    if (!failed)
        failed = rename(temp_path, path) != 0;
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
    if (root == NULL || cJSON_AddNumberToObject(root, "schema", 1) == NULL ||
        cJSON_AddStringToObject(root, "game_id", game_id) == NULL ||
        cJSON_AddStringToObject(root, "system_id", system_id) == NULL ||
        cJSON_AddStringToObject(root, "rom_path", rom_path) == NULL ||
        cJSON_AddStringToObject(root, "launcher", launcher) == NULL ||
        cJSON_AddStringToObject(root, "emulator_type", emulator_type) == NULL ||
        (core == NULL ? cJSON_AddNullToObject(root, "core") : cJSON_AddStringToObject(root, "core", core)) == NULL ||
        cJSON_AddBoolToObject(root, "auto_load_state", auto_load_state != 0) == NULL ||
        cJSON_AddArrayToObject(root, "append_configs") == NULL || cJSON_AddNullToObject(root, "requested_resolution") == NULL ||
        cJSON_AddObjectToObject(root, "environment") == NULL) {
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
    int failed = file == NULL || fputs("LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so ", file) == EOF ||
                 write_legacy_quoted(file, launcher->valuestring) != 0 || fputc(' ', file) == EOF ||
                 write_legacy_quoted(file, rom_path->valuestring) != 0 || fputc('\n', file) == EOF;
    if (!failed)
        failed = fflush(file) != 0 || fsync(fd) != 0 || fchmod(fd, 0755) != 0;
    if (file != NULL)
        failed = fclose(file) != 0 || failed;
    else
        close(fd);
    if (!failed)
        failed = rename(temp_path, command_path) != 0;
    if (failed) {
        set_error(error, error_size, "cannot write legacy command: %s", strerror(errno));
        unlink(temp_path);
    }
    free(temp_path);
    cJSON_Delete(request);
    return failed ? -1 : 0;
}
