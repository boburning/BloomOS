#include "bloom_library_database.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define BLOOM_ROOT "/mnt/SDCARD/.bloom"
#define LIBRARY_ROOT BLOOM_ROOT "/library"
#define DATABASE_PATH LIBRARY_ROOT "/catalog.sqlite3"

static int ensure_directory(const char *path)
{
    if (mkdir(path, 0700) == 0)
        return 0;
    struct stat metadata;
    return errno == EEXIST && lstat(path, &metadata) == 0 && S_ISDIR(metadata.st_mode) &&
                   !S_ISLNK(metadata.st_mode)
               ? 0
               : -1;
}

int main(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "status") != 0) {
        fprintf(stderr, "Usage: bloom-library status\n");
        return 2;
    }
    if (ensure_directory(BLOOM_ROOT) != 0 || ensure_directory(LIBRARY_ROOT) != 0) {
        fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"storage_unavailable\"}}\n");
        return 1;
    }
    sqlite3 *database = NULL;
    BloomLibraryHealth health;
    if (bloom_library_database_open(DATABASE_PATH, &database) != SQLITE_OK ||
        bloom_library_database_health(database, &health) != SQLITE_OK) {
        sqlite3_close(database);
        fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"library_unavailable\"}}\n");
        return 1;
    }
    sqlite3_close(database);
    printf("{\"schema\":1,\"service\":\"bloom-library\",\"state\":\"%s\","
           "\"database_schema\":%d,\"generation\":%d,\"systems\":%d,\"games\":%d,"
           "\"apps\":%d,\"favorites\":%d}\n",
           health.status, health.schema_version, health.generation, health.systems, health.games,
           health.apps, health.favorites);
    return 0;
}
