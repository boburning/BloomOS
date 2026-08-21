#ifndef TWEAKS_TEXT_ENTRY_H__
#define TWEAKS_TEXT_ENTRY_H__

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "system/keymap_sw.h"
#include "theme/render/dialog.h"
#include "theme/sound.h"

#include "./appstate.h"

typedef enum {
    TEXT_KEY_CHAR,
    TEXT_KEY_SHIFT,
    TEXT_KEY_SYMBOLS,
    TEXT_KEY_LETTERS,
    TEXT_KEY_BACKSPACE,
    TEXT_KEY_SPACE,
    TEXT_KEY_DONE,
} TextKeyAction;

typedef struct {
    const char *label;
    char value;
    TextKeyAction action;
} TextKey;

#define CHAR_KEY(character)                      \
    {                                            \
        #character, #character[0], TEXT_KEY_CHAR \
    }
#define NAMED_KEY(name, key_action) \
    {                               \
        name, 0, key_action         \
    }

static const TextKey text_keys_lower[][10] = {
    {CHAR_KEY(1), CHAR_KEY(2), CHAR_KEY(3), CHAR_KEY(4), CHAR_KEY(5), CHAR_KEY(6), CHAR_KEY(7), CHAR_KEY(8), CHAR_KEY(9), CHAR_KEY(0)},
    {CHAR_KEY(q), CHAR_KEY(w), CHAR_KEY(e), CHAR_KEY(r), CHAR_KEY(t), CHAR_KEY(y), CHAR_KEY(u), CHAR_KEY(i), CHAR_KEY(o), CHAR_KEY(p)},
    {CHAR_KEY(a), CHAR_KEY(s), CHAR_KEY(d), CHAR_KEY(f), CHAR_KEY(g), CHAR_KEY(h), CHAR_KEY(j), CHAR_KEY(k), CHAR_KEY(l)},
    {NAMED_KEY("Shift", TEXT_KEY_SHIFT), CHAR_KEY(z), CHAR_KEY(x), CHAR_KEY(c), CHAR_KEY(v), CHAR_KEY(b), CHAR_KEY(n), CHAR_KEY(m), NAMED_KEY("Bksp", TEXT_KEY_BACKSPACE)},
    {NAMED_KEY("123!?", TEXT_KEY_SYMBOLS), NAMED_KEY("Space", TEXT_KEY_SPACE), NAMED_KEY("Done", TEXT_KEY_DONE)},
};

static const TextKey text_keys_upper[][10] = {
    {CHAR_KEY(1), CHAR_KEY(2), CHAR_KEY(3), CHAR_KEY(4), CHAR_KEY(5), CHAR_KEY(6), CHAR_KEY(7), CHAR_KEY(8), CHAR_KEY(9), CHAR_KEY(0)},
    {CHAR_KEY(Q), CHAR_KEY(W), CHAR_KEY(E), CHAR_KEY(R), CHAR_KEY(T), CHAR_KEY(Y), CHAR_KEY(U), CHAR_KEY(I), CHAR_KEY(O), CHAR_KEY(P)},
    {CHAR_KEY(A), CHAR_KEY(S), CHAR_KEY(D), CHAR_KEY(F), CHAR_KEY(G), CHAR_KEY(H), CHAR_KEY(J), CHAR_KEY(K), CHAR_KEY(L)},
    {NAMED_KEY("shift", TEXT_KEY_SHIFT), CHAR_KEY(Z), CHAR_KEY(X), CHAR_KEY(C), CHAR_KEY(V), CHAR_KEY(B), CHAR_KEY(N), CHAR_KEY(M), NAMED_KEY("Bksp", TEXT_KEY_BACKSPACE)},
    {NAMED_KEY("123!?", TEXT_KEY_SYMBOLS), NAMED_KEY("Space", TEXT_KEY_SPACE), NAMED_KEY("Done", TEXT_KEY_DONE)},
};

