#ifndef BLOOM_LIBRARY_LEGACY_H
#define BLOOM_LIBRARY_LEGACY_H

#include <sqlite3/sqlite3.h>
#include <stddef.h>

typedef struct {
    int favorites;
    int recents;
    int matched;
    int unmatched;
    int duplicates;
    int invalid;
} BloomLibraryLegacyResult;

int bloom_library_import_legacy(sqlite3 *database, const char *rom_root,
                                const char *favorites_path, const char *recents_path,
                                BloomLibraryLegacyResult *result, char *error,
                                size_t error_size);

#endif
