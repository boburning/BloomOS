#ifndef PLAY_ACTIVITY_IDENTITY_H
#define PLAY_ACTIVITY_IDENTITY_H

#include <sqlite3/sqlite3.h>

struct play_activity_identity_result {
    int updated;
    int deferred;
};

int play_activity_backfill_game_ids(sqlite3 *database, int dry_run, struct play_activity_identity_result *result);

#endif
