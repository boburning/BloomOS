#include "bloom_ra.h"

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

static int print_status(void)
{
    BloomRaStatus status;
    bloom_ra_get_status(&status);
    printf("{\"schema\":%d,\"service\":\"bloom-ra\",\"enabled\":%s,\"state\":", status.schema,
           status.enabled ? "true" : "false");
    json_string(status.state);
    printf(",\"catalog\":{\"status\":");
    json_string(status.catalog_status);
    printf("},\"indexed_games\":%lu,\"identified_games\":%lu}\n", status.indexed_games,
           status.identified_games);
    return 0;
}

static int print_game(const char *game_id)
{
    BloomRaGame game;
    char error[128] = {0};
    if (bloom_ra_get_game(game_id, &game, error, sizeof(error)) != 0) {
        fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"invalid_game_id\",\"message\":\"%s\"}}\n",
                error);
        return 1;
    }
    printf("{\"schema\":%d,\"game_id\":", game.schema);
    json_string(game.game_id);
    printf(",\"status\":");
    json_string(game.status);
    printf(",\"has_ra_badge\":%s,\"ra\":{\"game_id\":null,\"official_set\":false,"
           "\"achievement_count\":%lu}}\n",
           game.has_ra_badge ? "true" : "false", game.achievement_count);
    return 0;
}

static int usage(void)
{
    fprintf(stderr, "Usage: bloom-ra {status|game BLOOM_GAME_ID}\n");
    return 2;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "status") == 0)
        return print_status();
    if (argc == 3 && strcmp(argv[1], "game") == 0)
        return print_game(argv[2]);
    return usage();
}
