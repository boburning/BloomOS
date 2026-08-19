#include "bloom_launch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    fprintf(stderr, "Usage: bloom-launch validate REQUEST.json\n"
                    "       bloom-launch create REQUEST.json GAME_ID SYSTEM_ID ROM_PATH LAUNCHER EMULATOR CORE|- AUTO_LOAD\n"
                    "       bloom-launch get REQUEST.json FIELD\n"
                    "       bloom-launch resolve-achievements REQUEST.json ENABLED MODE OFFLINE_CASUAL PROXY_READY RA_GAME_ID CERTIFICATION\n"
                    "       bloom-launch write-ra-config REQUEST.json CONFIG USERNAME PROXY_HOST|- < TOKEN\n"
                    "       bloom-launch write-legacy REQUEST.json COMMAND.sh\n");
}

static int boolean(const char *value, int *output)
{
    if (strcmp(value, "true") == 0)
        *output = 1;
    else if (strcmp(value, "false") == 0)
        *output = 0;
    else
        return -1;
    return 0;
}

int main(int argc, char **argv)
{
    char error[256] = {0};
    char value[4096] = {0};
    int result = -1;
    if (argc == 3 && strcmp(argv[1], "validate") == 0)
        result = bloom_launch_validate_file(argv[2], error, sizeof(error));
    else if (argc == 10 && strcmp(argv[1], "create") == 0 &&
             (strcmp(argv[9], "true") == 0 || strcmp(argv[9], "false") == 0))
        result = bloom_launch_create_file(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7],
                                          strcmp(argv[8], "-") == 0 ? NULL : argv[8],
                                          strcmp(argv[9], "true") == 0, error, sizeof(error));
    else if (argc == 4 && strcmp(argv[1], "write-legacy") == 0)
        result = bloom_launch_write_legacy(argv[2], argv[3], error, sizeof(error));
    else if (argc == 9 && strcmp(argv[1], "resolve-achievements") == 0) {
        int enabled = 0, offline_casual = 0, proxy_ready = 0;
        char *end = NULL;
        long game_id = strtol(argv[7], &end, 10);
        if (boolean(argv[3], &enabled) != 0 || boolean(argv[5], &offline_casual) != 0 ||
            boolean(argv[6], &proxy_ready) != 0 || end == argv[7] || *end != '\0' || game_id < 0 ||
            game_id > 2147483647L) {
            usage();
            return 2;
        }
        result = bloom_launch_resolve_achievement_transport(argv[2], enabled, argv[4], offline_casual,
                                                            proxy_ready, (int)game_id, argv[8], error,
                                                            sizeof(error));
    }
    else if (argc == 6 && strcmp(argv[1], "write-ra-config") == 0) {
        char token[128] = {0};
        if (fgets(token, sizeof(token), stdin) == NULL || (!feof(stdin) && strchr(token, '\n') == NULL)) {
            fprintf(stderr, "bloom-launch: credential input is invalid\n");
            return 1;
        }
        token[strcspn(token, "\r\n")] = '\0';
        result = bloom_launch_write_ra_config(argv[2], argv[3], argv[4], token,
                                              strcmp(argv[5], "-") == 0 ? NULL : argv[5], error, sizeof(error));
        memset(token, 0, sizeof(token));
    }
    else if (argc == 4 && strcmp(argv[1], "get") == 0) {
        result = bloom_launch_get_string(argv[2], argv[3], value, sizeof(value), error, sizeof(error));
        if (result == 0)
            printf("%s\n", value);
    }
    else {
        usage();
        return 2;
    }
    if (result != 0) {
        fprintf(stderr, "bloom-launch: %s\n", error[0] == '\0' ? "request failed" : error);
        return 1;
    }
    return 0;
}
