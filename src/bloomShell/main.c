#include "bloom_shell_launch.h"
#include "bloom_shell_status.h"

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
#define BLOOMCTL_BINARY "/mnt/SDCARD/.tmp_update/bin/bloomctl"
#define GB_CORE "gambatte_libretro.so"
#define GAME_PAGE_SIZE 100
#define GAME_CAPACITY_MAX 4096
#define FAVORITES_CAPACITY_MAX 100
#define LAUNCH_READY_EXIT 20

static int load_games(BloomLibraryGame **games, size_t *game_count, BloomLibraryGame *recent,
                      int *has_recent, BloomLibraryGame *favorites, size_t *favorite_count)
{
    sqlite3 *database = NULL;
    *games = calloc(GAME_CAPACITY_MAX, sizeof(**games));
    *game_count = 0;
    *has_recent = 0;
    *favorite_count = 0;
    if (*games == NULL ||
        sqlite3_open_v2(DATABASE_PATH, &database, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK ||
        sqlite3_exec(database, "PRAGMA query_only=ON", NULL, NULL, NULL) != SQLITE_OK) {
        if (database != NULL)
            sqlite3_close(database);
        free(*games);
        *games = NULL;
        return -1;
    }
    size_t recent_count = 0;
    int result = bloom_library_query_recents(database, "gb", 1, recent, 1, &recent_count);
    if (result == SQLITE_OK)
        *has_recent = recent_count == 1;
    if (result == SQLITE_OK)
        result = bloom_library_query_favorites(database, "gb", FAVORITES_CAPACITY_MAX, favorites,
                                               FAVORITES_CAPACITY_MAX, favorite_count);
    char cursor[79] = {0};
    while (result == SQLITE_OK && *game_count < GAME_CAPACITY_MAX) {
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

static void draw(SDL_Surface *screen, SDL_Surface *video, const BloomUiLayout *layout, TTF_Font *font,
                 BloomUiDestination destination, const BloomUiFocus *library_focus,
                 const BloomUiFocus *collections_focus, const BloomLibraryGame *games,
                 const BloomLibraryGame *favorites, const BloomLibraryGame *recent, int has_recent,
                 size_t home_selected, const BloomUiFocus *settings_focus,
                 const BloomShellStatus *status)
{
    const BloomUiFocus *focus = destination == BLOOM_UI_DESTINATION_COLLECTIONS
                                    ? collections_focus
                                    : library_focus;
    const BloomLibraryGame *rows = destination == BLOOM_UI_DESTINATION_COLLECTIONS ? favorites : games;
    size_t item_count = destination == BLOOM_UI_DESTINATION_HOME   ? (has_recent ? 2 : 1)
                        : destination == BLOOM_UI_DESTINATION_APPS ? 1
                        : destination == BLOOM_UI_DESTINATION_SETTINGS
                            ? settings_focus->item_count
                            : focus->item_count;
    size_t selected = destination == BLOOM_UI_DESTINATION_HOME   ? home_selected
                      : destination == BLOOM_UI_DESTINATION_APPS ? 0
                      : destination == BLOOM_UI_DESTINATION_SETTINGS
                          ? settings_focus->selected
                          : focus->selected;
    size_t window_start = destination == BLOOM_UI_DESTINATION_HOME ||
                                  destination == BLOOM_UI_DESTINATION_APPS
                              ? 0
                          : destination == BLOOM_UI_DESTINATION_SETTINGS
                              ? settings_focus->window_start
                              : focus->window_start;
    BloomUiScene scene = {
        .destination = destination,
        .item_count = item_count,
        .selected = selected,
        .window_start = window_start,
        .healthy = status->ready && status->healthy,
    };
    bloom_ui_render_shell(screen, layout, &scene);
    SDL_Color cream = {243, 226, 189, 0};
    if (destination == BLOOM_UI_DESTINATION_HOME) {
        char label[640];
        if (has_recent) {
            snprintf(label, sizeof(label), "Resume %s", recent->display_title);
            render_label(screen, font, label, layout->content.x + 20,
                         layout->content.y + layout->row_height / 3,
                         layout->content.width - 40, cream);
            snprintf(label, sizeof(label), "Browse Game Boy");
            render_label(screen, font, label, layout->content.x + 20,
                         layout->content.y + layout->row_height + layout->row_height / 3,
                         layout->content.width - 40, cream);
        }
        else {
            snprintf(label, sizeof(label), "Browse Game Boy");
            render_label(screen, font, label, layout->content.x + 20,
                         layout->content.y + layout->row_height / 3,
                         layout->content.width - 40, cream);
        }
    }
    else if (destination == BLOOM_UI_DESTINATION_APPS) {
        render_label(screen, font, "Applications migration in progress", layout->content.x + 20,
                     layout->content.y + layout->row_height / 3, layout->content.width - 40,
                     cream);
    }
    else if (destination == BLOOM_UI_DESTINATION_SETTINGS) {
        for (size_t row = 0; row < settings_focus->item_count; ++row) {
            char label[96];
            if (bloom_shell_status_label(status, row, label, sizeof(label)) == 0)
                render_label(screen, font, label, layout->content.x + 20,
                             layout->content.y + (int)row * layout->row_height +
                                 layout->row_height / 3,
                             layout->content.width - 40, cream);
        }
    }
    else
        for (size_t row = 0; row < layout->visible_rows && focus->window_start + row < focus->item_count;
             ++row)
            render_label(screen, font, rows[focus->window_start + row].display_title,
                         layout->content.x + 20,
                         layout->content.y + (int)row * layout->row_height + layout->row_height / 3,
                         layout->content.width - 40, cream);
#ifdef PLATFORM_MIYOOMINI
    bloom_ui_rotate_180(screen);
    SDL_BlitSurface(screen, NULL, video, NULL);
    if (bloom_ui_publish_framebuffer_pages(video, "/dev/fb0", 3) != 0)
        SDL_Flip(video);
#else
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
#endif
}

int main(int argc, char **argv)
{
    BloomShellStatus status = {0};
    bloom_shell_status_load(BLOOMCTL_BINARY, &status);
    BloomLibraryGame *games = NULL;
    BloomLibraryGame recent = {0};
    BloomLibraryGame favorites[FAVORITES_CAPACITY_MAX] = {0};
    size_t game_count = 0;
    size_t favorite_count = 0;
    int has_recent = 0;
    if (load_games(&games, &game_count, &recent, &has_recent, favorites, &favorite_count) != 0)
        return 1;
    if (argc == 2 && strcmp(argv[1], "--probe") == 0) {
        printf("{\"schema\":1,\"service\":\"bloom-shell\",\"ready\":true,\"gb_games\":%zu,"
               "\"gb_recent\":%s,\"gb_favorites\":%zu,\"health_ready\":%s,\"healthy\":%s}\n",
               game_count, has_recent ? "true" : "false", favorite_count,
               status.ready ? "true" : "false", status.healthy ? "true" : "false");
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
    SDL_Surface *video = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE);
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, 0, 0, 0, 0);
    TTF_Font *font = TTF_OpenFont("/customer/app/wqy-microhei.ttc", height >= 540 ? 26 : 22);
    if (video == NULL || screen == NULL || font == NULL) {
        if (screen != NULL)
            SDL_FreeSurface(screen);
        free(games);
        return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(300, 70);

    BloomUiDestination destination = BLOOM_UI_DESTINATION_HOME;
    size_t home_selected = 0;
    BloomUiFocus library_focus;
    BloomUiFocus collections_focus;
    BloomUiFocus settings_focus;
    bloom_ui_focus_init(&library_focus, game_count);
    bloom_ui_focus_init(&collections_focus, favorite_count);
    bloom_ui_focus_init(&settings_focus, 3);
    draw(screen, video, &layout, font, destination, &library_focus, &collections_focus, games,
         favorites, &recent, has_recent, home_selected, &settings_focus, &status);
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
            if (destination != BLOOM_UI_DESTINATION_HOME)
                destination = BLOOM_UI_DESTINATION_HOME;
            else
                running = 0;
        }
        else if (action == BLOOM_UI_ACTION_NEXT_DESTINATION)
            destination = bloom_ui_destination_step(destination, 1);
        else if (action == BLOOM_UI_ACTION_PREVIOUS_DESTINATION)
            destination = bloom_ui_destination_step(destination, -1);
        else if ((action == BLOOM_UI_ACTION_FOCUS_UP || action == BLOOM_UI_ACTION_FOCUS_DOWN) &&
                 destination == BLOOM_UI_DESTINATION_HOME && has_recent)
            home_selected = home_selected == 0 ? 1 : 0;
        else if (action == BLOOM_UI_ACTION_CONFIRM && destination == BLOOM_UI_DESTINATION_HOME) {
            if (has_recent && home_selected == 0) {
                char error[256] = {0};
                if (bloom_shell_stage_game(&recent, GB_CORE, REQUEST_PATH, COMMAND_PATH,
                                           SESSION_REQUEST_PATH, SESSION_BINARY, error,
                                           sizeof(error)) == 0) {
                    exit_code = LAUNCH_READY_EXIT;
                    running = 0;
                }
            }
            else
                destination = BLOOM_UI_DESTINATION_LIBRARY;
        }
        else if (action == BLOOM_UI_ACTION_FOCUS_UP && destination == BLOOM_UI_DESTINATION_LIBRARY)
            bloom_ui_focus_step(&library_focus, -1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_DOWN && destination == BLOOM_UI_DESTINATION_LIBRARY)
            bloom_ui_focus_step(&library_focus, 1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_UP &&
                 destination == BLOOM_UI_DESTINATION_COLLECTIONS)
            bloom_ui_focus_step(&collections_focus, -1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_DOWN &&
                 destination == BLOOM_UI_DESTINATION_COLLECTIONS)
            bloom_ui_focus_step(&collections_focus, 1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_UP &&
                 destination == BLOOM_UI_DESTINATION_SETTINGS)
            bloom_ui_focus_step(&settings_focus, -1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_DOWN &&
                 destination == BLOOM_UI_DESTINATION_SETTINGS)
            bloom_ui_focus_step(&settings_focus, 1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_CONFIRM && destination == BLOOM_UI_DESTINATION_LIBRARY &&
                 library_focus.item_count > 0) {
            char error[256] = {0};
            if (bloom_shell_stage_game(&games[library_focus.selected], GB_CORE, REQUEST_PATH,
                                       COMMAND_PATH, SESSION_REQUEST_PATH, SESSION_BINARY, error,
                                       sizeof(error)) == 0) {
                exit_code = LAUNCH_READY_EXIT;
                running = 0;
            }
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM &&
                 destination == BLOOM_UI_DESTINATION_COLLECTIONS &&
                 collections_focus.item_count > 0) {
            char error[256] = {0};
            if (bloom_shell_stage_game(&favorites[collections_focus.selected], GB_CORE, REQUEST_PATH,
                                       COMMAND_PATH, SESSION_REQUEST_PATH, SESSION_BINARY, error,
                                       sizeof(error)) == 0) {
                exit_code = LAUNCH_READY_EXIT;
                running = 0;
            }
        }
        if (running)
            draw(screen, video, &layout, font, destination, &library_focus, &collections_focus,
                 games, favorites, &recent, has_recent, home_selected, &settings_focus, &status);
    }
    TTF_CloseFont(font);
    SDL_FreeSurface(screen);
    TTF_Quit();
    SDL_Quit();
    free(games);
    return exit_code;
}
