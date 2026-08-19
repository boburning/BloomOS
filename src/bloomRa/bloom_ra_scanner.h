#ifndef BLOOM_RA_SCANNER_H
#define BLOOM_RA_SCANNER_H

#include <sqlite3/sqlite3.h>

typedef struct {
    int skipped;
    int identified;
    const char *status;
} BloomRaScanResult;

typedef struct {
    unsigned long processed;
    unsigned long skipped;
    unsigned long identified;
    unsigned long errors;
    int canceled;
    int paused;
} BloomRaScanStats;

int bloom_ra_scan_game(sqlite3 *database, const char *bloom_game_id, const char *system_id, const char *rom_path,
                       const char *rom_root, const char *normalized_rom_path, int force, BloomRaScanResult *scan_result);
int bloom_ra_scan_tree(sqlite3 *database, const char *system_id, const char *system_path, int force,
                       const char *session_state_path, const char *cancel_path, BloomRaScanStats *stats);

#endif
