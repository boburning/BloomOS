#include "playActivityHealth.h"

#include "playActivityMigrations.h"

#include <string.h>

static int scalar(sqlite3 *database, const char *sql, int *value)
{
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(database, sql, -1, &statement, NULL);
    if (result != SQLITE_OK)
        return result;
    result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        *value = sqlite3_column_int(statement, 0);
        result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : SQLITE_CORRUPT;
    }
    else {
        result = result == SQLITE_DONE ? SQLITE_CORRUPT : result;
    }
    sqlite3_finalize(statement);
    return result;
}

int play_activity_health_check(sqlite3 *database, struct play_activity_health *health)
{
    if (database == NULL || health == NULL)
        return SQLITE_MISUSE;
    memset(health, 0, sizeof(*health));
    int result = play_activity_schema_version(database, &health->schema_version);
    if (result != SQLITE_OK)
        return result;
    if (health->schema_version != PLAY_ACTIVITY_SCHEMA_VERSION)
        return SQLITE_MISMATCH;
    result = play_activity_schema_migrate(database, NULL);
    if (result != SQLITE_OK)
        return result;
    sqlite3_stmt *statement = NULL;
    result = sqlite3_prepare_v2(database, "PRAGMA quick_check", -1, &statement, NULL);
    if (result != SQLITE_OK)
        return result;
    result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        const unsigned char *value = sqlite3_column_text(statement, 0);
        health->quick_check_ok = value != NULL && strcmp((const char *)value, "ok") == 0;
        result = sqlite3_step(statement) == SQLITE_DONE ? SQLITE_OK : SQLITE_CORRUPT;
    }
    else {
        result = result == SQLITE_DONE ? SQLITE_CORRUPT : result;
    }
    sqlite3_finalize(statement);
    if (result != SQLITE_OK)
        return result;
    if ((result = scalar(database,
                         "SELECT COUNT(*) FROM play_activity p LEFT JOIN rom r ON r.id=p.rom_id WHERE r.id IS NULL",
                         &health->orphan_activities)) != SQLITE_OK ||
        (result = scalar(database, "SELECT COUNT(*) FROM play_activity WHERE play_time < 0",
                         &health->negative_durations)) != SQLITE_OK ||
        (result = scalar(database, "SELECT COUNT(*) FROM play_activity WHERE play_time IS NULL",
                         &health->active_sessions)) != SQLITE_OK ||
        (result = scalar(database, "SELECT COUNT(*) FROM rom WHERE game_id IS NULL", &health->unidentified_roms)) !=
            SQLITE_OK)
        return result;
    return SQLITE_OK;
}
