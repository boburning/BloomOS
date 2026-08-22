#include "bloom_shell_search.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int contains_case_insensitive(const char *text, const char *query)
{
    if (query[0] == '\0')
        return 1;
    for (; *text != '\0'; ++text) {
        const char *candidate = text;
        const char *needle = query;
        while (*candidate != '\0' && *needle != '\0' &&
               tolower((unsigned char)*candidate) == tolower((unsigned char)*needle)) {
            ++candidate;
            ++needle;
        }
        if (*needle == '\0')
            return 1;
    }
    return 0;
}

int bloom_shell_search_init(BloomShellSearch *search, size_t capacity)
{
    if (search == NULL || capacity == 0)
        return -1;
    memset(search, 0, sizeof(*search));
    search->results = calloc(capacity, sizeof(*search->results));
    if (search->results == NULL)
        return -1;
    search->capacity = capacity;
    bloom_ui_keyboard_init(&search->keyboard);
    bloom_ui_focus_init(&search->focus, 0);
    return 0;
}

void bloom_shell_search_clear(BloomShellSearch *search)
{
    if (search == NULL)
        return;
    search->open = 0;
    search->active = 0;
    search->query[0] = '\0';
    bloom_ui_keyboard_init(&search->keyboard);
    bloom_ui_focus_init(&search->focus, 0);
}

void bloom_shell_search_destroy(BloomShellSearch *search)
{
    if (search == NULL)
        return;
    free(search->results);
    memset(search, 0, sizeof(*search));
}

int bloom_shell_search_rebuild(BloomShellSearch *search, const BloomLibraryGame *games,
                               size_t game_count, size_t visible_rows)
{
    if (search == NULL || (games == NULL && game_count > 0))
        return -1;
    size_t count = 0;
    for (size_t index = 0; index < game_count && count < search->capacity; ++index)
        if (contains_case_insensitive(games[index].display_title, search->query))
            search->results[count++] = &games[index];
    search->active = search->query[0] != '\0';
    bloom_ui_focus_set_count(&search->focus, count, visible_rows);
    return 0;
}

int bloom_shell_search_append(BloomShellSearch *search, const BloomLibraryGame *games,
                              size_t game_count, size_t visible_rows)
{
    if (search == NULL ||
        bloom_ui_text_append(search->query, sizeof(search->query),
                             bloom_ui_keyboard_character(&search->keyboard)) != 0)
        return -1;
    return bloom_shell_search_rebuild(search, games, game_count, visible_rows);
}

int bloom_shell_search_backspace(BloomShellSearch *search, const BloomLibraryGame *games,
                                 size_t game_count, size_t visible_rows)
{
    if (search == NULL || bloom_ui_text_backspace(search->query, sizeof(search->query)) != 0)
        return -1;
    return bloom_shell_search_rebuild(search, games, game_count, visible_rows);
}
