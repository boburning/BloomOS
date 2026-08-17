#include "gameSwitcherIdentity.h"

#include "../bloomGameId/bloom_game_id.h"

#include <stdio.h>
#include <string.h>

struct launcher_identity {
    const char *launcher;
    const char *system_id;
};

int gameswitcher_game_id(const char *launcher, const char *rom_path, char *game_id, size_t game_id_size)
{
    static const struct launcher_identity identities[] = {
        {"/mnt/SDCARD/Emu/GB/launch.sh", "gb"},
        {"/mnt/SDCARD/Emu/GBC/launch.sh", "gbc"},
        {"/mnt/SDCARD/Emu/GBA/launch.sh", "gba"},
        {"/mnt/SDCARD/Emu/FC/launch.sh", "nes"},
        {"/mnt/SDCARD/Emu/SFC/launch.sh", "snes"},
        {"/mnt/SDCARD/Emu/PS/launch.sh", "psx"},
    };
    if (launcher == NULL || rom_path == NULL || game_id == NULL)
        return -1;

    const char *system_id = NULL;
    for (size_t i = 0; i < sizeof(identities) / sizeof(identities[0]); i++) {
        if (strcmp(launcher, identities[i].launcher) == 0) {
            system_id = identities[i].system_id;
            break;
        }
    }
    if (system_id == NULL)
        return -1;

    char relative_path[4096];
    char error[128];
    return bloom_game_id_create(system_id, rom_path, game_id, game_id_size, relative_path, sizeof(relative_path), error,
                                sizeof(error));
}

int gameswitcher_romscreen_path(const char *game_id, char *path, size_t path_size)
{
    static const char prefix[] = "bloom-game-v1:";
    if (!bloom_game_id_valid(game_id) || path == NULL || path_size == 0)
        return -1;
    const char *digest = game_id + strlen(prefix);
    int length = snprintf(path, path_size, "/mnt/SDCARD/Saves/CurrentProfile/romScreens/%s.png", digest);
    return length > 0 && (size_t)length < path_size ? 0 : -1;
}
