#ifndef BLOOM_LIBRARY_DATABASE_H
#define BLOOM_LIBRARY_DATABASE_H

#include <sqlite3/sqlite3.h>

#define BLOOM_LIBRARY_DATABASE_SCHEMA_VERSION 2

typedef struct {
    int schema_version;
    int generation;
    char status[16];
    int systems;
    int games;
    int apps;
    int favorites;
} BloomLibraryHealth;

int bloom_library_database_open(const char *path, sqlite3 **database);
int bloom_library_database_migrate(sqlite3 *database);
int bloom_library_database_health(sqlite3 *database, BloomLibraryHealth *health);

#endif