static const TextKey text_keys_symbols[][10] = {
    {{"`", '`', TEXT_KEY_CHAR}, {"~", '~', TEXT_KEY_CHAR}, {"!", '!', TEXT_KEY_CHAR}, {"@", '@', TEXT_KEY_CHAR}, {"#", '#', TEXT_KEY_CHAR}, {"$", '$', TEXT_KEY_CHAR}, {"%", '%', TEXT_KEY_CHAR}, {"^", '^', TEXT_KEY_CHAR}, {"&", '&', TEXT_KEY_CHAR}, {"*", '*', TEXT_KEY_CHAR}},
    {{"(", '(', TEXT_KEY_CHAR}, {")", ')', TEXT_KEY_CHAR}, {"-", '-', TEXT_KEY_CHAR}, {"_", '_', TEXT_KEY_CHAR}, {"=", '=', TEXT_KEY_CHAR}, {"+", '+', TEXT_KEY_CHAR}, {"[", '[', TEXT_KEY_CHAR}, {"]", ']', TEXT_KEY_CHAR}, {"{", '{', TEXT_KEY_CHAR}, {"}", '}', TEXT_KEY_CHAR}},
    {{"\\", '\\', TEXT_KEY_CHAR}, {"|", '|', TEXT_KEY_CHAR}, {";", ';', TEXT_KEY_CHAR}, {":", ':', TEXT_KEY_CHAR}, {"'", '\'', TEXT_KEY_CHAR}, {"\"", '\"', TEXT_KEY_CHAR}, {",", ',', TEXT_KEY_CHAR}, {".", '.', TEXT_KEY_CHAR}, {"<", '<', TEXT_KEY_CHAR}, {">", '>', TEXT_KEY_CHAR}},
    {{"/", '/', TEXT_KEY_CHAR}, {"?", '?', TEXT_KEY_CHAR}, NAMED_KEY("Bksp", TEXT_KEY_BACKSPACE)},
    {NAMED_KEY("ABC", TEXT_KEY_LETTERS), NAMED_KEY("Space", TEXT_KEY_SPACE), NAMED_KEY("Done", TEXT_KEY_DONE)},
};

static const int text_key_counts[] = {10, 10, 9, 9, 3};
static const int text_symbol_key_counts[] = {10, 10, 10, 3, 3};

static const TextKey *text_entry_key(int page, int row, int column)
{
    if (page == 1)
        return &text_keys_upper[row][column];
    if (page == 2)
        return &text_keys_symbols[row][column];
    return &text_keys_lower[row][column];
}

static int text_entry_row_count(int page, int row)
{
    return page == 2 ? text_symbol_key_counts[row] : text_key_counts[row];
}

static float text_entry_key_weight(const TextKey *key)
{
    if (key->action == TEXT_KEY_SPACE)
        return 4.5f;
    if (key->action == TEXT_KEY_SYMBOLS || key->action == TEXT_KEY_LETTERS ||
        key->action == TEXT_KEY_DONE)
        return 1.8f;
    if (key->action == TEXT_KEY_SHIFT || key->action == TEXT_KEY_BACKSPACE)
        return 1.5f;
    return 1.0f;
}

static void text_entry_draw_label(const char *label, TTF_Font *font,
                                  SDL_Color color, SDL_Rect bounds)
{
    SDL_Surface *text = TTF_RenderUTF8_Blended(font, label, color);
    if (text == NULL)
        return;
    SDL_Rect position = {
        bounds.x + (bounds.w - text->w) / 2,
        bounds.y + (bounds.h - text->h) / 2,
    };
    SDL_BlitSurface(text, NULL, screen, &position);
    SDL_FreeSurface(text);
}

