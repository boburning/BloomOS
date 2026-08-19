#include "bloom_ra.h"
#include "bloom_ra_database.h"
#include "bloom_ra_scanner.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define RA_ROOT "/mnt/SDCARD/.bloom/achievements"
#define CANCEL_PATH RA_ROOT "/scan.cancel"
#define SESSION_STATE "/tmp/bloom-session/state"
#define SCAN_LOCK "/tmp/bloom-ra-scan.lock"
#define SCAN_PID SCAN_LOCK "/pid"

typedef struct {
    const char *folder;
    const char *system;
} ScanSystem;

static const ScanSystem scan_systems[] = {{"GB", "gb"}, {"GBC", "gbc"}, {"GBA", "gba"}, {"FC", "nes"}, {"FDS", "fds"}, {"SFC", "snes"}, {"PS", "psx"}, {"MD", "genesis"}, {"SEGACD", "segacd"}, {"THIRTYTWOX", "32x"}, {"GG", "gamegear"}, {"MS", "mastersystem"}, {"SEGASGONE", "sg1000"}, {"ARCADE", "arcade"}, {"CPS1", "cps1"}, {"CPS2", "cps2"}, {"CPS3", "cps3"}, {"NEOGEO", "neogeo"}, {"ATARI", "atari2600"}, {"SEVENTYEIGHTHUNDRED", "atari7800"}, {"LYNX", "lynx"}, {"PCE", "pce"}, {"PCECD", "pcecd"}, {"SGFX", "supergrafx"}, {"VB", "virtualboy"}, {"WS", "wonderswan"}, {"NGP", "ngpc"}, {"COLECO", "coleco"}, {"MSX", "msx"}, {"CPC", "amstrad"}, {"AMIGA", "amiga"}};

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

static int ensure_directory(const char *path)
{
    struct stat status;
    if (lstat(path, &status) == 0)
        return S_ISDIR(status.st_mode) && !S_ISLNK(status.st_mode) ? 0 : -1;
    return errno == ENOENT && mkdir(path, 0700) == 0 ? 0 : -1;
}

static int open_catalog(sqlite3 **database)
{
    if (ensure_directory("/mnt/SDCARD/.bloom") != 0 || ensure_directory(RA_ROOT) != 0)
        return SQLITE_CANTOPEN;
    return bloom_ra_database_open(BLOOM_RA_DATABASE_PATH, database);
}

static int scan_lock_active(void)
{
    struct stat status;
    if (lstat(SCAN_LOCK, &status) != 0)
        return 0;
    if (!S_ISDIR(status.st_mode) || S_ISLNK(status.st_mode))
        return 1;
    FILE *file = fopen(SCAN_PID, "r");
    long pid = 0;
    int valid = file != NULL && fscanf(file, "%ld", &pid) == 1 && pid > 1;
    if (file != NULL)
        fclose(file);
    if (valid && (kill((pid_t)pid, 0) == 0 || errno == EPERM))
        return 1;
    unlink(SCAN_PID);
    rmdir(SCAN_LOCK);
    return 0;
}

static int acquire_scan_lock(void)
{
    if (scan_lock_active() || mkdir(SCAN_LOCK, 0700) != 0)
        return -1;
    int descriptor = open(SCAN_PID, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (descriptor < 0) {
        rmdir(SCAN_LOCK);
        return -1;
    }
    char pid[32];
    int length = snprintf(pid, sizeof(pid), "%ld\n", (long)getpid());
    int result = write(descriptor, pid, (size_t)length) == length ? 0 : -1;
    close(descriptor);
    if (result != 0) {
        unlink(SCAN_PID);
        rmdir(SCAN_LOCK);
    }
    return result;
}

static void release_scan_lock(void)
{
    unlink(SCAN_PID);
    rmdir(SCAN_LOCK);
}

static int scan_status(void)
{
    sqlite3 *database = NULL;
    int result = open_catalog(&database);
    int version = 0, indexed = 0, identified = 0;
    if (result == SQLITE_OK)
        result = bloom_ra_database_health(database, &version, &indexed, &identified);
    if (database != NULL)
        sqlite3_close(database);
    if (result != SQLITE_OK) {
        fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"database_unavailable\"}}\n");
        return 1;
    }
    printf("{\"schema\":1,\"state\":\"%s\",\"database_schema\":%d,\"indexed_games\":%d,"
           "\"identified_games\":%d}\n",
           scan_lock_active() ? "running" : "idle", version, indexed, identified);
    return 0;
}

