#include "bloom_save_flush.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: bloom-save-flush CORE_FILE\n");
        return 2;
    }
    const char *root = getenv("BLOOM_SD_ROOT");
    if (root == NULL || root[0] == '\0')
        root = "/mnt/SDCARD";
    struct bloom_save_flush_result result;
    char error[256] = {0};
    if (bloom_save_flush(root, argv[1], &result, error, sizeof(error)) != 0) {
        fprintf(stderr, "bloom-save-flush: %s\n", error[0] == '\0' ? "flush failed" : error);
        return 1;
    }
    printf("{\"schema\":1,\"core\":\"%s\",\"corename\":\"%s\",\"files_flushed\":%zu,"
           "\"directories_flushed\":%zu}\n",
           argv[1], result.corename, result.files_flushed, result.directories_flushed);
    return 0;
}
