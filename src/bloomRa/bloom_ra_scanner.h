#ifndef BLOOM_RA_SCANNER_H
#define BLOOM_RA_SCANNER_H

#include <sqlite3/sqlite3.h>

typedef struct {
    int skipped;
    int identified;
    const char *status;
} BloomRaScanResult;

int bloom_ra_scan_game(sqlite3 *database, const char *bloom_game_id, const char *system_id, const char *rom_path,
                       const char *rom_root, const char *normalized_rom_path, int force, BloomRaScanResult *scan_result);

#endif
