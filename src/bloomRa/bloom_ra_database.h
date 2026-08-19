#ifndef BLOOM_RA_DATABASE_H
#define BLOOM_RA_DATABASE_H

#include <sqlite3/sqlite3.h>

#define BLOOM_RA_DATABASE_SCHEMA_VERSION 1
#define BLOOM_RA_DATABASE_PATH "/mnt/SDCARD/.bloom/achievements/catalog.sqlite3"

int bloom_ra_database_open(const char *path, sqlite3 **database);
int bloom_ra_database_version(sqlite3 *database, int *version);
int bloom_ra_database_migrate(sqlite3 *database);
int bloom_ra_database_health(sqlite3 *database, int *version, int *indexed_games, int *identified_games);
int bloom_ra_database_catalog_status(sqlite3 *database, char *status, unsigned long status_size);

typedef int (*BloomRaCollectionVisitor)(const char *game_id, const char *system_id, void *context);
int bloom_ra_database_collection(sqlite3 *database, BloomRaCollectionVisitor visitor, void *context,
                                 unsigned long *count);

#endif
