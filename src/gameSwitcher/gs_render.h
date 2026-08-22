#ifndef GAME_SWITCHER_RENDER_H__
#define GAME_SWITCHER_RENDER_H__

#include <SDL/SDL.h>

#include "gs_appState.h"
#include "gs_bloomRender.h"
#include "gs_model.h"
#include "gs_popMenu.h"

void renderCentered(SDL_Surface *image, int view_mode, SDL_Rect *overrideSrcRect,
                    SDL_Rect *overrideDestRect)
{
    (void)view_mode;
    int offset_x = (g_display.width - image->w) / 2;
    int offset_y = (g_display.height - image->h) / 2;
    SDL_Rect image_size = {0, 0, (Uint16)g_display.width, (Uint16)g_display.height};
    SDL_Rect image_position = {(Sint16)offset_x, (Sint16)offset_y, 0, 0};
    if (overrideSrcRect != NULL) {
        image_size = *overrideSrcRect;
        image_size.x -= offset_x;
        image_size.y -= offset_y;
    }
    if (overrideDestRect != NULL)
        image_position = *overrideDestRect;
    SDL_BlitSurface(image, &image_size, screen, &image_position);
}

void render_showFullscreenMessage(const char *message, bool draw_bg)
{
    (void)draw_bg;
    bloomGsRenderMessage(message);
    render();
}

#endif
