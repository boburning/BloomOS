#include "bloom_ui_input.h"

#include "system/keymap_sw.h"

BloomUiInput bloom_ui_input_from_sdl_key(SDLKey key)
{
    switch (key) {
    case SW_BTN_UP:
        return BLOOM_UI_INPUT_UP;
    case SW_BTN_DOWN:
        return BLOOM_UI_INPUT_DOWN;
    case SW_BTN_LEFT:
        return BLOOM_UI_INPUT_LEFT;
    case SW_BTN_RIGHT:
        return BLOOM_UI_INPUT_RIGHT;
    case SW_BTN_A:
        return BLOOM_UI_INPUT_CONFIRM;
    case SW_BTN_B:
        return BLOOM_UI_INPUT_BACK;
    case SW_BTN_X:
        return BLOOM_UI_INPUT_CONTEXT;
    case SW_BTN_Y:
        return BLOOM_UI_INPUT_FAVORITE;
    case SW_BTN_SELECT:
        return BLOOM_UI_INPUT_SEARCH;
    case SW_BTN_START:
        return BLOOM_UI_INPUT_QUICK_SETTINGS;
    case SW_BTN_MENU:
        return BLOOM_UI_INPUT_GAME_SWITCHER;
    case SW_BTN_L1:
        return BLOOM_UI_INPUT_PAGE_UP;
    case SW_BTN_R1:
        return BLOOM_UI_INPUT_PAGE_DOWN;
    default:
        return BLOOM_UI_INPUT_NONE;
    }
}
