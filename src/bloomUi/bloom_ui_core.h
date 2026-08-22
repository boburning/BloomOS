#ifndef BLOOM_UI_CORE_H
#define BLOOM_UI_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x;
    int y;
    int width;
    int height;
} BloomUiRect;

typedef struct {
    int viewport_width;
    int viewport_height;
    int margin;
    int row_height;
    size_t visible_rows;
    BloomUiRect header;
    BloomUiRect content;
    BloomUiRect footer;
} BloomUiLayout;

typedef enum {
    BLOOM_UI_INPUT_NONE = 0,
    BLOOM_UI_INPUT_UP,
    BLOOM_UI_INPUT_DOWN,
    BLOOM_UI_INPUT_LEFT,
    BLOOM_UI_INPUT_RIGHT,
    BLOOM_UI_INPUT_CONFIRM,
    BLOOM_UI_INPUT_BACK,
    BLOOM_UI_INPUT_CONTEXT,
    BLOOM_UI_INPUT_FAVORITE,
    BLOOM_UI_INPUT_SEARCH,
    BLOOM_UI_INPUT_QUICK_SETTINGS,
    BLOOM_UI_INPUT_GAME_SWITCHER,
    BLOOM_UI_INPUT_PAGE_UP,
    BLOOM_UI_INPUT_PAGE_DOWN,
} BloomUiInput;

typedef enum {
    BLOOM_UI_ACTION_NONE = 0,
    BLOOM_UI_ACTION_FOCUS_UP,
    BLOOM_UI_ACTION_FOCUS_DOWN,
    BLOOM_UI_ACTION_FOCUS_LEFT,
    BLOOM_UI_ACTION_FOCUS_RIGHT,
    BLOOM_UI_ACTION_CONFIRM,
    BLOOM_UI_ACTION_BACK,
    BLOOM_UI_ACTION_CONTEXT,
    BLOOM_UI_ACTION_TOGGLE_FAVORITE,
    BLOOM_UI_ACTION_SEARCH,
    BLOOM_UI_ACTION_QUICK_SETTINGS,
    BLOOM_UI_ACTION_GAME_SWITCHER,
    BLOOM_UI_ACTION_PAGE_UP,
    BLOOM_UI_ACTION_PAGE_DOWN,
} BloomUiAction;

typedef enum {
    BLOOM_UI_DESTINATION_ROOT = 0,
    BLOOM_UI_DESTINATION_GAMES,
    BLOOM_UI_DESTINATION_FAVORITES,
    BLOOM_UI_DESTINATION_RECENT,
    BLOOM_UI_DESTINATION_APPS,
    BLOOM_UI_DESTINATION_SETTINGS,
    BLOOM_UI_DESTINATION_COUNT,
} BloomUiDestination;

typedef struct {
    size_t item_count;
    size_t selected;
    size_t window_start;
} BloomUiFocus;

typedef struct {
    size_t button_count;
    size_t selected;
    size_t destructive;
} BloomUiDialogFocus;

typedef enum {
    BLOOM_UI_KEYBOARD_LOWER = 0,
    BLOOM_UI_KEYBOARD_UPPER,
    BLOOM_UI_KEYBOARD_SYMBOLS,
    BLOOM_UI_KEYBOARD_MODE_COUNT,
} BloomUiKeyboardMode;

typedef struct {
    BloomUiKeyboardMode mode;
    size_t row;
    size_t column;
} BloomUiKeyboardFocus;

int bloom_ui_layout_init(int width, int height, int large_text, BloomUiLayout *layout);
BloomUiAction bloom_ui_normalize_input(BloomUiInput input);
void bloom_ui_focus_init(BloomUiFocus *focus, size_t item_count);
void bloom_ui_focus_set_count(BloomUiFocus *focus, size_t item_count, size_t visible_rows);
int bloom_ui_focus_step(BloomUiFocus *focus, int direction, size_t visible_rows);
int bloom_ui_focus_page(BloomUiFocus *focus, int direction, size_t visible_rows);
int bloom_ui_dialog_init(BloomUiDialogFocus *dialog, size_t button_count, size_t default_button,
                         size_t destructive_button);
int bloom_ui_dialog_step(BloomUiDialogFocus *dialog, int direction);
void bloom_ui_keyboard_init(BloomUiKeyboardFocus *keyboard);
int bloom_ui_keyboard_move(BloomUiKeyboardFocus *keyboard, int horizontal, int vertical);
void bloom_ui_keyboard_cycle_mode(BloomUiKeyboardFocus *keyboard);
char bloom_ui_keyboard_character(const BloomUiKeyboardFocus *keyboard);
size_t bloom_ui_keyboard_row_length(BloomUiKeyboardMode mode, size_t row);
int bloom_ui_text_append(char *buffer, size_t capacity, char character);
int bloom_ui_text_backspace(char *buffer, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
