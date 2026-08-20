#include "bloom_ra_account.h"

#include "cjson/cJSON.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void set_error(char *error, size_t size, const char *message)
{
    if (error != NULL && size > 0)
        snprintf(error, size, "%s", message);
}

static int safe_value(const char *value, size_t maximum)
{
    if (value == NULL || value[0] == '\0' || strlen(value) >= maximum)
        return 0;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor; cursor++)
        if (iscntrl(*cursor))
            return 0;
    return 1;
}

static int write_atomic(const char *path, const char *content, size_t length)
{
    char temporary[4096];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", path, (long)getpid()) >= (int)sizeof(temporary))
        return -1;
    int descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (descriptor < 0)
        return -1;
    size_t written = 0;
    while (written < length) {
        ssize_t result = write(descriptor, content + written, length - written);
        if (result <= 0) {
            close(descriptor);
            unlink(temporary);
            return -1;
        }
        written += (size_t)result;
    }
    int result = fsync(descriptor) == 0 && close(descriptor) == 0 && rename(temporary, path) == 0 ? 0 : -1;
    if (result != 0)
        unlink(temporary);
    if (result == 0)
        chmod(path, 0600);
    return result;
}

int bloom_ra_account_read_token(const char *credentials_path, char *token, size_t token_size)
{
    if (credentials_path == NULL || token == NULL || token_size < 2)
        return -1;
    struct stat metadata;
    if (lstat(credentials_path, &metadata) != 0 || !S_ISREG(metadata.st_mode) || S_ISLNK(metadata.st_mode) ||
        (metadata.st_mode & 077) != 0)
        return -1;
    int descriptor = open(credentials_path, O_RDONLY | O_NOFOLLOW);
    if (descriptor < 0)
        return -1;
    ssize_t length = read(descriptor, token, token_size - 1);
    char extra;
    ssize_t overflow = read(descriptor, &extra, 1);
    close(descriptor);
    if (length <= 0 || overflow != 0)
        return -1;
    token[length] = '\0';
    while (length > 0 && (token[length - 1] == '\n' || token[length - 1] == '\r'))
        token[--length] = '\0';
    return safe_value(token, token_size) ? 0 : -1;
}

int bloom_ra_account_load(const char *settings_path, const char *credentials_path, BloomRaAccountStatus *status,
                          char *error, size_t error_size)
{
    if (settings_path == NULL || credentials_path == NULL || status == NULL) {
        set_error(error, error_size, "invalid account request");
        return -1;
    }
    memset(status, 0, sizeof(*status));
    status->schema = BLOOM_RA_ACCOUNT_SCHEMA;
    snprintf(status->mode, sizeof(status->mode), "softcore");
    struct stat metadata;
    if (lstat(settings_path, &metadata) != 0)
        return errno == ENOENT ? 0 : -1;
    if (!S_ISREG(metadata.st_mode) || S_ISLNK(metadata.st_mode) || (metadata.st_mode & 077) != 0) {
        set_error(error, error_size, "account settings are invalid");
        return -1;
    }
    FILE *file = fopen(settings_path, "rb");
    if (file == NULL)
        return -1;
    char buffer[4096];
    size_t length = fread(buffer, 1, sizeof(buffer) - 1, file);
    int complete = feof(file);
    fclose(file);
    if (!complete) {
        set_error(error, error_size, "account settings are invalid");
        return -1;
    }
    buffer[length] = '\0';
    cJSON *root = cJSON_Parse(buffer);
    cJSON *schema = root ? cJSON_GetObjectItemCaseSensitive(root, "schema") : NULL;
    cJSON *username = root ? cJSON_GetObjectItemCaseSensitive(root, "username") : NULL;
    cJSON *enabled = root ? cJSON_GetObjectItemCaseSensitive(root, "enabled") : NULL;
    cJSON *mode = root ? cJSON_GetObjectItemCaseSensitive(root, "mode") : NULL;
    cJSON *offline = root ? cJSON_GetObjectItemCaseSensitive(root, "offline_casual") : NULL;
    if (!cJSON_IsNumber(schema) || schema->valueint != 1 || !cJSON_IsString(username) ||
        !safe_value(username->valuestring, sizeof(status->username)) || !cJSON_IsBool(enabled) ||
        !cJSON_IsString(mode) || (strcmp(mode->valuestring, "softcore") != 0 && strcmp(mode->valuestring, "hardcore") != 0) ||
        !cJSON_IsBool(offline)) {
        cJSON_Delete(root);
        set_error(error, error_size, "account settings are invalid");
        return -1;
    }
    snprintf(status->username, sizeof(status->username), "%s", username->valuestring);
    snprintf(status->mode, sizeof(status->mode), "%s", mode->valuestring);
    status->enabled = cJSON_IsTrue(enabled);
    status->offline_casual = cJSON_IsTrue(offline);
    char token[128];
    status->authenticated = bloom_ra_account_read_token(credentials_path, token, sizeof(token)) == 0;
    memset(token, 0, sizeof(token));
    cJSON_Delete(root);
    return 0;
}

int bloom_ra_account_store(const char *settings_path, const char *credentials_path, const char *username,
                           const char *token, int enabled, const char *mode, int offline_casual, char *error,
                           size_t error_size)
{
    if (!safe_value(username, 64) || !safe_value(token, 128) || mode == NULL ||
        (strcmp(mode, "softcore") != 0 && strcmp(mode, "hardcore") != 0) || (offline_casual && strcmp(mode, "hardcore") == 0)) {
        set_error(error, error_size, "invalid account settings");
        return -1;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "schema", BLOOM_RA_ACCOUNT_SCHEMA);
    cJSON_AddBoolToObject(root, "enabled", enabled != 0);
    cJSON_AddStringToObject(root, "username", username);
    cJSON_AddStringToObject(root, "mode", mode);
    cJSON_AddBoolToObject(root, "offline_casual", offline_casual != 0);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL || write_atomic(credentials_path, token, strlen(token)) != 0 ||
        write_atomic(settings_path, json, strlen(json)) != 0) {
        cJSON_free(json);
        set_error(error, error_size, "account settings could not be stored");
        return -1;
    }
    cJSON_free(json);
    return 0;
}

int bloom_ra_account_sign_out(const char *settings_path, const char *credentials_path, char *error,
                              size_t error_size)
{
    if ((unlink(credentials_path) != 0 && errno != ENOENT) || (unlink(settings_path) != 0 && errno != ENOENT)) {
        set_error(error, error_size, "account settings could not be removed");
        return -1;
    }
    return 0;
}
