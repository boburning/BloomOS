#ifndef GAME_SWITCHER_KEY_STATE_H__
#define GAME_SWITCHER_KEY_STATE_H__

#include "components/list.h"
#include "system/keymap_sw.h"
#include "theme/render/dialog.h"
#include "utils/keystate.h"

#include "gs_appState.h"
#include "gs_model.h"
#include "gs_popMenu.h"
#include "gs_quickSettings.h"
#include "gs_romscreen.h"

typedef struct {
    KeyState keystate[320];
    bool btn_a_pressed;
    SDLKey changed_key;
} AppKeyState_s;

static AppKeyState_s _gs_keystate = {
    .keystate = {RELEASED},
    .btn_a_pressed = false,
    .changed_key = SDLK_UNKNOWN,
};

void removeCurrentItem()
{
    Game_s *game = &game_list[appState.current_game];

    printf_debug("removing: %s\n", game->name);
    printf_debug("linenumber: %i\n", game->recentItem.lineNo);

    if (game->recentItem.bloom_owned &&
        gameswitcher_library_remove_recent(BLOOM_LIBRARY_DATABASE_PATH, game->game_id) != 0)
        return;

    if (game->romScreen != NULL) {
        SDL_FreeSurface(game->romScreen);
        game->romScreen = NULL;
    }

    if (!game->recentItem.bloom_owned)
        file_delete_line(getMiyooRecentFilePath(), game->recentItem.lineNo);

    if (!game->recentItem.bloom_owned && strlen(game->recentItem.imgpath) > 0 &&
        is_file(game->recentItem.imgpath)) {
        if (strncmp(game->recentItem.imgpath, ROM_SCREENS_DIR, strlen(ROM_SCREENS_DIR)) == 0) {
            remove(game->recentItem.imgpath);
        }
    }

    // Copy next element value to current element
    for (int i = appState.current_game; i < game_list_len - 1; i++) {
        game_list[i] = game_list[i + 1];
        if (!game_list[i].recentItem.bloom_owned)
            game_list[i].recentItem.lineNo -= 1;
        game_list[i].index -= 1;
    }

    game_list_len--;
}

int checkQuitAction(void)
{
    FILE *fp;
    char prev_state[10];
    file_get(fp, "/tmp/prev_state", "%s", prev_state);
    if (strncmp(prev_state, "mainui", 6) == 0)
        return 1;
    return 0;
}

void action_confirmRemove(void *_)
{
    AppState *state = &appState;
    bloomGsRenderDialog("Remove from Recent", "Remove this game from your recent history?", 1);
    render();

    KeyState *keystate = _gs_keystate.keystate;

    while (!state->quit) {
        if (_updateKeystate(keystate, &state->quit, true, NULL)) {
            if (keystate[SW_BTN_A] == PRESSED) {
                removeCurrentItem();
                state->pop_menu_open = false;
                popMenu_destroy();
                if (game_list_len == 0) {
                    state->exit_to_menu = true;
                    state->quit = true;
                    break;
                }
                if (state->current_game >= game_list_len)
                    state->current_game = game_list_len - 1;
                state->current_game_changed = true;
                loadRomScreen(state->current_game);
                state->changed = true;
                break;
            }
            if (keystate[SW_BTN_B] == PRESSED) {
                state->changed = true;
                break;
            }
        }
    }
}

void handleUpdateKeystateMain(AppState *state)
{
    KeyState *keystate = _gs_keystate.keystate;

    if (keystate[SW_BTN_RIGHT] >= PRESSED) {
        if (state->current_game < game_list_len - 1) {
            state->current_game++;
            state->current_game_changed = true;
            state->changed = true;
        }
    }

    if (keystate[SW_BTN_LEFT] >= PRESSED) {
        if (state->current_game > 0) {
            state->current_game--;
            state->current_game_changed = true;
            state->changed = true;
        }
    }

    if (keystate[SW_BTN_A] == PRESSED && game_list_len > 0) {
        _gs_keystate.btn_a_pressed = true;
    }
    else if (keystate[SW_BTN_A] == RELEASED && _gs_keystate.btn_a_pressed) {
        _gs_keystate.btn_a_pressed = false;
        state->quit = true;
        return;
    }

    if (keystate[SW_BTN_B] == PRESSED) {
        state->exit_to_menu = true;
        state->quit = true;
        return;
    }

    if (state->current_game_changed) {
        popMenu_destroy();
    }
}

void handleUpdateKeystatePopMenu(AppState *state)
{
    KeyState *keystate = _gs_keystate.keystate;
    ListItem *item = list_currentItem(&state->pop_menu_list);

    if (keystate[SW_BTN_B] == PRESSED) {
        state->pop_menu_open = false;
        state->changed = true;
    }

    if (keystate[SW_BTN_A] == PRESSED) {
        _gs_keystate.btn_a_pressed = true;
    }
    else if (keystate[SW_BTN_A] == RELEASED && _gs_keystate.btn_a_pressed) {
        _gs_keystate.btn_a_pressed = false;
        list_activateItem(&state->pop_menu_list);
    }

    if (keystate[SW_BTN_DOWN] >= PRESSED) {
        if (list_keyDown(&state->pop_menu_list, keystate[SW_BTN_DOWN] == REPEATING))
            state->changed = true;
    }
    else if (keystate[SW_BTN_UP] >= PRESSED) {
        if (list_keyUp(&state->pop_menu_list, keystate[SW_BTN_UP] == REPEATING))
            state->changed = true;
    }

    if (item != NULL && item->action_id == POP_MENU_ACTION_LOAD) {
        if (keystate[SW_BTN_LEFT] >= PRESSED) {
            if (g_save_state_info.selected_slot > 0) {
                g_save_state_info.selected_slot--;
                setLoadPreview();
                state->changed = true;
            }
        }
        else if (keystate[SW_BTN_RIGHT] >= PRESSED) {
            if (g_save_state_info.selected_slot < g_save_state_info.slot_count - 1) {
                g_save_state_info.selected_slot++;
                setLoadPreview();
                state->changed = true;
            }
        }
        else if (keystate[SW_BTN_X] == PRESSED) {
            popMenu_deleteSaveState();
            keystate[SW_BTN_X] = RELEASED;
        }
    }
}

void handleKeystate(AppState *state)
{
    KeyState *keystate = _gs_keystate.keystate;

    if (_updateKeystate(keystate, &state->quit, true, &_gs_keystate.changed_key)) {
        if (keystate[SW_BTN_MENU] == PRESSED) {
            if (!state->is_overlay || game_list_len == 0)
                state->exit_to_menu = true;
            state->quit = true;
            return;
        }

        if (keystate[SW_BTN_START] == PRESSED) {
            if (state->quick_settings_open)
                quickSettings_close();
            else {
                state->pop_menu_open = false;
                popMenu_destroy();
                quickSettings_open();
            }
            return;
        }

        if (state->quick_settings_open)
            quickSettings_handle(keystate);
        else if (keystate[SW_BTN_X] == PRESSED && game_list_len != 0) {
            state->pop_menu_open = !state->pop_menu_open;
            state->changed = true;
        }
        else if (state->pop_menu_open)
            handleUpdateKeystatePopMenu(state);
        else
            handleUpdateKeystateMain(state);
    }
}

#endif // GAME_SWITCHER_KEY_STATE_H__
