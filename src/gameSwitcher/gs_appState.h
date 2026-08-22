#ifndef GAME_SWITCHER_APP_STATE_H__
#define GAME_SWITCHER_APP_STATE_H__

#include <SDL/SDL.h>
#include <signal.h>

#include "gs_model.h"

#define VIEW_FULLSCREEN -1

typedef struct {
    List pop_menu_list;
    List quick_settings_list;
    bool quit;
    bool exit_to_menu;
    bool exit_to_settings;
    bool changed;
    bool current_game_changed;
    bool pop_menu_open;
    bool quick_settings_open;
    bool is_overlay;
    uint32_t acc_ticks;
    uint32_t last_ticks;
    uint32_t time_step;
    SDL_Surface *current_bg;
    bool first_render;
    int current_game;
} AppState;

static AppState appState = {
    .pop_menu_list = {{0}},
    .quick_settings_list = {{0}},
    .quit = false,
    .exit_to_menu = false,
    .exit_to_settings = false,
    .changed = true,
    .current_game_changed = true,
    .pop_menu_open = false,
    .quick_settings_open = false,
    .is_overlay = false,
    .acc_ticks = 0,
    .last_ticks = 0,
    .time_step = 1000 / 30,
    .current_bg = NULL,
    .first_render = true,
    .current_game = 0};

static void sigHandler(int sig)
{
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        appState.exit_to_menu = true;
        appState.quit = true;
        break;
    default:
        break;
    }
}

Game_s *currentGame(void)
{
    return &game_list[appState.current_game];
}

#endif // GAME_SWITCHER_APP_STATE_H__
