#include "bloom_ui_renderer.h"

#include <SDL/SDL.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static const uint32_t palette[BLOOM_UI_COLOR_COUNT] = {
    0x211711, /* canvas */
    0x352319, /* surface */
    0x493025, /* surface-raised */
    0xF3E2BD, /* cream */
    0xCDAF7B, /* sand */
    0xD86A2C, /* orange */
    0xE2A93B, /* gold */
    0xA84832, /* red */
    0x708A4A, /* green */
};

uint32_t bloom_ui_color_rgb(BloomUiColor color)
{
    if (color < BLOOM_UI_COLOR_CANVAS || color >= BLOOM_UI_COLOR_COUNT) {
        return 0;
    }
    return palette[color];
}

static Uint32 mapped_color(SDL_Surface *surface, BloomUiColor color)
{
    uint32_t rgb = bloom_ui_color_rgb(color);
    return SDL_MapRGB(surface->format, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb);
}

static void fill(SDL_Surface *surface, int x, int y, int width, int height, BloomUiColor color)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    SDL_Rect rectangle = {(Sint16)x, (Sint16)y, (Uint16)width, (Uint16)height};
    SDL_FillRect(surface, &rectangle, mapped_color(surface, color));
}

static void draw_bloom_mark(SDL_Surface *surface, int center_x, int center_y, int scale)
{
    const int petal = scale * 2;
    fill(surface, center_x - scale, center_y - scale * 4, petal, petal, BLOOM_UI_COLOR_ORANGE);
    fill(surface, center_x - scale, center_y + scale * 2, petal, petal, BLOOM_UI_COLOR_ORANGE);
    fill(surface, center_x - scale * 4, center_y - scale, petal, petal, BLOOM_UI_COLOR_GOLD);
    fill(surface, center_x + scale * 2, center_y - scale, petal, petal, BLOOM_UI_COLOR_GOLD);
    fill(surface, center_x - scale * 3, center_y - scale * 3, petal, petal,
         BLOOM_UI_COLOR_SURFACE_RAISED);
    fill(surface, center_x + scale, center_y - scale * 3, petal, petal,
         BLOOM_UI_COLOR_SURFACE_RAISED);
    fill(surface, center_x - scale * 3, center_y + scale, petal, petal,
         BLOOM_UI_COLOR_SURFACE_RAISED);
    fill(surface, center_x + scale, center_y + scale, petal, petal,
         BLOOM_UI_COLOR_SURFACE_RAISED);
    fill(surface, center_x - scale, center_y - scale, petal, petal, BLOOM_UI_COLOR_CREAM);
}

static void draw_navigation(SDL_Surface *surface, const BloomUiLayout *layout,
                            BloomUiDestination destination)
{
    const int mark_width = layout->header.height;
    const int navigation_x = mark_width + layout->margin;
    const int navigation_width = layout->viewport_width - navigation_x - layout->margin;
    const int tab_width = navigation_width / BLOOM_UI_DESTINATION_COUNT;
    for (int index = 0; index < BLOOM_UI_DESTINATION_COUNT; ++index) {
        int x = navigation_x + index * tab_width;
        BloomUiColor color = index == (int)destination ? BLOOM_UI_COLOR_SURFACE_RAISED
                                                       : BLOOM_UI_COLOR_SURFACE;
        fill(surface, x + 2, 8, tab_width - 4, layout->header.height - 16, color);
        if (index == (int)destination) {
            fill(surface, x + 2, layout->header.height - 6, tab_width - 4, 4,
                 BLOOM_UI_COLOR_ORANGE);
        }
    }
    draw_bloom_mark(surface, layout->header.height / 2, layout->header.height / 2,
                    layout->viewport_height >= 540 ? 4 : 3);
}

static void draw_rows(SDL_Surface *surface, const BloomUiLayout *layout,
                      const BloomUiScene *scene)
{
    size_t available = 0;
    if (scene->window_start < scene->item_count) {
        available = scene->item_count - scene->window_start;
    }
    size_t row_count = available < layout->visible_rows ? available : layout->visible_rows;
    for (size_t row = 0; row < row_count; ++row) {
        size_t item = scene->window_start + row;
        int y = layout->content.y + (int)row * layout->row_height + 4;
        int height = layout->row_height - 8;
        int selected = item == scene->selected;
        fill(surface, layout->content.x, y, layout->content.width, height,
             selected ? BLOOM_UI_COLOR_SURFACE_RAISED : BLOOM_UI_COLOR_SURFACE);
        if (selected) {
            fill(surface, layout->content.x, y, 5, height, BLOOM_UI_COLOR_ORANGE);
        }
        /* Deterministic text placeholders; the font adapter overlays real labels. */
        int primary_width = layout->content.width * (int)(55 + (item % 4) * 7) / 100;
        fill(surface, layout->content.x + 20, y + height / 3, primary_width, 4,
             BLOOM_UI_COLOR_CREAM);
        fill(surface, layout->content.x + 20, y + height * 2 / 3, primary_width * 2 / 3, 3,
             BLOOM_UI_COLOR_SAND);
    }
}

