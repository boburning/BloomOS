#include "bloom_save_flush.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#define BLOOM_PATH_MAX 4096
#define BLOOM_INFO_MAX (256 * 1024)

static void set_error(char *error, size_t size, const char *format, ...)
{
    if (error == NULL || size == 0)
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, size, format, args);
    va_end(args);
}

static bool valid_core(const char *core)
{
    static const char suffix[] = "_libretro.so";
    if (core == NULL || strlen(core) <= strlen(suffix) ||
        strcmp(core + strlen(core) - strlen(suffix), suffix) != 0)
        return false;
    for (const unsigned char *p = (const unsigned char *)core; *p != '\0'; p++) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' ||
              *p == '-' || *p == '.'))
            return false;
    }
    return true;
}

static bool valid_component(const char *value)
{
    if (value == NULL || value[0] == '\0' || strcmp(value, ".") == 0 || strcmp(value, "..") == 0)
        return false;
    for (const unsigned char *p = (const unsigned char *)value; *p != '\0'; p++) {
        if (*p < 0x20 || *p == 0x7f || *p == '/' || *p == '\\' || *p == '"')
            return false;
    }
    return true;
}

static int join_path(char *output, size_t size, const char *left, const char *right, char *error, size_t error_size)
{
    int length = snprintf(output, size, "%s/%s", left, right);
    if (length < 0 || (size_t)length >= size) {
        set_error(error, error_size, "path is too long");
        return -1;
    }
    return 0;
}

static int read_corename(const char *sd_root, const char *core, char *corename, size_t corename_size, char *error,
                         size_t error_size)
{
    char info_dir[BLOOM_PATH_MAX];
    char info_path[BLOOM_PATH_MAX];
    if (join_path(info_dir, sizeof(info_dir), sd_root, "RetroArch/.retroarch/cores", error, error_size) != 0 ||
        snprintf(info_path, sizeof(info_path), "%s/%.*s.info", info_dir, (int)(strlen(core) - strlen(".so")), core) >=
            (int)sizeof(info_path)) {
        set_error(error, error_size, "core info path is too long");
        return -1;
    }
    struct stat status;
    if (lstat(info_path, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size < 0 ||
        status.st_size > BLOOM_INFO_MAX) {
        set_error(error, error_size, "core info is missing or unsafe");
        return -1;
    }
    FILE *file = fopen(info_path, "r");
    if (file == NULL) {
        set_error(error, error_size, "cannot open core info: %s", strerror(errno));
        return -1;
    }
    char line[512];
    int found = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        char parsed[128];
        char trailing;
        if (sscanf(line, " corename = \"%127[^\"]\" %c", parsed, &trailing) == 1) {
            if (found || !valid_component(parsed) || strlen(parsed) >= corename_size) {
                set_error(error, error_size, "core corename is invalid or duplicated");
                fclose(file);
                return -1;
            }
            strcpy(corename, parsed);
            found = 1;
        }
    }
    if (ferror(file) || fclose(file) != 0 || !found) {
        set_error(error, error_size, "core corename is unavailable");
        return -1;
    }
    return 0;
}

