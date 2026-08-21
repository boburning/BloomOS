#ifndef BLOOM_UI_RENDERER_H
#define BLOOM_UI_RENDERER_H

#include "bloom_ui_core.h"

#include <stddef.h>
#include <stdint.h>

typedef struct SDL_Surface SDL_Surface;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BLOOM_UI_COLOR_CANVAS = 0,
    BLOOM_UI_COLOR_SURFACE,
    BLOOM_UI_COLOR_SURFACE_RAISED,
    BLOOM_UI_COLOR_CREAM,
    BLOOM_UI_COLOR_SAND,
    BLOOM_UI_COLOR_ORANGE,
    BLOOM_UI_COLOR_GOLD,
    BLOOM_UI_COLOR_RED,
    BLOOM_UI_COLOR_GREEN,
    BLOOM_UI_COLOR_COUNT,
} BloomUiColor;

typedef struct {
    BloomUiDestination destination;
    size_t item_count;
    size_t selected;
    size_t window_start;
    size_t progress_value;
    size_t progress_maximum;
    int healthy;
} BloomUiScene;

/* Canonical RGB value encoded as 0xRRGGBB. */
uint32_t bloom_ui_color_rgb(BloomUiColor color);

/*
 * Draws deterministic, pixel-aligned shell geometry onto an existing 32-bit
 * SDL surface. Text and decoded images remain separate adapters so rendering
 * never owns navigation, network, or file-I/O state.
 */
int bloom_ui_render_shell(SDL_Surface *surface, const BloomUiLayout *layout,
                          const BloomUiScene *scene);

/* Adapts logical top-left rendering to the Mini-family physical framebuffer. */
int bloom_ui_rotate_180(SDL_Surface *surface);

#ifdef __cplusplus
}
#endif

#endif
