#ifndef BLOOM_RA_CATALOG_H
#define BLOOM_RA_CATALOG_H

#include <sqlite3/sqlite3.h>

typedef struct {
    const char *name;
    int (*import_console)(sqlite3 *database, int console_id, const char *revision, const char *json);
} BloomRaCatalogProvider;

const BloomRaCatalogProvider *bloom_ra_official_catalog_provider(void);
int bloom_ra_catalog_resolve(sqlite3 *database, int console_id, const char *content_hash, int *ra_game_id,
                             int *achievement_count);

#endif
