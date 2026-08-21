#include "bloom_settings.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define SETTINGS_ROOT "/mnt/SDCARD/.bloom/settings"
#define SETTINGS_PATH SETTINGS_ROOT "/settings.json"
#define SNAPSHOT_PATH SETTINGS_ROOT "/onion-system.snapshot.json"
#define ONION_SYSTEM_PATH "/mnt/SDCARD/system.json"
#define ONION_CONFIG_ROOT "/mnt/SDCARD/.tmp_update/config"

static int ensure_directory(const char *path)
{
    if (mkdir(path, 0700) == 0)
        return 0;
    struct stat metadata;
    if (errno == EEXIST && lstat(path, &metadata) == 0 && S_ISDIR(metadata.st_mode) &&
        !S_ISLNK(metadata.st_mode))
        return 0;
    return -1;
}

static int usage(void)
{
    fprintf(stderr, "Usage: bloom-settings status|import-onion\n");
    return 2;
}

int main(int argc, char **argv)
{
    char error[160] = {0};
    if (argc != 2)
        return usage();
    if (strcmp(argv[1], "status") == 0) {
        int schema = 0;
        char source[32] = {0};
        char authority[16] = {0};
        if (bloom_settings_status(SETTINGS_PATH, &schema, source, sizeof(source), authority,
                                  sizeof(authority), error, sizeof(error)) != 0) {
            fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"settings_unavailable\"}}\n");
            return 1;
        }
        printf("{\"schema\":1,\"service\":\"bloom-settings\",\"settings_schema\":%d,"
               "\"state\":\"ready\",\"source\":\"%s\",\"authority\":\"%s\"}\n",
               schema, source, authority);
        return 0;
    }
    if (strcmp(argv[1], "import-onion") != 0)
        return usage();
    if (ensure_directory("/mnt/SDCARD/.bloom") != 0 || ensure_directory(SETTINGS_ROOT) != 0) {
        fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"storage_unavailable\"}}\n");
        return 1;
    }
    BloomSettingsImportResult result;
    if (bloom_settings_import_onion(ONION_SYSTEM_PATH, ONION_CONFIG_ROOT, SETTINGS_PATH,
                                    SNAPSHOT_PATH, &result, error, sizeof(error)) != 0) {
        fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"import_failed\"}}\n");
        return 1;
    }
    printf("{\"schema\":1,\"service\":\"bloom-settings\",\"imported\":%s,"
           "\"used_defaults\":%s,\"legacy_snapshot_written\":%s}\n",
           result.imported ? "true" : "false", result.used_defaults ? "true" : "false",
           result.legacy_snapshot_written ? "true" : "false");
    return 0;
}
