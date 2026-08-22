#ifndef BLOOM_SHELL_LAUNCH_H
#define BLOOM_SHELL_LAUNCH_H

#include "../bloomLibrary/bloom_library_query.h"

#include <stddef.h>

int bloom_shell_stage_game(const BloomLibraryGame *game, const char *core,
                           const char *request_path, const char *command_path,
                           const char *session_request_path, const char *session_binary,
                           char *error, size_t error_size);

int bloom_shell_stage_app(const BloomLibraryApp *app, const char *sd_root,
                          const char *command_path, char *error, size_t error_size);

int bloom_shell_stage_executable(const char *executable, const char *command_path,
                                 char *error, size_t error_size);

#endif