static void text_entry_render(const char *title, const char *value, bool masked,
                              int page, int row, int column)
{
    char shown[33] = {0};
    size_t length = strlen(value);
    size_t start = length > 30 ? length - 30 : 0;
    size_t shown_length = length - start;
    for (size_t i = 0; i < shown_length; i++)
        shown[i] = masked ? '*' : value[start + i];
    if (shown[0] == '\0')
        strcpy(shown, "_");

    SDL_BlitSurface(background_cache, NULL, screen, NULL);

    Uint32 backdrop = SDL_MapRGB(screen->format, 18, 21, 25);
    Uint32 field_color = SDL_MapRGB(screen->format, 35, 40, 46);
    Uint32 key_color = SDL_MapRGB(screen->format, 54, 60, 67);
    Uint32 selected_color = SDL_MapRGB(screen->format, 235, 238, 242);
    SDL_Color primary_text = {245, 247, 250, 0};
    SDL_Color secondary_text = {177, 184, 193, 0};
    SDL_Color selected_text = {20, 23, 27, 0};
    SDL_FillRect(screen, NULL, backdrop);

    TTF_Font *title_font = resource_getFont(TITLE);
    TTF_Font *key_font = resource_getFont(HINT);

    SDL_Rect title_bounds = {24, 13, g_display.width - 48, 35};
    text_entry_draw_label(title, title_font, primary_text, title_bounds);

    SDL_Rect field = {24, 52, g_display.width - 48, 52};
    SDL_FillRect(screen, &field, field_color);
    SDL_Rect field_text = {field.x + 14, field.y, field.w - 28, field.h};
    SDL_Surface *value_text = TTF_RenderUTF8_Blended(title_font, shown, primary_text);
    if (value_text != NULL) {
        SDL_Rect position = {field_text.x, field_text.y + (field_text.h - value_text->h) / 2};
        SDL_BlitSurface(value_text, NULL, screen, &position);
        SDL_FreeSurface(value_text);
    }

    const int keyboard_x = 18;
    const int keyboard_width = g_display.width - keyboard_x * 2;
    const int keyboard_y = 118;
    const int key_height = 52;
    const int gap = 6;
    for (int y = 0; y < 5; y++) {
        int count = text_entry_row_count(page, y);
        float total_weight = 0.0f;
        for (int x = 0; x < count; x++)
            total_weight += text_entry_key_weight(text_entry_key(page, y, x));
        float unit = (keyboard_width - gap * (count - 1)) / total_weight;
        int row_width = 0;
        for (int x = 0; x < count; x++)
            row_width += (int)(unit * text_entry_key_weight(text_entry_key(page, y, x)));
        row_width += gap * (count - 1);
        int key_x = keyboard_x + (keyboard_width - row_width) / 2;

        for (int x = 0; x < count; x++) {
            const TextKey *key = text_entry_key(page, y, x);
            int key_width = (int)(unit * text_entry_key_weight(key));
            SDL_Rect key_rect = {key_x, keyboard_y + y * (key_height + gap), key_width, key_height};
            bool selected = y == row && x == column;
            SDL_FillRect(screen, &key_rect, selected ? selected_color : key_color);
            text_entry_draw_label(key->label, key_font,
                                  selected ? selected_text : primary_text,
                                  key_rect);
            key_x += key_width + gap;
        }
    }

    SDL_Rect footer = {24, g_display.height - 48, g_display.width - 48, 32};
    text_entry_draw_label("D-pad  Move        A  Select        B  Cancel",
                          key_font, secondary_text, footer);
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
}

static bool text_entry_dialog(const char *title, char *value, size_t value_size,
                              bool masked)
{
    bool done = false;
    bool accepted = false;
    int page = 0;
    int row = 1;
    int column = 0;
    SDLKey changed_key = SDLK_UNKNOWN;

    keys_enabled = false;
    background_cache = SDL_CreateRGBSurface(SDL_HWSURFACE, g_display.width, g_display.height, 32, 0, 0, 0, 0);
    if (background_cache == NULL)
        return false;
    SDL_BlitSurface(screen, NULL, background_cache, NULL);
    text_entry_render(title, value, masked, page, row, column);

    while (!done) {
        if (!updateKeystate(keystate, &done, true, &changed_key) ||
            changed_key == SDLK_UNKNOWN || keystate[changed_key] != PRESSED)
            continue;
        if (changed_key == SW_BTN_UP)
            row = (row + 4) % 5;
        else if (changed_key == SW_BTN_DOWN)
            row = (row + 1) % 5;
        else if (changed_key == SW_BTN_LEFT)
            column = (column + text_entry_row_count(page, row) - 1) % text_entry_row_count(page, row);
        else if (changed_key == SW_BTN_RIGHT)
            column = (column + 1) % text_entry_row_count(page, row);
        else if (changed_key == SW_BTN_Y) {
            size_t length = strlen(value);
            if (length > 0)
                value[length - 1] = '\0';
        }
        else if (changed_key == SW_BTN_START) {
            accepted = value[0] != '\0';
            done = accepted;
        }
        else if (changed_key == SW_BTN_A) {
            const TextKey *key = text_entry_key(page, row, column);
            size_t length = strlen(value);
            if (key->action == TEXT_KEY_SHIFT)
                page = page == 1 ? 0 : 1;
            else if (key->action == TEXT_KEY_SYMBOLS)
                page = 2;
            else if (key->action == TEXT_KEY_LETTERS)
                page = 0;
            else if (key->action == TEXT_KEY_BACKSPACE) {
                if (length > 0)
                    value[length - 1] = '\0';
            }
            else if (key->action == TEXT_KEY_DONE) {
                accepted = value[0] != '\0';
                done = accepted;
            }
            else if (length + 1 < value_size) {
                value[length] = key->action == TEXT_KEY_SPACE ? ' ' : key->value;
                value[length + 1] = '\0';
            }
        }
        else if (changed_key == SW_BTN_B) {
            done = true;
        }
        column %= text_entry_row_count(page, row);
        sound_change();
        if (!done)
            text_entry_render(title, value, masked, page, row, column);
    }

    SDL_FreeSurface(background_cache);
    background_cache = NULL;
    keys_enabled = true;
    all_changed = true;
    return accepted;
}

#undef CHAR_KEY
#undef NAMED_KEY

#endif
