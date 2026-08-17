#include "playActivityDuration.h"

#include <limits.h>
#include <stddef.h>

int play_activity_set_latest_duration(sqlite3 *database, int rom_id, sqlite3_int64 duration_seconds,
                                      sqlite3_int64 updated_at, int *updated_rows)
{
    if (database == NULL || rom_id < 0 || duration_seconds < 0 || duration_seconds > INT_MAX || updated_at < 0 ||
        updated_rows == NULL)
        return SQLITE_MISUSE;

    *updated_rows = 0;
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database,
        "UPDATE play_activity SET play_time=?1, updated_at=?2 "
        "WHERE rowid=(SELECT rowid FROM play_activity WHERE rom_id=?3 AND play_time IS NULL "
        "ORDER BY rowid DESC LIMIT 1)",
        -1, &statement, NULL);
    if (result != SQLITE_OK)
        return result;

    sqlite3_bind_int64(statement, 1, duration_seconds);
    sqlite3_bind_int64(statement, 2, updated_at);
    sqlite3_bind_int(statement, 3, rom_id);
    result = sqlite3_step(statement);
    if (result == SQLITE_DONE) {
        *updated_rows = sqlite3_changes(database);
        result = SQLITE_OK;
    }
    sqlite3_finalize(statement);
    return result;
}
