#ifndef BLOOM_LAUNCH_H
#define BLOOM_LAUNCH_H

#include <stddef.h>

int bloom_launch_validate_file(const char *request_path, char *error, size_t error_size);
int bloom_launch_write_legacy(const char *request_path, const char *command_path, char *error, size_t error_size);
int bloom_launch_create_file(const char *request_path, const char *game_id, const char *system_id, const char *rom_path,
                             const char *launcher, const char *emulator_type, const char *core, int auto_load_state,
                             char *error, size_t error_size);

#endif
