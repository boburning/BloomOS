#ifndef PLAY_ACTIVITY_HEALTH_H
#define PLAY_ACTIVITY_HEALTH_H

#include <sqlite3/sqlite3.h>

struct play_activity_health {
    int schema_version;
    int quick_check_ok;
    int orphan_activities;
    int negative_durations;
    int active_sessions;
    int unidentified_roms;
};

int play_activity_health_check(sqlite3 *database, struct play_activity_health *health);

#endif
