#ifndef BLOOM_RA_H
#define BLOOM_RA_H

#include <stddef.h>

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
    unsigned long achievement_count;
} BloomRaGame;

void bloom_ra_get_status(BloomRaStatus *status);
int bloom_ra_get_game(const char *game_id, BloomRaGame *game, char *error, size_t error_size);

#endif
