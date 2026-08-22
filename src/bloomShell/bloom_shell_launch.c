#include "bloom_shell_launch.h"

#include "../bloomLaunch/bloom_launch.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void set_error(char *error, size_t size, const char *format, ...)
{
    if (error == NULL || size == 0)
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, size, format, args);
    va_end(args);
}

static int safe_relative(const char *value)
{
    return value != NULL && value[0] != '\0' && value[0] != '/' && strstr(value, "../") == NULL &&
           strcmp(value, "..") != 0 && strstr(value, "/..") == NULL;
}

int bloom_shell_detect_core(const char *sd_root, const char *launch_path, char *core,
                            size_t core_size)
{
    if (sd_root == NULL || sd_root[0] != '/' || launch_path == NULL ||
        strncmp(launch_path, "Emu/", 4) != 0 || !safe_relative(launch_path) || core == NULL ||
        core_size < 16)
        return -1;
    char path[1024];
    if (snprintf(path, sizeof(path), "%s/%s", sd_root, launch_path) >= (int)sizeof(path))
        return -1;
    struct stat metadata;
    if (lstat(path, &metadata) != 0 || !S_ISREG(metadata.st_mode) || S_ISLNK(metadata.st_mode) ||
        metadata.st_size <= 0 || metadata.st_size > 65536)
        return -1;
    int descriptor = open(path, O_RDONLY | O_NOFOLLOW);
    if (descriptor < 0)
        return -1;
    struct stat opened;
    if (fstat(descriptor, &opened) != 0 || !S_ISREG(opened.st_mode) ||
        opened.st_dev != metadata.st_dev || opened.st_ino != metadata.st_ino ||
        opened.st_size != metadata.st_size) {
        close(descriptor);
        return -1;
    }
    char script[65537];
    size_t length = 0;
    int failed = 0;
    while (!failed && length < (size_t)opened.st_size) {
        ssize_t chunk = read(descriptor, script + length, (size_t)opened.st_size - length);
        if (chunk > 0)
            length += (size_t)chunk;
        else if (chunk == 0)
            failed = 1;
        else if (errno != EINTR)
            failed = 1;
    }
    failed = close(descriptor) != 0 || failed || length == 0 || length != (size_t)opened.st_size;
    if (failed)
        return -1;
    script[length] = '\0';
    const char suffix[] = "_libretro.so";
    char *suffix_start = strstr(script, suffix);
    if (suffix_start == NULL)
        return -1;
    char *end = suffix_start + sizeof(suffix) - 1;
    char *start = suffix_start;
    while (start > script &&
           ((start[-1] >= 'a' && start[-1] <= 'z') || (start[-1] >= 'A' && start[-1] <= 'Z') ||
            (start[-1] >= '0' && start[-1] <= '9') || start[-1] == '_' || start[-1] == '-'))
        --start;
    size_t core_length = (size_t)(end - start);
    if (start == suffix_start || core_length >= core_size)
        return -1;
    memcpy(core, start, core_length);
    core[core_length] = '\0';
    return 0;
}

static int write_quoted(int descriptor, const char *value)
{
    if (dprintf(descriptor, "'") < 0)
        return -1;
    for (const char *cursor = value; *cursor != '\0'; ++cursor) {
        if (*cursor == '\'' && dprintf(descriptor, "'\\''") < 0)
            return -1;
        if (*cursor != '\'' && write(descriptor, cursor, 1) != 1)
            return -1;
    }
    return dprintf(descriptor, "'") < 0 ? -1 : 0;
}

static int sync_parent(const char *path)
{
    char parent[1100];
    if (snprintf(parent, sizeof(parent), "%s", path) >= (int)sizeof(parent))
        return -1;
    char *separator = strrchr(parent, '/');
    if (separator == NULL)
        return -1;
    *separator = '\0';
    int descriptor = open(parent[0] == '\0' ? "/" : parent, O_RDONLY | O_DIRECTORY);
    if (descriptor < 0)
        return -1;
    int result = fsync(descriptor);
    if (close(descriptor) != 0)
        result = -1;
    return result;
}

int bloom_shell_stage_executable(const char *executable, const char *command_path,
                                 char *error, size_t error_size)
{
    if (executable == NULL || executable[0] != '/' || command_path == NULL ||
        command_path[0] != '/') {
        set_error(error, error_size, "executable launch arguments are invalid");
        return -1;
    }
    char temporary[1100];
    struct stat metadata;
    if (snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", command_path, (long)getpid()) >=
            (int)sizeof(temporary) ||
        lstat(command_path, &metadata) == 0 || lstat(temporary, &metadata) == 0 ||
        lstat(executable, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
        S_ISLNK(metadata.st_mode) || access(executable, X_OK) != 0) {
        set_error(error, error_size, "executable launch boundary is unavailable");
        return -1;
    }
    int descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0700);
    int failed = descriptor < 0;
    int published = 0;
    if (!failed) {
        failed = dprintf(descriptor, "#!/bin/sh\nexec ") < 0 ||
                 write_quoted(descriptor, executable) != 0 || dprintf(descriptor, "\n") < 0 ||
                 fsync(descriptor) != 0;
        if (close(descriptor) != 0)
            failed = 1;
        descriptor = -1;
    }
    if (!failed) {
        failed = rename(temporary, command_path) != 0;
        published = !failed;
    }
    if (!failed)
        failed = sync_parent(command_path) != 0;
    if (failed) {
        if (descriptor >= 0)
            close(descriptor);
        unlink(temporary);
        if (published) {
            unlink(command_path);
            sync_parent(command_path);
        }
        set_error(error, error_size, "executable launch staging failed");
        return -1;
    }
    return 0;
}

