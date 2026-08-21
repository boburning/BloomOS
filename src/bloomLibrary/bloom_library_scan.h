#ifndef BLOOM_LIBRARY_SCAN_H
#define BLOOM_LIBRARY_SCAN_H

#include <sqlite3/sqlite3.h>
#include <stddef.h>

typedef struct {
    int changed;
    int generation;
    int systems;
    int games;
    int errors;
} BloomLibraryScanResult;

int bloom_library_scan_games(sqlite3 *database, const char *rom_root, const char *system_id,
                             BloomLibraryScanResult *result, char *error, size_t error_size);

#endif
