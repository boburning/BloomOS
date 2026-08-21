#include "bloom_library_database.h"
#include "bloom_library_import.h"
#include "bloom_library_legacy.h"
#include "bloom_library_query.h"
#include "bloom_library_scan.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define BLOOM_ROOT "/mnt/SDCARD/.bloom"
#define LIBRARY_ROOT BLOOM_ROOT "/library"
#define DATABASE_PATH LIBRARY_ROOT "/catalog.sqlite3"
#define SYSTEM_CATALOG_PATH "/mnt/SDCARD/.tmp_update/config/system-catalog.json"
#define EMU_ROOT "/mnt/SDCARD/Emu"
#define APP_ROOT "/mnt/SDCARD/App"
#define ROM_ROOT "/mnt/SDCARD/Roms"
#define FAVORITES_PATH ROM_ROOT "/favourite.json"
#define RECENTS_PATH ROM_ROOT "/recentlist.json"
#define RECENTS_HIDDEN_PATH ROM_ROOT "/recentlist-hidden.json"

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

static void print_json_string(const char *value)
{
    putchar('"');
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor) {
        if (*cursor == '"' || *cursor == '\\')
            printf("\\%c", *cursor);
        else if (*cursor < 0x20)
            printf("\\u%04x", *cursor);
        else
            putchar(*cursor);
    }
    putchar('"');
}

static int parse_limit(const char *value, size_t *limit)
{
    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed == 0 ||
        parsed > BLOOM_LIBRARY_QUERY_LIMIT_MAX)
        return -1;
    *limit = (size_t)parsed;
    return 0;
}

static int games_command(sqlite3 *database, int argc, char **argv)
{
    const char *system_id = NULL;
    const char *cursor = NULL;
    const char *limit_value = NULL;
    if (argc == 4 && strcmp(argv[2], "--limit") == 0)
        limit_value = argv[3];
    else if (argc == 6 && strcmp(argv[2], "--limit") == 0 &&
             strcmp(argv[4], "--after") == 0) {
        limit_value = argv[3];
        cursor = argv[5];
    }
    else if (argc == 6 && strcmp(argv[2], "--system") == 0 &&
             strcmp(argv[4], "--limit") == 0) {
        system_id = argv[3];
        limit_value = argv[5];
    }
    else if (argc == 8 && strcmp(argv[2], "--system") == 0 &&
             strcmp(argv[4], "--limit") == 0 && strcmp(argv[6], "--after") == 0) {
        system_id = argv[3];
        limit_value = argv[5];
        cursor = argv[7];
    }
    else
        return 2;
    size_t limit = 0;
    if (parse_limit(limit_value, &limit) != 0)
        return 2;
    BloomLibraryGame *games = calloc(limit, sizeof(*games));
    BloomLibraryGamePage page;
    if (games == NULL)
        return 1;
    int sql = bloom_library_query_games(database, system_id, cursor, limit, games, limit, &page);
    if (sql != SQLITE_OK) {
        free(games);
        return sql == SQLITE_MISUSE ? 2 : 1;
    }
    printf("{\"schema\":1,\"service\":\"bloom-library\",\"games\":[");
    for (size_t index = 0; index < page.count; ++index) {
        const BloomLibraryGame *game = &games[index];
        if (index > 0)
            putchar(',');
        printf("{\"game_id\":");
        print_json_string(game->bloom_game_id);
        printf(",\"system\":");
        print_json_string(game->system_id);
        printf(",\"rom_path\":");
        print_json_string(game->normalized_rom_path);
        printf(",\"title\":");
        print_json_string(game->display_title);
        printf(",\"image_path\":");
        if (game->image_path[0] == '\0')
            printf("null");
        else
            print_json_string(game->image_path);
        printf(",\"file_size\":%lld,\"file_mtime\":%lld}", (long long)game->file_size,
               (long long)game->file_mtime);
    }
    printf("],\"next_cursor\":");
    if (page.has_more)
        print_json_string(page.next_cursor);
    else
        printf("null");
    printf("}\n");
    free(games);
    return 0;
}