int bloom_shell_stage_app(const BloomLibraryApp *app, int allow_development, const char *sd_root,
                          const char *command_path, char *error, size_t error_size)
{
    if (app == NULL || (allow_development != 0 && allow_development != 1) || sd_root == NULL ||
        command_path == NULL || command_path[0] != '/' ||
        (strcmp(app->compatibility, "bloom-native") != 0 &&
         strcmp(app->compatibility, "onion-compatible") != 0 &&
         (!allow_development || strcmp(app->compatibility, "development-only") != 0)) ||
        strncmp(app->launch_path, "App/", 4) != 0 || !safe_relative(app->launch_path)) {
        set_error(error, error_size, "application is not launchable");
        return -1;
    }
    char launcher[1024];
    char temporary[1100];
    if (snprintf(launcher, sizeof(launcher), "%s/%s", sd_root, app->launch_path) >=
            (int)sizeof(launcher) ||
        snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", command_path, (long)getpid()) >=
            (int)sizeof(temporary)) {
        set_error(error, error_size, "application launch path is too long");
        return -1;
    }
    struct stat metadata;
    if (lstat(command_path, &metadata) == 0 || lstat(temporary, &metadata) == 0 ||
        lstat(launcher, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
        S_ISLNK(metadata.st_mode) || access(launcher, X_OK) != 0) {
        set_error(error, error_size, "application launch boundary is unavailable");
        return -1;
    }
    int descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0700);
    int failed = descriptor < 0;
    int published = 0;
    if (!failed) {
        failed = dprintf(descriptor, "#!/bin/sh\nexec ") < 0 || write_quoted(descriptor, launcher) != 0 ||
                 dprintf(descriptor, "\n") < 0 || fsync(descriptor) != 0;
        if (close(descriptor) != 0)
            failed = 1;
        descriptor = -1;
    }
    if (!failed) {
        failed = rename(temporary, command_path) != 0;
        published = !failed;
    }
    if (!failed)
        failed = sync_parent(command_path) != 0;
    if (failed) {
        if (descriptor >= 0)
            close(descriptor);
        unlink(temporary);
        if (published) {
            unlink(command_path);
            sync_parent(command_path);
        }
        set_error(error, error_size, "application launch staging failed");
        return -1;
    }
    return 0;
}

static int run_session(const char *binary, const char *first, const char *second,
                       const char *third)
{
    pid_t child = fork();
    if (child < 0)
        return -1;
    if (child == 0) {
        if (third != NULL)
            execl(binary, binary, first, second, third, (char *)NULL);
        else if (second == NULL)
            execl(binary, binary, first, (char *)NULL);
        else
            execl(binary, binary, first, second, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

int bloom_shell_stage_game(const BloomLibraryGame *game, const char *core,
                           const char *request_path, const char *command_path,
                           const char *session_request_path, const char *session_binary,
                           char *error, size_t error_size)
{
    if (game == NULL || core == NULL || request_path == NULL || command_path == NULL ||
        session_request_path == NULL || session_binary == NULL || !safe_relative(game->normalized_rom_path) ||
        !safe_relative(game->launch_path)) {
        set_error(error, error_size, "launch arguments are invalid");
        return -1;
    }
    struct stat metadata;
    if (lstat(command_path, &metadata) == 0 || access(session_binary, X_OK) != 0) {
        set_error(error, error_size, "launch boundary is unavailable");
        return -1;
    }
    char rom_path[1024];
    char launcher[1024];
    if (snprintf(rom_path, sizeof(rom_path), "/mnt/SDCARD/Roms/%s", game->normalized_rom_path) >=
            (int)sizeof(rom_path) ||
        snprintf(launcher, sizeof(launcher), "/mnt/SDCARD/%s", game->launch_path) >=
            (int)sizeof(launcher)) {
        set_error(error, error_size, "launch path is too long");
        return -1;
    }
    if (bloom_launch_create_file(request_path, game->bloom_game_id, game->system_id, rom_path,
                                 launcher, "retroarch", core, 0, error, error_size) != 0)
        return -1;
    if (run_session(session_binary, "start", request_path, NULL) != 0) {
        unlink(request_path);
        set_error(error, error_size, "session launch staging failed");
        return -1;
    }
    if (bloom_launch_write_legacy(session_request_path, command_path, error, error_size) != 0 ||
        run_session(session_binary, "transition", "PREPARING", "STARTING") != 0) {
        unlink(command_path);
        run_session(session_binary, "fail", "shell_launch_failed", NULL);
        unlink(request_path);
        set_error(error, error_size, "session launch staging failed");
        return -1;
    }
    unlink(request_path);
    return 0;
}
