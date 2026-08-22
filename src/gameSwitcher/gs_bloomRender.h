#ifndef GAME_SWITCHER_BLOOM_RENDER_H__
#define GAME_SWITCHER_BLOOM_RENDER_H__

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include "components/list.h"
#include "system/battery.h"
#include "system/display.h"
#include "theme/resources.h"

#define GS_BLOOM_CANVAS 0x211711
#define GS_BLOOM_SURFACE 0x352319
#define GS_BLOOM_RAISED 0x493025
#define GS_BLOOM_CREAM 0xF3E2BD
#define GS_BLOOM_SAND 0xCDAF7B
#define GS_BLOOM_ORANGE 0xD86A2C
#define GS_BLOOM_GOLD 0xE2A93B

static TTF_Font *bloom_gs_title_font = NULL;
static TTF_Font *bloom_gs_hint_font = NULL;

static TTF_Font *bloomGsTitleFont(void)
{
    if (bloom_gs_title_font == NULL)
        bloom_gs_title_font = TTF_OpenFont("/customer/app/wqy-microhei.ttc",
                                           g_display.height >= 540 ? 26 : 22);
    return bloom_gs_title_font != NULL ? bloom_gs_title_font : resource_getFont(TITLE);
}

static TTF_Font *bloomGsHintFont(void)
{
    if (bloom_gs_hint_font == NULL)
        bloom_gs_hint_font = TTF_OpenFont("/customer/app/wqy-microhei.ttc",
                                          g_display.height >= 540 ? 20 : 17);
    return bloom_gs_hint_font != NULL ? bloom_gs_hint_font : resource_getFont(HINT);
}

static void bloomGsFontsDestroy(void)
{
    if (bloom_gs_title_font != NULL) {
        TTF_CloseFont(bloom_gs_title_font);
        bloom_gs_title_font = NULL;
    }
    if (bloom_gs_hint_font != NULL) {
        TTF_CloseFont(bloom_gs_hint_font);
        bloom_gs_hint_font = NULL;
    }
}

static Uint32 bloomGsColor(uint32_t rgb)
{
    return SDL_MapRGB(screen->format, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb);
}

static void bloomGsFill(int x, int y, int width, int height, uint32_t rgb)
{
    if (width <= 0 || height <= 0)
        return;
    SDL_Rect rect = {(Sint16)x, (Sint16)y, (Uint16)width, (Uint16)height};
    SDL_FillRect(screen, &rect, bloomGsColor(rgb));
}

