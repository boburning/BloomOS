#ifndef PLAY_ACTIVITY_DURATION_H
#define PLAY_ACTIVITY_DURATION_H

#include <sqlite3/sqlite3.h>

int play_activity_set_latest_duration(sqlite3 *database, int rom_id, sqlite3_int64 duration_seconds,
                                      sqlite3_int64 updated_at, int *updated_rows);

#endif
