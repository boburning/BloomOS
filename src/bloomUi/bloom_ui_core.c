#include "bloom_ui_core.h"

#include <string.h>

#define BLOOM_UI_MIN_WIDTH 640
#define BLOOM_UI_MIN_HEIGHT 480
#define BLOOM_UI_MAX_DIMENSION 2048

static const char *keyboard_rows[BLOOM_UI_KEYBOARD_MODE_COUNT][4] = {
    {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"},
    {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"},
    {"!@#$%^&*()", "-_+=[]{}", ";:'\",.<>/?", "\\|`~"},
};

static size_t normalized_visible_rows(size_t visible_rows)
{
    return visible_rows > 0 ? visible_rows : 1;
}

static void reveal_selection(BloomUiFocus *focus, size_t visible_rows)
{
    visible_rows = normalized_visible_rows(visible_rows);
    if (focus->item_count == 0) {
        focus->selected = 0;
        focus->window_start = 0;
        return;
    }
    if (focus->selected >= focus->item_count) {
        focus->selected = focus->item_count - 1;
    }
    if (focus->selected < focus->window_start) {
        focus->window_start = focus->selected;
    }
    else if (focus->selected - focus->window_start >= visible_rows) {
        focus->window_start = focus->selected - visible_rows + 1;
    }
    size_t maximum_start = focus->item_count > visible_rows ? focus->item_count - visible_rows : 0;
    if (focus->window_start > maximum_start) {
        focus->window_start = maximum_start;
    }
}

int bloom_ui_layout_init(int width, int height, int large_text, BloomUiLayout *layout)
{
    if (layout == NULL || width < BLOOM_UI_MIN_WIDTH || height < BLOOM_UI_MIN_HEIGHT ||
        width > BLOOM_UI_MAX_DIMENSION || height > BLOOM_UI_MAX_DIMENSION || (large_text != 0 && large_text != 1)) {
        return -1;
    }

    memset(layout, 0, sizeof(*layout));
    layout->viewport_width = width;
    layout->viewport_height = height;
    layout->margin = width >= 720 ? 28 : 24;
    layout->header = (BloomUiRect){0, 0, width, height >= 540 ? 64 : 56};
    layout->footer = (BloomUiRect){0, height - (height >= 540 ? 48 : 40), width, height >= 540 ? 48 : 40};
    layout->content = (BloomUiRect){layout->margin, layout->header.height,
                                    width - layout->margin * 2,
                                    layout->footer.y - layout->header.height};
    layout->row_height = large_text ? (height >= 540 ? 76 : 68) : (height >= 540 ? 64 : 56);
    layout->visible_rows = (size_t)(layout->content.height / layout->row_height);
    if (layout->content.width <= 0 || layout->content.height <= 0 || layout->visible_rows == 0) {
        memset(layout, 0, sizeof(*layout));
        return -1;
    }
    return 0;
}

BloomUiAction bloom_ui_normalize_input(BloomUiInput input)
{
    static const BloomUiAction actions[] = {
        BLOOM_UI_ACTION_NONE,
        BLOOM_UI_ACTION_FOCUS_UP,
        BLOOM_UI_ACTION_FOCUS_DOWN,
        BLOOM_UI_ACTION_FOCUS_LEFT,
        BLOOM_UI_ACTION_FOCUS_RIGHT,
        BLOOM_UI_ACTION_CONFIRM,
        BLOOM_UI_ACTION_BACK,
        BLOOM_UI_ACTION_PREVIOUS_DESTINATION,
        BLOOM_UI_ACTION_NEXT_DESTINATION,
        BLOOM_UI_ACTION_CONTEXT,
        BLOOM_UI_ACTION_TOGGLE_FAVORITE,
        BLOOM_UI_ACTION_SEARCH,
        BLOOM_UI_ACTION_QUICK_SETTINGS,
        BLOOM_UI_ACTION_GAME_SWITCHER,
    };
    if (input < BLOOM_UI_INPUT_NONE || input > BLOOM_UI_INPUT_GAME_SWITCHER) {
        return BLOOM_UI_ACTION_NONE;
    }
    return actions[input];
}

BloomUiDestination bloom_ui_destination_step(BloomUiDestination current, int direction)
{
    if (current < BLOOM_UI_DESTINATION_HOME || current >= BLOOM_UI_DESTINATION_COUNT) {
        return BLOOM_UI_DESTINATION_HOME;
    }
    if (direction < 0) {
        return current == BLOOM_UI_DESTINATION_HOME ? BLOOM_UI_DESTINATION_SETTINGS
                                                    : (BloomUiDestination)(current - 1);
    }
    if (direction > 0) {
        return current == BLOOM_UI_DESTINATION_SETTINGS ? BLOOM_UI_DESTINATION_HOME
                                                        : (BloomUiDestination)(current + 1);
    }
    return current;
}

void bloom_ui_focus_init(BloomUiFocus *focus, size_t item_count)
{
    if (focus == NULL) {
        return;
    }
    focus->item_count = item_count;
    focus->selected = 0;
    focus->window_start = 0;
}

void bloom_ui_focus_set_count(BloomUiFocus *focus, size_t item_count, size_t visible_rows)
{
    if (focus == NULL) {
        return;
    }
    focus->item_count = item_count;
    reveal_selection(focus, visible_rows);
}

int bloom_ui_focus_step(BloomUiFocus *focus, int direction, size_t visible_rows)
{
    if (focus == NULL || focus->item_count == 0 || (direction != -1 && direction != 1)) {
        return 0;
    }
    size_t previous = focus->selected;
    if (direction < 0 && focus->selected > 0) {
        focus->selected--;
    }
    else if (direction > 0 && focus->selected + 1 < focus->item_count) {
        focus->selected++;
    }
    reveal_selection(focus, visible_rows);
    return previous != focus->selected;
}

int bloom_ui_dialog_init(BloomUiDialogFocus *dialog, size_t button_count, size_t default_button,
                         size_t destructive_button)
{
    if (dialog == NULL || button_count == 0 || button_count > 3 || default_button >= button_count ||
        (destructive_button != SIZE_MAX && destructive_button >= button_count)) {
        return -1;
    }
    dialog->button_count = button_count;
    dialog->selected = default_button;
    dialog->destructive = destructive_button;
    return 0;
}

int bloom_ui_dialog_step(BloomUiDialogFocus *dialog, int direction)
{
    if (dialog == NULL || dialog->button_count == 0 || (direction != -1 && direction != 1)) {
        return 0;
    }
    size_t previous = dialog->selected;
    if (direction < 0 && dialog->selected > 0) {
        dialog->selected--;
    }
    else if (direction > 0 && dialog->selected + 1 < dialog->button_count) {
        dialog->selected++;
    }
    return previous != dialog->selected;
}

static size_t keyboard_row_length(const BloomUiKeyboardFocus *keyboard)
{
    return strlen(keyboard_rows[keyboard->mode][keyboard->row]);
}

void bloom_ui_keyboard_init(BloomUiKeyboardFocus *keyboard)
{
    if (keyboard == NULL) {
        return;
    }
    keyboard->mode = BLOOM_UI_KEYBOARD_LOWER;
    keyboard->row = 0;
    keyboard->column = 0;
}

int bloom_ui_keyboard_move(BloomUiKeyboardFocus *keyboard, int horizontal, int vertical)
{
    if (keyboard == NULL || keyboard->mode < BLOOM_UI_KEYBOARD_LOWER ||
        keyboard->mode >= BLOOM_UI_KEYBOARD_MODE_COUNT || keyboard->row >= 4 ||
        (horizontal < -1 || horizontal > 1) || (vertical < -1 || vertical > 1) ||
        (horizontal != 0 && vertical != 0)) {
        return 0;
    }
    size_t previous_row = keyboard->row;
    size_t previous_column = keyboard->column;
    if (horizontal < 0 && keyboard->column > 0) {
        keyboard->column--;
    }
    else if (horizontal > 0 && keyboard->column + 1 < keyboard_row_length(keyboard)) {
        keyboard->column++;
    }
    else if (vertical < 0 && keyboard->row > 0) {
        keyboard->row--;
    }
    else if (vertical > 0 && keyboard->row < 3) {
        keyboard->row++;
    }
    size_t row_length = keyboard_row_length(keyboard);
    if (keyboard->column >= row_length) {
        keyboard->column = row_length - 1;
    }
    return previous_row != keyboard->row || previous_column != keyboard->column;
}

void bloom_ui_keyboard_cycle_mode(BloomUiKeyboardFocus *keyboard)
{
    if (keyboard == NULL || keyboard->mode < BLOOM_UI_KEYBOARD_LOWER ||
        keyboard->mode >= BLOOM_UI_KEYBOARD_MODE_COUNT || keyboard->row >= 4) {
        return;
    }
    keyboard->mode = (BloomUiKeyboardMode)((keyboard->mode + 1) % BLOOM_UI_KEYBOARD_MODE_COUNT);
    size_t row_length = keyboard_row_length(keyboard);
    if (keyboard->column >= row_length) {
        keyboard->column = row_length - 1;
    }
}

char bloom_ui_keyboard_character(const BloomUiKeyboardFocus *keyboard)
{
    if (keyboard == NULL || keyboard->mode < BLOOM_UI_KEYBOARD_LOWER ||
        keyboard->mode >= BLOOM_UI_KEYBOARD_MODE_COUNT || keyboard->row >= 4 ||
        keyboard->column >= keyboard_row_length(keyboard)) {
        return '\0';
    }
    return keyboard_rows[keyboard->mode][keyboard->row][keyboard->column];
}

int bloom_ui_text_append(char *buffer, size_t capacity, char character)
{
    if (buffer == NULL || capacity == 0 || character < 0x20 || character > 0x7e) {
        return -1;
    }
    size_t length = strnlen(buffer, capacity);
    if (length >= capacity || length + 1 >= capacity) {
        return -1;
    }
    buffer[length] = character;
    buffer[length + 1] = '\0';
    return 0;
}

int bloom_ui_text_backspace(char *buffer, size_t capacity)
{
    if (buffer == NULL || capacity == 0) {
        return -1;
    }
    size_t length = strnlen(buffer, capacity);
    if (length >= capacity) {
        return -1;
    }
    if (length > 0) {
        buffer[length - 1] = '\0';
    }
    return 0;
}