static int scan_cancel(void)
{
    if (ensure_directory("/mnt/SDCARD/.bloom") != 0 || ensure_directory(RA_ROOT) != 0)
        return 1;
    struct stat status;
    if (lstat(CANCEL_PATH, &status) == 0) {
        if (!S_ISREG(status.st_mode) || S_ISLNK(status.st_mode))
            return 1;
        printf("{\"schema\":1,\"cancel_requested\":true}\n");
        return 0;
    }
    int descriptor = open(CANCEL_PATH, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (descriptor < 0)
        return 1;
    close(descriptor);
    printf("{\"schema\":1,\"cancel_requested\":true}\n");
    return 0;
}

static int scan_run(const ScanSystem *selected, int force)
{
    if (acquire_scan_lock() != 0) {
        fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"scan_already_running\"}}\n");
        return 1;
    }
    sqlite3 *database = NULL;
    int result = open_catalog(&database);
    if (result != SQLITE_OK) {
        release_scan_lock();
        return 1;
    }
    unlink(CANCEL_PATH);
    nice(10);
    BloomRaScanStats total = {0};
    size_t start = selected == NULL ? 0 : (size_t)(selected - scan_systems);
    size_t end = selected == NULL ? sizeof(scan_systems) / sizeof(scan_systems[0]) : start + 1;
    for (size_t i = start; i < end; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/mnt/SDCARD/Roms/%s", scan_systems[i].folder);
        BloomRaScanStats current;
        int scan = bloom_ra_scan_tree(database, scan_systems[i].system, path, force, SESSION_STATE, CANCEL_PATH,
                                      &current);
        if (scan == SQLITE_CANTOPEN && selected == NULL)
            continue;
        total.processed += current.processed;
        total.skipped += current.skipped;
        total.identified += current.identified;
        total.errors += current.errors;
        total.canceled |= current.canceled;
        total.paused |= current.paused;
        if (scan == SQLITE_INTERRUPT || scan == SQLITE_BUSY || scan == SQLITE_CANTOPEN) {
            result = scan;
            break;
        }
    }
    sqlite3_close(database);
    unlink(CANCEL_PATH);
    release_scan_lock();
    printf("{\"schema\":1,\"processed\":%lu,\"skipped\":%lu,\"identified\":%lu,\"errors\":%lu,"
           "\"canceled\":%s,\"paused\":%s}\n",
           total.processed, total.skipped, total.identified, total.errors, total.canceled ? "true" : "false",
           total.paused ? "true" : "false");
    return result == SQLITE_OK || result == SQLITE_INTERRUPT || result == SQLITE_BUSY ? 0 : 1;
}

static int scan_command(int argc, char **argv)
{
    if (argc == 1 && strcmp(argv[0], "--status") == 0)
        return scan_status();
    if (argc == 1 && strcmp(argv[0], "--cancel") == 0)
        return scan_cancel();
    if (argc == 1 && strcmp(argv[0], "--changed") == 0)
        return scan_run(NULL, 0);
    if (argc == 1 && strcmp(argv[0], "--all") == 0)
        return scan_run(NULL, 1);
    if (argc == 2 && strcmp(argv[0], "--system") == 0) {
        for (size_t i = 0; i < sizeof(scan_systems) / sizeof(scan_systems[0]); i++)
            if (strcmp(argv[1], scan_systems[i].folder) == 0)
                return scan_run(&scan_systems[i], 0);
        fprintf(stderr, "{\"schema\":1,\"error\":{\"code\":\"unsupported_system\"}}\n");
        return 1;
    }
    return 2;
}

static int usage(void)
{
    fprintf(stderr, "Usage: bloom-ra {status|game BLOOM_GAME_ID|scan --changed|--all|--system SYSTEM|--status|--cancel}\n");
    return 2;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "status") == 0)
        return print_status();
    if (argc == 3 && strcmp(argv[1], "game") == 0)
        return print_game(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "scan") == 0) {
        int result = scan_command(argc - 2, argv + 2);
        return result == 2 ? usage() : result;
    }
    return usage();
}
