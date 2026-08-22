#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <fcntl.h>
#include <libgen.h>
#include <linux/fb.h>
#include <pthread.h>
#include <sqlite3/sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "png/png.h"

#include "system/battery.h"
#include "system/lang.h"
#include "system/settings.h"
#include "system/state.h"
#include "theme/background.h"
#include "theme/sound.h"
#include "theme/theme.h"
#include "utils/config.h"
#include "utils/msleep.h"
#include "utils/surfaceSetAlpha.h"

#include "../bloomRa/bloom_ra_database.h"
#include "gameSwitcherAchievements.h"
#include "gs_appState.h"
#include "gs_history.h"
#include "gs_keystate.h"
#include "gs_overlay.h"
#include "gs_render.h"

#include "../bloomShell/bloom_shell_launch.h"

#define BLOOM_SHELL_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-shell"

int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "--bloom-recents-probe") == 0) {
        GameSwitcherLibraryRecent *recents = calloc(MAX_HISTORY, sizeof(*recents));
        size_t count = 0;
        int result = recents == NULL
                         ? -1
                         : gameswitcher_library_read_recents(BLOOM_LIBRARY_DATABASE_PATH,
                                                             MAX_HISTORY, recents, MAX_HISTORY, &count);
        free(recents);
        if (result != 0) {
            fputs("{\"schema\":1,\"service\":\"game-switcher\",\"ready\":false}\n", stdout);
            return EXIT_FAILURE;
        }
        printf("{\"schema\":1,\"service\":\"game-switcher\",\"ready\":true,"
               "\"canonical_recents\":%zu}\n",
               count);
        return EXIT_SUCCESS;
    }
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--overlay") != 0))
        return EXIT_FAILURE;
    appState.is_overlay = argc > 1 && strcmp(argv[1], "--overlay") == 0;

    log_setName("gameSwitcher");
    print_debug("\n\nDebug logging enabled");

    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    init(INIT_ALL);

    readFirstEntry();
    overlay_init();
    loadRomScreens();

    settings_load();
    lang_load();

    mkdirs("/mnt/SDCARD/.tmp_update/config/gameSwitcher");

    quickSettings_init();

    int battery_percentage = battery_getPercentage();

    appState.last_ticks = SDL_GetTicks();

    print_debug("gameSwitcher started\n");

    while (!appState.quit) {
        uint32_t ticks = SDL_GetTicks();
        appState.acc_ticks += ticks - appState.last_ticks;
        appState.last_ticks = ticks;

        handleKeystate(&appState);

        if (battery_hasChanged(ticks, &battery_percentage))
            appState.changed = true;

        if (appState.acc_ticks >= appState.time_step) {
            appState.acc_ticks -= appState.time_step;

            if (!appState.changed)
                continue;

            Game_s *game = &game_list[appState.current_game];
            processItem(game);

            if (appState.changed) {
                SDL_FillRect(screen, NULL, 0);

                if (game_list_len == 0) {
                    appState.current_bg = NULL;
                    bloomGsRenderEmpty();
                }
                else {
                    appState.current_bg = loadRomScreen(appState.current_game);

                    if (appState.current_bg != NULL) {
                        renderCentered(appState.current_bg, VIEW_FULLSCREEN, NULL, NULL);
                    }
                }
            }

            if (game_list_len > 0 && !appState.pop_menu_open && !appState.quick_settings_open)
                bloomGsRenderGameTitle(game->shortname, appState.current_game + 1, game_list_len);

            bloomGsRenderFooter(game_list_len > 0);
            bloomGsRenderHeader(battery_percentage);

            if (!appState.first_render) {
                renderPopMenu(&appState);
                quickSettings_render(&appState);
            }

            render();

            if (appState.first_render) {
                appState.first_render = false;
                readHistory();
                loadRomScreens();
            }
            else {
                appState.changed = false;
                appState.current_game_changed = false;
            }
        }
    }

    if (appState.exit_to_menu) {
        char error[256] = {0};
        print_debug("Returning to Bloom Shell");
        remove("/mnt/SDCARD/.tmp_update/.runGameSwitcher");
        if (bloom_shell_stage_executable(BLOOM_SHELL_BINARY, CMD_TO_RUN_PATH, error,
                                         sizeof(error)) != 0)
            printf_debug("Unable to stage Bloom Shell: %s\n", error);
        overlay_exit();
        SDL_FillRect(screen, NULL, 0);
        render();
    }
    else if (currentGame()->is_running) {
        if (appState.current_bg != NULL) {
            SDL_FillRect(screen, NULL, 0);
            renderCentered(appState.current_bg, VIEW_FULLSCREEN, NULL, NULL);
        }
        overlay_resume();
    }
    else {
        printf_debug("Resuming game - current_game : %i - index: %i\n", appState.current_game, game_list[appState.current_game].index);
        Game_s *game = &game_list[appState.current_game];
        if (game->recentItem.bloom_owned) {
            GameSwitcherLibraryRecent recent = {0};
            char error[256] = {0};
            snprintf(recent.game_id, sizeof(recent.game_id), "%s", game->game_id);
            snprintf(recent.system_id, sizeof(recent.system_id), "%s", game->recentItem.system_id);
            snprintf(recent.rom_path, sizeof(recent.rom_path), "%s", game->recentItem.rompath);
            snprintf(recent.launcher, sizeof(recent.launcher), "%s", game->recentItem.launch);
            if (gameswitcher_library_stage_recent(BLOOM_LIBRARY_DATABASE_PATH, &recent,
                                                  BLOOM_GAMESWITCHER_REQUEST_PATH,
                                                  CMD_TO_RUN_PATH, error, sizeof(error)) != 0)
                printf_debug("Canonical resume failed: %s\n", error);
        }
        else {
            resumeGame(game->index);
        }
        overlay_exit();
        render_showFullscreenMessage("LOADING", true);
    }

#ifndef PLATFORM_MIYOOMINI
    msleep(200);
#endif

    popMenu_destroy();
    quickSettings_destroy();

    resources_free();

    freeRomScreens();
    ra_freeHistory();
    gameswitcher_achievements_close();
    bloomGsFontsDestroy();

    deinit();

    return EXIT_SUCCESS;
}
