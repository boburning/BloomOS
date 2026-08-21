#include "bloom_shell_launch.h"

#include "../bloomUi/bloom_ui_core.h"
#include "../bloomUi/bloom_ui_input.h"
#include "../bloomUi/bloom_ui_renderer.h"

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <sqlite3/sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DATABASE_PATH "/mnt/SDCARD/.bloom/library/catalog.sqlite3"
#define REQUEST_PATH "/mnt/SDCARD/.tmp_update/bloom-shell-launch.json"
#define COMMAND_PATH "/mnt/SDCARD/.tmp_update/cmd_to_run.sh"
#define SESSION_REQUEST_PATH "/tmp/bloom-session/request.json"
#define SESSION_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-session"
#define GB_CORE "gambatte_libretro.so"
#define GAME_PAGE_SIZE 100
#define GAME_CAPACITY_MAX 4096
#define LAUNCH_READY_EXIT 20

static int load_games(BloomLibraryGame **games, size_t *game_count)
{
    sqlite3 *database = NULL;
    *games = calloc(GAME_CAPACITY_MAX, sizeof(**games));
    *game_count = 0;
    if (*games == NULL ||
        sqlite3_open_v2(DATABASE_PATH, &database, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK ||
        sqlite3_exec(database, "PRAGMA query_only=ON", NULL, NULL, NULL) != SQLITE_OK) {
        if (database != NULL)
            sqlite3_close(database);
        free(*games);
        *games = NULL;
        return -1;
    }
    char cursor[79] = {0};
    int result = SQLITE_OK;
    while (*game_count < GAME_CAPACITY_MAX) {
        size_t remaining = GAME_CAPACITY_MAX - *game_count;
        size_t limit = remaining < GAME_PAGE_SIZE ? remaining : GAME_PAGE_SIZE;
        BloomLibraryGamePage page = {0};
        result = bloom_library_query_games(database, "gb", cursor[0] == '\0' ? NULL : cursor,
                                           limit, *games + *game_count, remaining, &page);
        if (result != SQLITE_OK)
            break;
        *game_count += page.count;
        if (!page.has_more)
            break;
        snprintf(cursor, sizeof(cursor), "%s", page.next_cursor);
    }
    sqlite3_close(database);
    if (result != SQLITE_OK) {
        free(*games);
        *games = NULL;
        *game_count = 0;
        return -1;
    }
    return 0;
}

static void render_label(SDL_Surface *screen, TTF_Font *font, const char *label, int x, int y,
                         int maximum_width, SDL_Color color)
{
    char bounded[96];
    snprintf(bounded, sizeof(bounded), "%s", label);
    int width = 0;
    int height = 0;
    while (bounded[0] != '\0' &&
           (TTF_SizeUTF8(font, bounded, &width, &height) != 0 || width > maximum_width))
        bounded[strlen(bounded) - 1] = '\0';
    SDL_Surface *text = TTF_RenderUTF8_Blended(font, bounded, color);
    if (text != NULL) {
        SDL_Rect target = {(Sint16)x, (Sint16)y, 0, 0};
        SDL_BlitSurface(text, NULL, screen, &target);
        SDL_FreeSurface(text);
    }
}

static void draw(SDL_Surface *screen, const BloomUiLayout *layout, TTF_Font *font,
                 BloomUiDestination destination, const BloomUiFocus *focus,
                 const BloomLibraryGame *games)
{
    BloomUiScene scene = {
        .destination = destination,
        .item_count = destination == BLOOM_UI_DESTINATION_HOME ? 1 : focus->item_count,
        .selected = destination == BLOOM_UI_DESTINATION_HOME ? 0 : focus->selected,
        .window_start = destination == BLOOM_UI_DESTINATION_HOME ? 0 : focus->window_start,
        .healthy = 1,
    };
    bloom_ui_render_shell(screen, layout, &scene);
    SDL_Color cream = {243, 226, 189, 0};
    if (destination == BLOOM_UI_DESTINATION_HOME)
        render_label(screen, font, "Browse Game Boy", layout->content.x + 20,
                     layout->content.y + layout->row_height / 3, layout->content.width - 40, cream);
    else
        for (size_t row = 0; row < layout->visible_rows && focus->window_start + row < focus->item_count;
             ++row)
            render_label(screen, font, games[focus->window_start + row].display_title,
                         layout->content.x + 20,
                         layout->content.y + (int)row * layout->row_height + layout->row_height / 3,
                         layout->content.width - 40, cream);
    SDL_Flip(screen);
}

int main(int argc, char **argv)
{
    BloomLibraryGame *games = NULL;
    size_t game_count = 0;
    if (load_games(&games, &game_count) != 0)
        return 1;
    if (argc == 2 && strcmp(argv[1], "--probe") == 0) {
        printf("{\"schema\":1,\"service\":\"bloom-shell\",\"ready\":true,\"gb_games\":%zu}\n",
               game_count);
        free(games);
        return 0;
    }
    if (argc != 1) {
        free(games);
        return 2;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0 || TTF_Init() != 0) {
        free(games);
        return 1;
    }
    const SDL_VideoInfo *info = SDL_GetVideoInfo();
    int width = info != NULL && info->current_w >= 640 ? info->current_w : 640;
    int height = info != NULL && info->current_h >= 480 ? info->current_h : 480;
    BloomUiLayout layout;
    if (bloom_ui_layout_init(width, height, 0, &layout) != 0) {
        free(games);
        return 1;
    }
    SDL_Surface *screen = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE);
    TTF_Font *font = TTF_OpenFont("/customer/app/wqy-microhei.ttc", height >= 540 ? 26 : 22);
    if (screen == NULL || font == NULL) {
        free(games);
        return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(300, 70);

    BloomUiDestination destination = BLOOM_UI_DESTINATION_HOME;
    BloomUiFocus focus;
    bloom_ui_focus_init(&focus, game_count);
    draw(screen, &layout, font, destination, &focus, games);
    int running = 1;
    int exit_code = 0;
    while (running) {
        SDL_Event event;
        if (!SDL_WaitEvent(&event))
            continue;
        if (event.type == SDL_QUIT) {
            running = 0;
            continue;
        }
        if (event.type != SDL_KEYDOWN)
            continue;
        BloomUiAction action = bloom_ui_normalize_input(bloom_ui_input_from_sdl_key(event.key.keysym.sym));
        if (action == BLOOM_UI_ACTION_BACK) {
            if (destination == BLOOM_UI_DESTINATION_LIBRARY)
                destination = BLOOM_UI_DESTINATION_HOME;
            else
                running = 0;
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM && destination == BLOOM_UI_DESTINATION_HOME)
            destination = BLOOM_UI_DESTINATION_LIBRARY;
        else if (action == BLOOM_UI_ACTION_FOCUS_UP && destination == BLOOM_UI_DESTINATION_LIBRARY)
            bloom_ui_focus_step(&focus, -1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_DOWN && destination == BLOOM_UI_DESTINATION_LIBRARY)
            bloom_ui_focus_step(&focus, 1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_CONFIRM && destination == BLOOM_UI_DESTINATION_LIBRARY &&
                 focus.item_count > 0) {
            char error[256] = {0};
            if (bloom_shell_stage_game(&games[focus.selected], GB_CORE, REQUEST_PATH, COMMAND_PATH,
                                       SESSION_REQUEST_PATH, SESSION_BINARY, error, sizeof(error)) == 0) {
                exit_code = LAUNCH_READY_EXIT;
                running = 0;
            }
        }
        if (running)
            draw(screen, &layout, font, destination, &focus, games);
    }
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    free(games);
    return exit_code;
}
