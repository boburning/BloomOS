#ifndef BLOOM_LAUNCH_H
#define BLOOM_LAUNCH_H

#include <stddef.h>

int bloom_launch_validate_file(const char *request_path, char *error, size_t error_size);
int bloom_launch_write_legacy(const char *request_path, const char *command_path, char *error, size_t error_size);
int bloom_launch_create_file(const char *request_path, const char *game_id, const char *system_id, const char *rom_path,
                             const char *launcher, const char *emulator_type, const char *core, int auto_load_state,
                             char *error, size_t error_size);
int bloom_launch_get_string(const char *request_path, const char *field, char *value, size_t value_size, char *error,
                            size_t error_size);
int bloom_launch_set_achievements(const char *request_path, int enabled, const char *mode, const char *transport,
                                  int ra_game_id, const char *core_certification, char *error, size_t error_size);
int bloom_launch_write_ra_config(const char *request_path, const char *config_path, const char *username,
                                 const char *token, const char *proxy_host, char *error, size_t error_size);

#endif