static int flush_directory(const char *path, struct bloom_save_flush_result *result, bool missing_ok, char *error,
                           size_t error_size)
{
    struct stat status;
    if (lstat(path, &status) != 0) {
        if (missing_ok && errno == ENOENT)
            return 0;
        set_error(error, error_size, "cannot inspect save path: %s", strerror(errno));
        return -1;
    }
    if (!S_ISDIR(status.st_mode)) {
        set_error(error, error_size, "save path is not a safe directory");
        return -1;
    }
    DIR *directory = opendir(path);
    if (directory == NULL) {
        set_error(error, error_size, "cannot open save directory: %s", strerror(errno));
        return -1;
    }
    struct dirent *entry;
    int failed = 0;
    while (!failed && (entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char child[BLOOM_PATH_MAX];
        if (join_path(child, sizeof(child), path, entry->d_name, error, error_size) != 0) {
            failed = 1;
            break;
        }
        if (lstat(child, &status) != 0) {
            set_error(error, error_size, "cannot inspect save entry: %s", strerror(errno));
            failed = 1;
        }
        else if (S_ISDIR(status.st_mode)) {
            failed = flush_directory(child, result, false, error, error_size) != 0;
        }
        else if (S_ISREG(status.st_mode)) {
            int descriptor = open(child, O_RDONLY | O_NOFOLLOW);
            if (descriptor < 0) {
                set_error(error, error_size, "cannot open save file: %s", strerror(errno));
                failed = 1;
            }
            else if (fsync(descriptor) != 0) {
                set_error(error, error_size, "cannot flush save file: %s", strerror(errno));
                close(descriptor);
                failed = 1;
            }
            else if (close(descriptor) != 0) {
                set_error(error, error_size, "cannot close flushed save file: %s", strerror(errno));
                failed = 1;
            }
            else {
                result->files_flushed++;
            }
        }
        else {
            set_error(error, error_size, "save tree contains an unsafe entry");
            failed = 1;
        }
    }
    if (closedir(directory) != 0 && !failed) {
        set_error(error, error_size, "cannot close save directory");
        failed = 1;
    }
    if (failed)
        return -1;
    int descriptor = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (descriptor < 0) {
        set_error(error, error_size, "cannot open save directory for flush: %s", strerror(errno));
        return -1;
    }
    errno = 0;
    if (fsync(descriptor) != 0 && errno != EINVAL) {
        set_error(error, error_size, "cannot flush save directory: %s", strerror(errno));
        close(descriptor);
        return -1;
    }
    if (close(descriptor) != 0) {
        set_error(error, error_size, "cannot close flushed save directory");
        return -1;
    }
    result->directories_flushed++;
    return 0;
}

static int flush_directory_entry(const char *path, struct bloom_save_flush_result *result, char *error,
                                 size_t error_size)
{
    struct stat status;
    if (lstat(path, &status) != 0) {
        if (errno == ENOENT)
            return 0;
        set_error(error, error_size, "cannot inspect save parent: %s", strerror(errno));
        return -1;
    }
    if (!S_ISDIR(status.st_mode)) {
        set_error(error, error_size, "save parent is not a safe directory");
        return -1;
    }
    int descriptor = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (descriptor < 0) {
        set_error(error, error_size, "cannot open save parent for flush: %s", strerror(errno));
        return -1;
    }
    errno = 0;
    if (fsync(descriptor) != 0 && errno != EINVAL) {
        set_error(error, error_size, "cannot flush save parent: %s", strerror(errno));
        close(descriptor);
        return -1;
    }
    if (close(descriptor) != 0) {
        set_error(error, error_size, "cannot close flushed save parent");
        return -1;
    }
    result->directories_flushed++;
    return 0;
}

int bloom_save_flush(const char *sd_root, const char *core, struct bloom_save_flush_result *result, char *error,
                     size_t error_size)
{
    if (sd_root == NULL || sd_root[0] != '/' || !valid_core(core) || result == NULL) {
        set_error(error, error_size, "root or core name is invalid");
        return -1;
    }
    memset(result, 0, sizeof(*result));
    if (read_corename(sd_root, core, result->corename, sizeof(result->corename), error, error_size) != 0)
        return -1;
    static const char *kinds[] = {"saves", "states"};
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        char parent[BLOOM_PATH_MAX];
        char path[BLOOM_PATH_MAX];
        char relative[256];
        snprintf(relative, sizeof(relative), "Saves/CurrentProfile/%s", kinds[i]);
        if (join_path(parent, sizeof(parent), sd_root, relative, error, error_size) != 0 ||
            join_path(path, sizeof(path), parent, result->corename, error, error_size) != 0)
            return -1;
        if (flush_directory(path, result, true, error, error_size) != 0)
            return -1;
        if (flush_directory_entry(parent, result, error, error_size) != 0)
            return -1;
    }
    return 0;
}
