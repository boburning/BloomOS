#include "bloom_launch.h"

#include <stdio.h>
#include <string.h>

static void usage(void)
{
    fprintf(stderr, "Usage: bloom-launch validate REQUEST.json\n"
                    "       bloom-launch create REQUEST.json GAME_ID SYSTEM_ID ROM_PATH LAUNCHER EMULATOR CORE|- AUTO_LOAD\n"
                    "       bloom-launch get REQUEST.json FIELD\n"
                    "       bloom-launch write-legacy REQUEST.json COMMAND.sh\n");
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