static void bloomGsText(TTF_Font *font, const char *text, int x, int y, int max_width,
                        uint32_t rgb)
{
    if (font == NULL || text == NULL || max_width <= 0)
        return;
    SDL_Color color = {(Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, 0};
    SDL_Surface *label = TTF_RenderUTF8_Blended(font, text, color);
    if (label == NULL)
        return;
    SDL_Rect source = {0, 0, (Uint16)(label->w < max_width ? label->w : max_width),
                       (Uint16)label->h};
    SDL_Rect destination = {(Sint16)x, (Sint16)y, source.w, source.h};
    SDL_BlitSurface(label, &source, screen, &destination);
    SDL_FreeSurface(label);
}

static void bloomGsMark(int center_x, int center_y, int scale)
{
    int petal = scale * 2;
    bloomGsFill(center_x - scale, center_y - scale * 4, petal, petal, GS_BLOOM_ORANGE);
    bloomGsFill(center_x - scale, center_y + scale * 2, petal, petal, GS_BLOOM_ORANGE);
    bloomGsFill(center_x - scale * 4, center_y - scale, petal, petal, GS_BLOOM_GOLD);
    bloomGsFill(center_x + scale * 2, center_y - scale, petal, petal, GS_BLOOM_GOLD);
    bloomGsFill(center_x - scale, center_y - scale, petal, petal, GS_BLOOM_CREAM);
}

static void bloomGsRenderHeader(int battery_percentage)
{
    int height = (int)(60.0 * g_scale);
    char battery[24];
    snprintf(battery, sizeof(battery), "%d%%", battery_percentage);
    bloomGsFill(0, 0, g_display.width, height, GS_BLOOM_SURFACE);
    bloomGsMark((int)(28.0 * g_scale), height / 2, g_display.height >= 540 ? 4 : 3);
    bloomGsText(bloomGsTitleFont(), "GameSwitcher", (int)(80.0 * g_scale),
                (int)(16.0 * g_scale), (int)(380.0 * g_scale), GS_BLOOM_CREAM);
    bloomGsText(bloomGsHintFont(), battery, g_display.width - (int)(78.0 * g_scale),
                (int)(21.0 * g_scale), (int)(64.0 * g_scale), GS_BLOOM_SAND);
}

static void bloomGsRenderFooter(int has_games)
{
    int height = (int)(60.0 * g_scale);
    int y = g_display.height - height;
    bloomGsFill(0, y, g_display.width, height, GS_BLOOM_SURFACE);
    bloomGsFill((int)(24.0 * g_scale), y + height / 2 - (int)(6.0 * g_scale),
                (int)(12.0 * g_scale), (int)(12.0 * g_scale), 0x708A4A);
    if (has_games) {
        bloomGsText(bloomGsHintFont(), "Left/Right Switch   A Resume   X Actions",
                    (int)(55.0 * g_scale), y + (int)(7.0 * g_scale),
                    g_display.width - (int)(70.0 * g_scale), GS_BLOOM_SAND);
        bloomGsText(bloomGsHintFont(), "B Home   MENU Close   START Quick",
                    (int)(55.0 * g_scale), y + (int)(31.0 * g_scale),
                    g_display.width - (int)(70.0 * g_scale), GS_BLOOM_SAND);
    }
    else {
        bloomGsText(bloomGsHintFont(), "B Home   MENU Close   START Quick",
                    (int)(55.0 * g_scale), y + (int)(19.0 * g_scale),
                    g_display.width - (int)(70.0 * g_scale), GS_BLOOM_SAND);
    }
}

static void bloomGsRenderOverlayFooter(int quick_settings)
{
    int height = (int)(60.0 * g_scale);
    int y = g_display.height - height;
    bloomGsFill(0, y, g_display.width, height, GS_BLOOM_SURFACE);
    bloomGsFill((int)(24.0 * g_scale), y + height / 2 - (int)(6.0 * g_scale),
                (int)(12.0 * g_scale), (int)(12.0 * g_scale), 0x708A4A);
    if (quick_settings) {
        bloomGsText(bloomGsHintFont(), "Up/Down Choose   Left/Right Change",
                    (int)(55.0 * g_scale), y + (int)(7.0 * g_scale),
                    g_display.width - (int)(70.0 * g_scale), GS_BLOOM_SAND);
        bloomGsText(bloomGsHintFont(), "A Toggle   B/START Close",
                    (int)(55.0 * g_scale), y + (int)(31.0 * g_scale),
                    g_display.width - (int)(70.0 * g_scale), GS_BLOOM_SAND);
    }
    else {
        bloomGsText(bloomGsHintFont(), "Up/Down Choose   A Open   B Close",
                    (int)(55.0 * g_scale), y + (int)(19.0 * g_scale),
                    g_display.width - (int)(70.0 * g_scale), GS_BLOOM_SAND);
    }
}

static void bloomGsRenderEmpty(void)
{
    int header = (int)(60.0 * g_scale);
    int footer = (int)(60.0 * g_scale);
    bloomGsFill(0, header, g_display.width, g_display.height - header - footer,
                GS_BLOOM_CANVAS);
    bloomGsText(bloomGsTitleFont(), "Nothing played yet", (int)(80.0 * g_scale),
                g_display.height / 2 - (int)(35.0 * g_scale),
                g_display.width - (int)(160.0 * g_scale), GS_BLOOM_CREAM);
    bloomGsText(bloomGsHintFont(), "Launch a game and it will appear here.",
                (int)(80.0 * g_scale), g_display.height / 2 + (int)(8.0 * g_scale),
                g_display.width - (int)(160.0 * g_scale), GS_BLOOM_SAND);
}

static void bloomGsRenderGameTitle(const char *title, int current, int total)
{
    int footer = (int)(60.0 * g_scale);
    int height = (int)(56.0 * g_scale);
    int y = g_display.height - footer - height;
    char position[24];
    snprintf(position, sizeof(position), "%d / %d", current, total);
    bloomGsFill(0, y, g_display.width, height, GS_BLOOM_RAISED);
    bloomGsFill(0, y, (int)(6.0 * g_scale), height, GS_BLOOM_ORANGE);
    bloomGsText(bloomGsTitleFont(), title, (int)(24.0 * g_scale),
                y + (int)(13.0 * g_scale), g_display.width - (int)(120.0 * g_scale),
                GS_BLOOM_CREAM);
    bloomGsText(bloomGsHintFont(), position, g_display.width - (int)(88.0 * g_scale),
                y + (int)(18.0 * g_scale), (int)(72.0 * g_scale), GS_BLOOM_SAND);
}

static void bloomGsRenderList(const char *title, List *list)
{
    if (list == NULL || !list->_created)
        return;
    int margin = (int)(24.0 * g_scale);
    int row_height = (int)(48.0 * g_scale);
    int width = g_display.width - margin * 2;
    int height = (list->item_count + 1) * row_height + margin;
    int maximum = g_display.height - (int)(100.0 * g_scale);
    if (height > maximum)
        height = maximum;
    int y = (g_display.height - height) / 2;
    bloomGsFill(margin - 4, y - 4, width + 8, height + 8, GS_BLOOM_ORANGE);
    bloomGsFill(margin, y, width, height, GS_BLOOM_RAISED);
    bloomGsText(bloomGsTitleFont(), title, margin + (int)(20.0 * g_scale),
                y + (int)(12.0 * g_scale), width - (int)(40.0 * g_scale), GS_BLOOM_CREAM);
    int visible = (height - row_height - margin) / row_height;
    int start = list->active_pos >= visible ? list->active_pos - visible + 1 : 0;
    for (int row = 0; row < visible && start + row < list->item_count; ++row) {
        int item = start + row;
        int row_y = y + row_height + row * row_height;
        int selected = item == list->active_pos;
        bloomGsFill(margin + (int)(12.0 * g_scale), row_y,
                    width - (int)(24.0 * g_scale), row_height - (int)(6.0 * g_scale),
                    selected ? GS_BLOOM_CREAM : GS_BLOOM_SURFACE);
        if (selected)
            bloomGsFill(margin + (int)(12.0 * g_scale), row_y,
                        (int)(5.0 * g_scale), row_height - (int)(6.0 * g_scale),
                        GS_BLOOM_ORANGE);
        bloomGsText(bloomGsHintFont(), list->items[item].label,
                    margin + (int)(28.0 * g_scale), row_y + (int)(10.0 * g_scale),
                    width - (int)(56.0 * g_scale), selected ? GS_BLOOM_CANVAS : GS_BLOOM_CREAM);
    }
}

static void bloomGsRenderDialog(const char *title, const char *message, int destructive)
{
    int width = g_display.width - (int)(96.0 * g_scale);
    int height = (int)(190.0 * g_scale);
    int x = (g_display.width - width) / 2;
    int y = (g_display.height - height) / 2;
    bloomGsFill(x - 4, y - 4, width + 8, height + 8,
                destructive ? 0xA84832 : GS_BLOOM_ORANGE);
    bloomGsFill(x, y, width, height, GS_BLOOM_RAISED);
    bloomGsText(bloomGsTitleFont(), title, x + (int)(24.0 * g_scale),
                y + (int)(22.0 * g_scale), width - (int)(48.0 * g_scale), GS_BLOOM_CREAM);
    bloomGsText(bloomGsHintFont(), message, x + (int)(24.0 * g_scale),
                y + (int)(76.0 * g_scale), width - (int)(48.0 * g_scale), GS_BLOOM_SAND);
    bloomGsText(bloomGsHintFont(), destructive ? "A Confirm   B Cancel" : "B Close",
                x + (int)(24.0 * g_scale), y + height - (int)(45.0 * g_scale),
                width - (int)(48.0 * g_scale), GS_BLOOM_CREAM);
}

static void bloomGsRenderMessage(const char *message)
{
    bloomGsFill(0, 0, g_display.width, g_display.height, GS_BLOOM_CANVAS);
    bloomGsMark(g_display.width / 2, g_display.height / 2 - (int)(50.0 * g_scale),
                g_display.height >= 540 ? 5 : 4);
    bloomGsText(bloomGsTitleFont(), message, (int)(80.0 * g_scale),
                g_display.height / 2, g_display.width - (int)(160.0 * g_scale),
                GS_BLOOM_CREAM);
}

static void bloomGsRenderSlotStatus(List *list, int selected, int total)
{
    if (list == NULL || total <= 0)
        return;
    int margin = (int)(24.0 * g_scale);
    int row_height = (int)(48.0 * g_scale);
    int height = (list->item_count + 1) * row_height + margin;
    int maximum = g_display.height - (int)(100.0 * g_scale);
    if (height > maximum)
        height = maximum;
    int y = (g_display.height - height) / 2;
    char slot[32];
    snprintf(slot, sizeof(slot), "Slot %d / %d", selected, total);
    bloomGsText(bloomGsHintFont(), slot, g_display.width - (int)(170.0 * g_scale),
                y + (int)(17.0 * g_scale), (int)(140.0 * g_scale), GS_BLOOM_GOLD);
}

#endif
