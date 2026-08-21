#include "bloom_shell_launch.h"

#include "../bloomLaunch/bloom_launch.h"

#include <errno.h>
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

static int run_session(const char *binary, const char *first, const char *second)
{
    pid_t child = fork();
    if (child < 0)
        return -1;
    if (child == 0) {
        if (second == NULL)
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
    if (run_session(session_binary, "start", request_path) != 0) {
        unlink(request_path);
        set_error(error, error_size, "session launch staging failed");
        return -1;
    }
    if (bloom_launch_write_legacy(session_request_path, command_path, error, error_size) != 0 ||
        run_session(session_binary, "transition", "PREPARING:STARTING") != 0) {
        unlink(command_path);
        run_session(session_binary, "fail", "shell_launch_failed");
        unlink(request_path);
        set_error(error, error_size, "session launch staging failed");
        return -1;
    }
    unlink(request_path);
    return 0;
}
