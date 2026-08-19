#include "bloom_ra_archive.h"

#include "unzip.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define BLOOM_RA_ARCHIVE_MAX_ROM_SIZE (48UL * 1024UL * 1024UL)

static void set_error(char *error, size_t size, const char *format, ...)
{
    if (error == NULL || size == 0)
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, size, format, args);
    va_end(args);
}

static const char *extension(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot != NULL && dot[1] != '\0' ? dot + 1 : "";
}

static int extension_is(const char *actual, const char *const *allowed, size_t count)
{
    for (size_t i = 0; i < count; i++)
        if (strcasecmp(actual, allowed[i]) == 0)
            return 1;
    return 0;
}

static int eligible_entry(const char *system_id, const char *name)
{
    static const char *gb[] = {"gb"};
    static const char *gbc[] = {"gbc"};
    static const char *gba[] = {"gba"};
    static const char *nes[] = {"nes"};
    static const char *fds[] = {"fds"};
    static const char *snes[] = {"sfc", "smc", "fig", "bs"};
    static const char *genesis[] = {"md", "gen", "bin"};
    static const char *gamegear[] = {"gg"};
    static const char *mastersystem[] = {"sms"};
    static const char *sg1000[] = {"sg"};
    static const char *atari2600[] = {"a26", "bin"};
    static const char *atari7800[] = {"a78"};
    static const char *lynx[] = {"lnx"};
    static const char *pce[] = {"pce"};
    static const char *wonderswan[] = {"ws", "wsc"};
    static const char *ngpc[] = {"ngc"};
    static const char *virtualboy[] = {"vb"};
    const char *ext = extension(name);
#define MATCH(system, list) \
    if (strcmp(system_id, system) == 0) return extension_is(ext, list, sizeof(list) / sizeof(list[0]))
    MATCH("gb", gb);
    MATCH("gbc", gbc);
    MATCH("gba", gba);
    MATCH("nes", nes);
    MATCH("fds", fds);
    MATCH("snes", snes);
    MATCH("genesis", genesis);
    MATCH("gamegear", gamegear);
    MATCH("mastersystem", mastersystem);
    MATCH("sg1000", sg1000);
    MATCH("atari2600", atari2600);
    MATCH("atari7800", atari7800);
    MATCH("lynx", lynx);
    MATCH("pce", pce);
    MATCH("supergrafx", pce);
    MATCH("wonderswan", wonderswan);
    MATCH("ngpc", ngpc);
    MATCH("virtualboy", virtualboy);
#undef MATCH
    return 0;
}

int bloom_ra_archive_read_rom(const char *system_id, const char *archive_path, char *entry_name,
                              size_t entry_name_size, uint8_t **content, size_t *content_size,
                              char *error, size_t error_size)
{
    if (system_id == NULL || archive_path == NULL || entry_name == NULL || entry_name_size == 0 ||
        content == NULL || content_size == NULL) {
        set_error(error, error_size, "invalid archive request");
        return -1;
    }
    *content = NULL;
    *content_size = 0;
    unzFile archive = unzOpen64(archive_path);
    if (archive == NULL) {
        set_error(error, error_size, "ROM archive is invalid");
        return -1;
    }
    int selected = 0;
    unz_file_info64 selected_info;
    memset(&selected_info, 0, sizeof(selected_info));
    int step = unzGoToFirstFile(archive);
    while (step == UNZ_OK) {
        unz_file_info64 info;
        char name[512] = {0};
        if (unzGetCurrentFileInfo64(archive, &info, name, sizeof(name), NULL, 0, NULL, 0) != UNZ_OK) {
            set_error(error, error_size, "ROM archive metadata is invalid");
            unzClose(archive);
            return -1;
        }
        if (info.size_filename >= sizeof(name)) {
            set_error(error, error_size, "ROM archive filename is too long");
            unzClose(archive);
            return -1;
        }
        if (strchr(name, '/') != NULL || strchr(name, '\\') != NULL || strstr(name, "..") != NULL) {
            set_error(error, error_size, "ROM archive contains an unsafe path");
            unzClose(archive);
            return -1;
        }
        if (eligible_entry(system_id, name)) {
            if (selected || info.uncompressed_size == 0 || info.uncompressed_size > BLOOM_RA_ARCHIVE_MAX_ROM_SIZE ||
                (info.flag & 1) != 0 || strlen(name) >= entry_name_size) {
                set_error(error, error_size, selected ? "ROM archive is ambiguous" : "ROM archive entry is unsupported");
                unzClose(archive);
                return -1;
            }
            selected = 1;
            selected_info = info;
            snprintf(entry_name, entry_name_size, "%s", name);
        }
        step = unzGoToNextFile(archive);
    }
    if (step != UNZ_END_OF_LIST_OF_FILE || !selected) {
        set_error(error, error_size, !selected ? "ROM archive has no supported game entry" : "ROM archive is invalid");
        unzClose(archive);
        return -1;
    }
    if (unzLocateFile(archive, entry_name, 1) != UNZ_OK || unzOpenCurrentFile(archive) != UNZ_OK) {
        set_error(error, error_size, "ROM archive entry could not be opened");
        unzClose(archive);
        return -1;
    }
    uint8_t *buffer = (uint8_t *)malloc((size_t)selected_info.uncompressed_size);
    size_t offset = 0;
    while (buffer != NULL && offset < (size_t)selected_info.uncompressed_size) {
        int count = unzReadCurrentFile(archive, buffer + offset,
                                       (unsigned int)((size_t)selected_info.uncompressed_size - offset));
        if (count <= 0)
            break;
        offset += (size_t)count;
    }
    int close_result = unzCloseCurrentFile(archive);
    unzClose(archive);
    if (buffer == NULL || offset != (size_t)selected_info.uncompressed_size || close_result != UNZ_OK) {
        free(buffer);
        set_error(error, error_size, "ROM archive extraction failed");
        return -1;
    }
    *content = buffer;
    *content_size = offset;
    return 0;
}
