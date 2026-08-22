#ifndef BLOOM_LIBRARY_MUTATION_H
#define BLOOM_LIBRARY_MUTATION_H

#include <sqlite3/sqlite3.h>

int bloom_library_favorite_set(sqlite3 *database, const char *game_id, int favorite,
                               int *changed);

#endif
