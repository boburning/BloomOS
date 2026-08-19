#ifndef BLOOM_RA_H
#define BLOOM_RA_H

#include <sqlite3/sqlite3.h>
#include <stddef.h>
#include <stdint.h>

#define BLOOM_RA_SCHEMA 1

typedef struct {
    int schema;
    int enabled;
    const char *state;
    const char *catalog_status;
    unsigned long indexed_games;
    unsigned long identified_games;
} BloomRaStatus;

typedef struct {
    int schema;
    const char *game_id;
    const char *status;
    int has_ra_badge;
    int ra_game_id;
    int official_set;
    unsigned long achievement_count;
} BloomRaGame;

void bloom_ra_get_status(BloomRaStatus *status);
int bloom_ra_get_game(const char *game_id, BloomRaGame *game, char *error, size_t error_size);
int bloom_ra_get_game_from_database(sqlite3 *database, const char *game_id, BloomRaGame *game, char *error,
                                    size_t error_size);
int bloom_ra_console_id(const char *system_id, uint32_t *console_id);
int bloom_ra_hash_file(const char *system_id, const char *rom_path, const char *rom_root, char hash[33], char *error,
                       size_t error_size);

#endif
