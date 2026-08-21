#ifndef BLOOM_UI_INPUT_H
#define BLOOM_UI_INPUT_H

#include "bloom_ui_core.h"

#include <SDL/SDL_keysym.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maps Miyoo's established SDL key contract into Bloom semantic input. */
BloomUiInput bloom_ui_input_from_sdl_key(SDLKey key);

#ifdef __cplusplus
}
#endif

#endif
