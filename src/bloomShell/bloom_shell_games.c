#include "bloom_shell_games.h"

#include <string.h>

void bloom_shell_games_init(BloomShellGamesBrowser *browser)
{
    if (browser != NULL)
        memset(browser, 0, sizeof(*browser));
}

int bloom_shell_games_add(BloomShellGamesBrowser *browser, const BloomLibrarySystem *system,
                          size_t game_offset, size_t game_count, const char *core)
{
    if (browser == NULL || system == NULL || core == NULL || core[0] == '\0' || game_count == 0 ||
        browser->system_count >= BLOOM_SHELL_SYSTEM_CAPACITY || strlen(core) >= 128)
        return -1;
    BloomShellGamesSystem *entry = &browser->systems[browser->system_count++];
    entry->system = *system;
    entry->system.game_count = game_count;
    entry->game_offset = game_offset;
    bloom_ui_focus_init(&entry->focus, game_count);
    memcpy(entry->core, core, strlen(core) + 1);
    return 0;
}

int bloom_shell_games_switch(BloomShellGamesBrowser *browser, int direction)
{
    if (browser == NULL || browser->system_count < 2 || (direction != -1 && direction != 1))
        return 0;
    if (direction < 0)
        browser->selected_system = browser->selected_system == 0 ? browser->system_count - 1
                                                                 : browser->selected_system - 1;
    else
        browser->selected_system =
            browser->selected_system + 1 == browser->system_count ? 0
                                                                  : browser->selected_system + 1;
    bloom_shell_games_release(browser);
    return 1;
}

int bloom_shell_games_step(BloomShellGamesBrowser *browser, int direction, size_t visible_rows)
{
    BloomShellGamesSystem *system = bloom_shell_games_current_mutable(browser);
    if (system == NULL || (direction != -1 && direction != 1))
        return 0;
    if (browser->held_direction != direction) {
        browser->held_direction = direction;
        browser->held_repeats = 0;
    }
    else if (browser->held_repeats < 1000)
        ++browser->held_repeats;
    unsigned int distance = browser->held_repeats >= 10 ? 3 : 1;
    int changed = 0;
    for (unsigned int step = 0; step < distance; ++step)
        changed |= bloom_ui_focus_step(&system->focus, direction, visible_rows);
    return changed;
}

void bloom_shell_games_release(BloomShellGamesBrowser *browser)
{
    if (browser == NULL)
        return;
    browser->held_direction = 0;
    browser->held_repeats = 0;
}

const BloomShellGamesSystem *bloom_shell_games_current(const BloomShellGamesBrowser *browser)
{
    return browser != NULL && browser->selected_system < browser->system_count
               ? &browser->systems[browser->selected_system]
               : NULL;
}

BloomShellGamesSystem *bloom_shell_games_current_mutable(BloomShellGamesBrowser *browser)
{
    return browser != NULL && browser->selected_system < browser->system_count
               ? &browser->systems[browser->selected_system]
               : NULL;
}

const char *bloom_shell_games_default_core(const char *system_id)
{
    static const struct {
        const char *system_id;
        const char *core;
    } mappings[] = {
        {"gb", "gambatte_libretro.so"},
        {"gbc", "gambatte_libretro.so"},
        {"gba", "gpsp_libretro.so"},
        {"nes", "fceumm_libretro.so"},
        {"snes", "snes9x_libretro.so"},
        {"psx", "pcsx_rearmed_libretro.so"},
        {"pico8", "fake08_libretro.so"},
        {"scummvm", "scummvm_libretro.so"},
        {"sg1000", "genesis_plus_gx_libretro.so"},
    };
    if (system_id == NULL)
        return NULL;
    for (size_t index = 0; index < sizeof(mappings) / sizeof(mappings[0]); ++index)
        if (strcmp(system_id, mappings[index].system_id) == 0)
            return mappings[index].core;
    return NULL;
}