int main(int argc, char **argv)
{
    int status_command = argc == 2 && strcmp(argv[1], "status") == 0;
    int import_command = argc == 2 && strcmp(argv[1], "import-onion") == 0;
    int import_legacy = argc == 2 && strcmp(argv[1], "import-legacy") == 0;
    int scan_all = argc == 3 && strcmp(argv[1], "scan") == 0 &&
                   (strcmp(argv[2], "--changed") == 0 || strcmp(argv[2], "--all") == 0);
    int scan_system = argc == 4 && strcmp(argv[1], "scan") == 0 &&
                      strcmp(argv[2], "--system") == 0;
    int query_games = argc >= 2 && strcmp(argv[1], "games") == 0;
    if (!status_command && !import_command && !import_legacy && !scan_all && !scan_system &&
        !query_games) {
        fprintf(stderr,
                "Usage: bloom-library status|import-onion|import-legacy|scan "
                "--changed|--all|--system SYSTEM|games --limit N [--after GAME_ID]|games "
                "--system SYSTEM --limit N [--after GAME_ID]\n");
        return 2;
    }
    if (ensure_directory(BLOOM_ROOT) != 0 || ensure_directory(LIBRARY_ROOT) != 0) {
        fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"storage_unavailable\"}}\n");
        return 1;
    }
    sqlite3 *database = NULL;
    BloomLibraryHealth health;
    if (bloom_library_database_open(DATABASE_PATH, &database) != SQLITE_OK) {
        fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"library_unavailable\"}}\n");
        return 1;
    }
    if (import_command) {
        BloomLibraryImportResult imported;
        char error[160] = {0};
        if (bloom_library_import_onion(database, SYSTEM_CATALOG_PATH, EMU_ROOT, APP_ROOT, &imported,
                                       error, sizeof(error)) != SQLITE_OK) {
            sqlite3_close(database);
            fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"import_failed\"}}\n");
            return 1;
        }
        sqlite3_close(database);
        printf("{\"schema\":1,\"service\":\"bloom-library\",\"changed\":%s,"
               "\"generation\":%d,\"systems\":%d,\"apps\":%d}\n",
               imported.changed ? "true" : "false", imported.generation, imported.systems,
               imported.apps);
        return 0;
    }
    if (import_legacy) {
        BloomLibraryLegacyResult imported;
        char error[160] = {0};
        struct stat metadata;
        const char *recents = lstat(RECENTS_HIDDEN_PATH, &metadata) == 0 ? RECENTS_HIDDEN_PATH
                                                                         : RECENTS_PATH;
        if (bloom_library_import_legacy(database, ROM_ROOT, FAVORITES_PATH, recents, &imported,
                                        error, sizeof(error)) != SQLITE_OK) {
            sqlite3_close(database);
            fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"legacy_import_failed\"}}\n");
            return 1;
        }
        sqlite3_close(database);
        printf("{\"schema\":1,\"service\":\"bloom-library\",\"favorites\":%d,"
               "\"recents\":%d,\"matched\":%d,\"unmatched\":%d,\"duplicates\":%d,"
               "\"invalid\":%d}\n",
               imported.favorites, imported.recents, imported.matched, imported.unmatched,
               imported.duplicates, imported.invalid);
        return 0;
    }
    if (scan_all || scan_system) {
        BloomLibraryScanResult scanned;
        char error[160] = {0};
        const char *system_id = scan_system ? argv[3] : NULL;
        if (bloom_library_scan_games(database, ROM_ROOT, system_id, &scanned, error,
                                     sizeof(error)) != SQLITE_OK) {
            sqlite3_close(database);
            fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"scan_failed\"}}\n");
            return 1;
        }
        sqlite3_close(database);
        printf("{\"schema\":1,\"service\":\"bloom-library\",\"changed\":%s,"
               "\"generation\":%d,\"systems\":%d,\"games\":%d,\"errors\":%d}\n",
               scanned.changed ? "true" : "false", scanned.generation, scanned.systems,
               scanned.games, scanned.errors);
        return 0;
    }
    if (query_games) {
        int result = games_command(database, argc, argv);
        sqlite3_close(database);
        if (result == 2)
            fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"invalid_request\"}}\n");
        else if (result != 0)
            fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"query_failed\"}}\n");
        return result;
    }
    if (bloom_library_database_health(database, &health) != SQLITE_OK) {
        sqlite3_close(database);
        fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"library_unavailable\"}}\n");
        return 1;
    }
    sqlite3_close(database);
    printf("{\"schema\":1,\"service\":\"bloom-library\",\"state\":\"%s\","
           "\"database_schema\":%d,\"generation\":%d,\"systems\":%d,\"games\":%d,"
           "\"apps\":%d,\"favorites\":%d,\"recents\":%d}\n",
           health.status, health.schema_version, health.generation, health.systems, health.games,
           health.apps, health.favorites, health.recents);
    return 0;
}
