#include "bloom_game_id.h"

#include <stdio.h>
#include <string.h>

static void json_string(const char *value)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p == '"' || *p == '\\')
            putchar('\\');
        putchar(*p);
    }
    putchar('"');
}

int main(int argc, char **argv)
{
    int json = argc == 4 && strcmp(argv[1], "--json") == 0;
    if (!(argc == 3 || json)) {
        fprintf(stderr, "Usage: bloom-game-id [--json] SYSTEM_ID ROM_PATH\n");
        return 2;
    }
    const char *system = argv[json ? 2 : 1];
    const char *rom = argv[json ? 3 : 2];
    char game_id[BLOOM_GAME_ID_LENGTH + 1];
    char relative[4096];
    char error[256] = {0};
    if (bloom_game_id_create(system, rom, game_id, sizeof(game_id), relative, sizeof(relative), error, sizeof(error)) !=
        0) {
        fprintf(stderr, "bloom-game-id: %s\n", error);
        return 1;
    }
    if (!json) {
        printf("%s\n", game_id);
        return 0;
    }
    printf("{\"schema\":1,\"game_id\":");
    json_string(game_id);
    printf(",\"system_id\":");
    json_string(system);
    printf(",\"rom_relative_path\":");
    json_string(relative);
    printf("}\n");
    return 0;
}
