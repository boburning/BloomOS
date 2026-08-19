#ifndef BLOOM_RA_DATABASE_H
#define BLOOM_RA_DATABASE_H

#include <sqlite3/sqlite3.h>

#define BLOOM_RA_DATABASE_SCHEMA_VERSION 1
#define BLOOM_RA_DATABASE_PATH "/mnt/SDCARD/.bloom/achievements/catalog.sqlite3"

int bloom_ra_database_open(const char *path, sqlite3 **database);
int bloom_ra_database_version(sqlite3 *database, int *version);
int bloom_ra_database_migrate(sqlite3 *database);
int bloom_ra_database_health(sqlite3 *database, int *version, int *indexed_games, int *identified_games);

#endif
