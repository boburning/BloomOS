#ifndef BLOOM_SHELL_SEARCH_H
#define BLOOM_SHELL_SEARCH_H

#include "../bloomLibrary/bloom_library_query.h"
#include "../bloomUi/bloom_ui_core.h"

#define BLOOM_SHELL_SEARCH_QUERY_SIZE 64

typedef struct {
    int open;
    int active;
    char query[BLOOM_SHELL_SEARCH_QUERY_SIZE];
    BloomUiKeyboardFocus keyboard;
    BloomUiFocus focus;
    const BloomLibraryGame **results;
    size_t capacity;
} BloomShellSearch;

int bloom_shell_search_init(BloomShellSearch *search, size_t capacity);
void bloom_shell_search_clear(BloomShellSearch *search);
void bloom_shell_search_destroy(BloomShellSearch *search);
int bloom_shell_search_rebuild(BloomShellSearch *search, const BloomLibraryGame *games,
                               size_t game_count, size_t visible_rows);
int bloom_shell_search_append(BloomShellSearch *search, const BloomLibraryGame *games,
                              size_t game_count, size_t visible_rows);
int bloom_shell_search_backspace(BloomShellSearch *search, const BloomLibraryGame *games,
                                 size_t game_count, size_t visible_rows);

#endif
