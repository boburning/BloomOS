#include "bloom_ra.h"

#include "../bloomGameId/bloom_game_id.h"
#include "rc_hash.h"

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static _Thread_local char active_hash_root[PATH_MAX];

static bool path_in_active_root(const char *path, char resolved[PATH_MAX])
{
    struct stat metadata;
    if (path == NULL || lstat(path, &metadata) != 0 || !S_ISREG(metadata.st_mode) || S_ISLNK(metadata.st_mode) ||
        realpath(path, resolved) == NULL)
        return false;
    size_t root_length = strlen(active_hash_root);
    return strncmp(resolved, active_hash_root, root_length) == 0 &&
           (resolved[root_length] == '/' || resolved[root_length] == '\0');
}

static void *guarded_open(const char *path)
{
    char resolved[PATH_MAX];
    return path_in_active_root(path, resolved) ? fopen(resolved, "rb") : NULL;
}

static void guarded_seek(void *handle, int64_t offset, int origin) { fseeko((FILE *)handle, (off_t)offset, origin); }
static int64_t guarded_tell(void *handle) { return (int64_t)ftello((FILE *)handle); }
static size_t guarded_read(void *handle, void *buffer, size_t size) { return fread(buffer, 1, size, (FILE *)handle); }
static void guarded_close(void *handle) { fclose((FILE *)handle); }

void bloom_ra_get_status(BloomRaStatus *status)
{
    if (status == NULL)
        return;

    status->schema = BLOOM_RA_SCHEMA;
    status->enabled = 0;
    status->state = "not_configured";
    status->catalog_status = "not_implemented";
    status->indexed_games = 0;
    status->identified_games = 0;
}

int bloom_ra_get_game(const char *game_id, BloomRaGame *game, char *error, size_t error_size)
{
    if (game == NULL) {
        if (error != NULL && error_size > 0)
            snprintf(error, error_size, "missing game output");
        return -1;
    }

    if (!bloom_game_id_valid(game_id)) {
        if (error != NULL && error_size > 0)
            snprintf(error, error_size, "invalid Bloom GameID");
        return -1;
    }
    game->schema = BLOOM_RA_SCHEMA;
    game->game_id = game_id;
    game->status = "unindexed";
    game->has_ra_badge = 0;
    game->achievement_count = 0;
    return 0;
}

static void set_error(char *error, size_t size, const char *format, ...)
{
    if (error == NULL || size == 0)
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, size, format, args);
    va_end(args);
}

int bloom_ra_console_id(const char *system_id, uint32_t *console_id)
{
    static const struct {
        const char *system;
        uint32_t console;
    } mappings[] = {{"gb", RC_CONSOLE_GAMEBOY},
                    {"gbc", RC_CONSOLE_GAMEBOY_COLOR},
                    {"gba", RC_CONSOLE_GAMEBOY_ADVANCE},
                    {"nes", RC_CONSOLE_NINTENDO},
                    {"fds", RC_CONSOLE_FAMICOM_DISK_SYSTEM},
                    {"snes", RC_CONSOLE_SUPER_NINTENDO},
                    {"psx", RC_CONSOLE_PLAYSTATION},
                    {"genesis", RC_CONSOLE_MEGA_DRIVE},
                    {"segacd", RC_CONSOLE_SEGA_CD},
                    {"32x", RC_CONSOLE_SEGA_32X},
                    {"gamegear", RC_CONSOLE_GAME_GEAR},
                    {"mastersystem", RC_CONSOLE_MASTER_SYSTEM},
                    {"sg1000", RC_CONSOLE_SG1000},
                    {"arcade", RC_CONSOLE_ARCADE},
                    {"cps1", RC_CONSOLE_ARCADE},
                    {"cps2", RC_CONSOLE_ARCADE},
                    {"cps3", RC_CONSOLE_ARCADE},
                    {"neogeo", RC_CONSOLE_ARCADE},
                    {"atari2600", RC_CONSOLE_ATARI_2600},
                    {"atari7800", RC_CONSOLE_ATARI_7800},
                    {"lynx", RC_CONSOLE_ATARI_LYNX},
                    {"pce", RC_CONSOLE_PC_ENGINE},
                    {"pcecd", RC_CONSOLE_PC_ENGINE_CD},
                    {"supergrafx", RC_CONSOLE_PC_ENGINE},
                    {"wonderswan", RC_CONSOLE_WONDERSWAN},
                    {"ngpc", RC_CONSOLE_NEOGEO_POCKET},
                    {"virtualboy", RC_CONSOLE_VIRTUAL_BOY},
                    {"coleco", RC_CONSOLE_COLECOVISION},
                    {"msx", RC_CONSOLE_MSX},
                    {"amstrad", RC_CONSOLE_AMSTRAD_PC},
                    {"amiga", RC_CONSOLE_AMIGA}};
    if (system_id == NULL || console_id == NULL)
        return -1;
    for (size_t i = 0; i < sizeof(mappings) / sizeof(mappings[0]); i++) {
        if (strcmp(system_id, mappings[i].system) == 0) {
            *console_id = mappings[i].console;
            return 0;
        }
    }
    return -1;
}

int bloom_ra_hash_file(const char *system_id, const char *rom_path, const char *rom_root, char hash[33], char *error,
                       size_t error_size)
{
    uint32_t console_id;
    char resolved_path[PATH_MAX];
    char resolved_root[PATH_MAX];
    if (hash == NULL || bloom_ra_console_id(system_id, &console_id) != 0) {
        set_error(error, error_size, "unsupported RA system");
        return -1;
    }
    if (rom_path == NULL || rom_root == NULL || realpath(rom_root, resolved_root) == NULL) {
        set_error(error, error_size, "ROM path is not a readable regular file");
        return -1;
    }
    snprintf(active_hash_root, sizeof(active_hash_root), "%s", resolved_root);
    if (!path_in_active_root(rom_path, resolved_path)) {
        active_hash_root[0] = '\0';
        set_error(error, error_size, "ROM path is outside the allowed root");
        return -1;
    }
    rc_hash_iterator_t iterator;
    rc_hash_initialize_iterator(&iterator, resolved_path, NULL, 0);
    iterator.callbacks.filereader.open = guarded_open;
    iterator.callbacks.filereader.seek = guarded_seek;
    iterator.callbacks.filereader.tell = guarded_tell;
    iterator.callbacks.filereader.read = guarded_read;
    iterator.callbacks.filereader.close = guarded_close;
    int result = rc_hash_generate(hash, console_id, &iterator);
    rc_hash_destroy_iterator(&iterator);
    active_hash_root[0] = '\0';
    if (!result) {
        set_error(error, error_size, "rcheevos could not hash ROM content");
        return -1;
    }
    return 0;
}