static void draw_footer(SDL_Surface *surface, const BloomUiLayout *layout,
                        const BloomUiScene *scene)
{
    int status_size = layout->footer.height / 3;
    fill(surface, layout->margin, layout->footer.y + (layout->footer.height - status_size) / 2,
         status_size, status_size, scene->healthy ? BLOOM_UI_COLOR_GREEN : BLOOM_UI_COLOR_RED);
    if (scene->progress_maximum > 0) {
        int width = layout->viewport_width / 4;
        int x = layout->viewport_width - layout->margin - width;
        int y = layout->footer.y + layout->footer.height / 2 - 3;
        fill(surface, x, y, width, 6, BLOOM_UI_COLOR_SURFACE_RAISED);
        size_t bounded = scene->progress_value < scene->progress_maximum
                             ? scene->progress_value
                             : scene->progress_maximum;
        int progress = (int)((uint64_t)width * bounded / scene->progress_maximum);
        fill(surface, x, y, progress, 6, BLOOM_UI_COLOR_GOLD);
    }
}

int bloom_ui_render_shell(SDL_Surface *surface, const BloomUiLayout *layout,
                          const BloomUiScene *scene)
{
    if (surface == NULL || surface->format == NULL || surface->format->BitsPerPixel != 32 ||
        layout == NULL || scene == NULL || surface->w != layout->viewport_width ||
        surface->h != layout->viewport_height || scene->destination < BLOOM_UI_DESTINATION_HOME ||
        scene->destination >= BLOOM_UI_DESTINATION_COUNT ||
        (scene->item_count > 0 && scene->selected >= scene->item_count) ||
        scene->window_start > scene->item_count || scene->progress_value > UINT32_MAX ||
        scene->progress_maximum > UINT32_MAX) {
        return -1;
    }

    fill(surface, 0, 0, layout->viewport_width, layout->viewport_height, BLOOM_UI_COLOR_CANVAS);
    fill(surface, layout->header.x, layout->header.y, layout->header.width, layout->header.height,
         BLOOM_UI_COLOR_SURFACE);
    fill(surface, layout->footer.x, layout->footer.y, layout->footer.width, layout->footer.height,
         BLOOM_UI_COLOR_SURFACE);
    draw_navigation(surface, layout, scene->destination);
    draw_rows(surface, layout, scene);
    draw_footer(surface, layout, scene);
    return 0;
}

int bloom_ui_rotate_180(SDL_Surface *surface)
{
    if (surface == NULL || surface->format == NULL || surface->format->BitsPerPixel != 32 ||
        surface->pixels == NULL) {
        return -1;
    }
    if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) != 0) {
        return -1;
    }
    const size_t pixel_count = (size_t)surface->w * (size_t)surface->h;
    Uint8 *bytes = surface->pixels;
    for (size_t index = 0; index < pixel_count / 2; ++index) {
        size_t opposite = pixel_count - index - 1;
        size_t y = index / (size_t)surface->w;
        size_t x = index % (size_t)surface->w;
        size_t opposite_y = opposite / (size_t)surface->w;
        size_t opposite_x = opposite % (size_t)surface->w;
        Uint32 *first = (Uint32 *)(bytes + y * (size_t)surface->pitch + x * sizeof(Uint32));
        Uint32 *second =
            (Uint32 *)(bytes + opposite_y * (size_t)surface->pitch + opposite_x * sizeof(Uint32));
        Uint32 temporary = *first;
        *first = *second;
        *second = temporary;
    }
    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }
    return 0;
}

static int write_all_at(int file, const Uint8 *bytes, size_t size, off_t offset)
{
    while (size > 0) {
        ssize_t written = pwrite(file, bytes, size, offset);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return -1;
        bytes += written;
        size -= (size_t)written;
        offset += written;
    }
    return 0;
}

int bloom_ui_publish_framebuffer_pages(SDL_Surface *surface, const char *framebuffer_path,
                                       size_t page_count)
{
    if (surface == NULL || surface->format == NULL || surface->format->BitsPerPixel != 32 ||
        surface->pixels == NULL || surface->w <= 0 || surface->h <= 0 ||
        framebuffer_path == NULL || framebuffer_path[0] == '\0' || page_count == 0)
        return -1;
    const size_t row_size = (size_t)surface->w * sizeof(Uint32);
    const size_t page_size = row_size * (size_t)surface->h;
    if ((size_t)surface->pitch < row_size || page_size > (size_t)INT64_MAX / page_count)
        return -1;
    if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) != 0)
        return -1;
    int file = open(framebuffer_path, O_WRONLY);
    int result = file < 0 ? -1 : 0;
    const Uint8 *pixels = surface->pixels;
    for (size_t page = 0; result == 0 && page < page_count; ++page)
        for (int row = 0; result == 0 && row < surface->h; ++row)
            result = write_all_at(file, pixels + (size_t)row * (size_t)surface->pitch, row_size,
                                  (off_t)(page * page_size + (size_t)row * row_size));
    if (file >= 0 && close(file) != 0)
        result = -1;
    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);
    return result;
}
