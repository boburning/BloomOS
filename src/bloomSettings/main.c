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
    fprintf(stderr,
            "Usage: bloom-settings status|import-onion|sync-onion|reconcile-onion|"
            "materialize-onion|activate-bloom|rollback-authority\n"
            "       bloom-settings set FIELD VALUE\n");
    return 2;
}

int main(int argc, char **argv)
{
    char error[160] = {0};
    if (argc == 4 && strcmp(argv[1], "set") == 0) {
        BloomSettingsMutationResult result;
        if (bloom_settings_set(SETTINGS_PATH, ONION_SYSTEM_PATH, ONION_CONFIG_ROOT, argv[2],
                               argv[3], &result, error, sizeof(error)) != 0) {
            fprintf(stderr,
                    "{\"schema\":1,\"error\":{\"code\":\"mutation_rejected\"},"
                    "\"changed\":%s,\"generation\":%d,\"materialized\":%s}\n",
                    result.changed ? "true" : "false", result.generation,
                    result.materialized ? "true" : "false");
            return 1;
        }
        printf("{\"schema\":1,\"service\":\"bloom-settings\",\"changed\":%s,"
               "\"generation\":%d,\"materialized\":%s}\n",
               result.changed ? "true" : "false", result.generation,
               result.materialized ? "true" : "false");
        return 0;
    }
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
    if (strcmp(argv[1], "sync-onion") == 0) {
        BloomSettingsSyncResult result;
        if (bloom_settings_sync_onion(ONION_SYSTEM_PATH, ONION_CONFIG_ROOT, SETTINGS_PATH,
                                      &result, error, sizeof(error)) != 0) {
            fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"sync_rejected\"}}\n");
            return 1;
        }
        printf("{\"schema\":1,\"service\":\"bloom-settings\",\"changed\":%s,"
               "\"generation\":%d}\n",
               result.changed ? "true" : "false", result.generation);
        return 0;
    }
    if (strcmp(argv[1], "reconcile-onion") == 0) {
        BloomSettingsSyncResult result;
        if (bloom_settings_reconcile_onion(ONION_SYSTEM_PATH, ONION_CONFIG_ROOT, SETTINGS_PATH,
                                           &result, error, sizeof(error)) != 0) {
            fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"reconcile_rejected\"}}\n");
            return 1;
        }
        printf("{\"schema\":1,\"service\":\"bloom-settings\",\"changed\":%s,"
               "\"generation\":%d}\n",
               result.changed ? "true" : "false", result.generation);
        return 0;
    }
    if (strcmp(argv[1], "materialize-onion") == 0) {
        if (bloom_settings_materialize_onion(SETTINGS_PATH, ONION_SYSTEM_PATH, ONION_CONFIG_ROOT,
                                             error, sizeof(error)) != 0) {
            fprintf(stderr,
                    "{\"schema\":1,\"error\":{\"code\":\"materialization_rejected\"}}\n");
            return 1;
        }
        printf("{\"schema\":1,\"service\":\"bloom-settings\",\"materialized\":true}\n");
        return 0;
    }
    if (strcmp(argv[1], "activate-bloom") == 0) {
        BloomSettingsAuthorityResult result;
        if (bloom_settings_activate(SETTINGS_PATH, ONION_SYSTEM_PATH, ONION_CONFIG_ROOT, &result,
                                    error, sizeof(error)) != 0) {
            fprintf(stderr,
                    "{\"schema\":1,\"error\":{\"code\":\"activation_rejected\"},"
                    "\"rolled_back\":%s}\n",
                    result.rolled_back ? "true" : "false");
            return 1;
        }
        printf("{\"schema\":1,\"service\":\"bloom-settings\",\"activated\":%s,"
               "\"generation\":%d}\n",
               result.changed ? "true" : "false", result.generation);
        return 0;
    }
    if (strcmp(argv[1], "rollback-authority") == 0) {
        BloomSettingsAuthorityResult result;
        if (bloom_settings_rollback_authority(SETTINGS_PATH, &result, error, sizeof(error)) != 0) {
            fprintf(stderr,
                    "{\"schema\":1,\"error\":{\"code\":\"rollback_rejected\"}}\n");
            return 1;
        }
        printf("{\"schema\":1,\"service\":\"bloom-settings\",\"rolled_back\":%s,"
               "\"generation\":%d}\n",
               result.changed ? "true" : "false", result.generation);
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
