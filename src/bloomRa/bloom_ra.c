#include "bloom_ra.h"

#include "../bloomGameId/bloom_game_id.h"

#include <stdio.h>

void bloom_ra_get_status(BloomRaStatus *status)
{
    if (status == NULL)
        return;

    status->schema = BLOOM_RA_SCHEMA;
    status->enabled = 0;
    status->state = "not_configured";
    status->catalog_status = "not_implemented";
    status->indexed_games = 0;
    status->identified_games = 0;
}

int bloom_ra_get_game(const char *game_id, BloomRaGame *game, char *error, size_t error_size)
{
    if (game == NULL) {
        if (error != NULL && error_size > 0)
            snprintf(error, error_size, "missing game output");
        return -1;
    }

    if (!bloom_game_id_valid(game_id)) {
        if (error != NULL && error_size > 0)
            snprintf(error, error_size, "invalid Bloom GameID");
        return -1;
    }
    game->schema = BLOOM_RA_SCHEMA;
    game->game_id = game_id;
    game->status = "unindexed";
    game->has_ra_badge = 0;
    game->achievement_count = 0;
    return 0;
}
