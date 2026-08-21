#ifndef BLOOM_LIBRARY_IMPORT_H
#define BLOOM_LIBRARY_IMPORT_H

#include <sqlite3/sqlite3.h>
#include <stddef.h>

typedef struct {
    int changed;
    int generation;
    int systems;
    int apps;
} BloomLibraryImportResult;

int bloom_library_import_onion(sqlite3 *database, const char *system_catalog_path,
                               const char *emu_root, const char *app_root,
                               BloomLibraryImportResult *result, char *error,
                               size_t error_size);

#endif
