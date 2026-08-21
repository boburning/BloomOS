#ifndef BLOOM_LIBRARY_QUERY_H
#define BLOOM_LIBRARY_QUERY_H

#include <sqlite3/sqlite3.h>
#include <stddef.h>

#define BLOOM_LIBRARY_QUERY_LIMIT_MAX 100
#define BLOOM_LIBRARY_TEXT_SIZE 512

typedef struct {
    char bloom_game_id[79];
    char system_id[64];
    char normalized_rom_path[BLOOM_LIBRARY_TEXT_SIZE];
    char display_title[BLOOM_LIBRARY_TEXT_SIZE];
    char image_path[BLOOM_LIBRARY_TEXT_SIZE];
    char launch_path[BLOOM_LIBRARY_TEXT_SIZE];
    sqlite3_int64 file_size;
    sqlite3_int64 file_mtime;
} BloomLibraryGame;

typedef struct {
    size_t count;
    int has_more;
    char next_cursor[79];
} BloomLibraryGamePage;

int bloom_library_query_games(sqlite3 *database, const char *system_id, const char *after_game_id,
                              size_t limit, BloomLibraryGame *games, size_t games_capacity,
                              BloomLibraryGamePage *page);

int bloom_library_query_recents(sqlite3 *database, const char *system_id, size_t limit,
                                BloomLibraryGame *games, size_t games_capacity, size_t *count);

#endif
