#ifndef PLAY_ACTIVITY_MIGRATIONS_H
#define PLAY_ACTIVITY_MIGRATIONS_H

#include <sqlite3/sqlite3.h>

#define PLAY_ACTIVITY_SCHEMA_VERSION 1

int play_activity_schema_version(sqlite3 *database, int *version);
int play_activity_schema_migrate(sqlite3 *database, const char *backup_path);

#endif
