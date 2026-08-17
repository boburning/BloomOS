#include "playActivityModel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROM_ROOT "/mnt/SDCARD/Roms/"

static int ends_with(const char *value, const char *suffix)
{
    size_t value_length = strlen(value);
    size_t suffix_length = strlen(suffix);
    return value_length >= suffix_length && strcmp(value + value_length - suffix_length, suffix) == 0;
}

int play_activity_image_path(const char *rom_file, char *output, size_t output_size)
{
    if (rom_file == NULL || rom_file[0] == '\0' || output == NULL || output_size == 0 || rom_file[0] == '/' ||
        strcmp(rom_file, "..") == 0 || strncmp(rom_file, "../", 3) == 0 || strstr(rom_file, "/../") != NULL ||
        (strlen(rom_file) >= 3 && strcmp(rom_file + strlen(rom_file) - 3, "/..") == 0) ||
        strchr(rom_file, '\\') != NULL)
        return -1;
    if (ends_with(rom_file, ".p8") || ends_with(rom_file, ".png")) {
        int length = snprintf(output, output_size, "%s%s", ROM_ROOT, rom_file);
        return length >= 0 && (size_t)length < output_size ? 0 : -1;
    }
    const char *folder_end = strchr(rom_file, '/');
    const char *basename = strrchr(rom_file, '/');
    if (folder_end == NULL || folder_end == rom_file || basename == NULL || basename[1] == '\0')
        return -1;
    basename++;
    const char *extension = strrchr(basename, '.');
    size_t name_length = extension != NULL && extension != basename ? (size_t)(extension - basename) : strlen(basename);
    int length = snprintf(output, output_size, "%s%.*s/Imgs/%.*s.png", ROM_ROOT, (int)(folder_end - rom_file), rom_file,
                          (int)name_length, basename);
    return length >= 0 && (size_t)length < output_size ? 0 : -1;
}

static char *duplicate_column(sqlite3_stmt *statement, int column)
{
    const unsigned char *value = sqlite3_column_text(statement, column);
    return value == NULL ? NULL : strdup((const char *)value);
}

void play_activity_free(PlayActivity *activity)
{
    if (activity == NULL)
        return;
    if (activity->rom != NULL) {
        free(activity->rom->type);
        free(activity->rom->name);
        free(activity->rom->file_path);
        free(activity->rom->image_path);
        free(activity->rom);
    }
    free(activity->first_played_at);
    free(activity->last_played_at);
    free(activity);
}

PlayActivity *play_activity_read_row(sqlite3_stmt *statement)
{
    if (statement == NULL || sqlite3_column_count(statement) < 9)
        return NULL;
    PlayActivity *activity = calloc(1, sizeof(*activity));
    ROM *rom = calloc(1, sizeof(*rom));
    if (activity == NULL || rom == NULL) {
        free(activity);
        free(rom);
        return NULL;
    }
    activity->rom = rom;
    rom->id = sqlite3_column_int(statement, 0);
    rom->type = duplicate_column(statement, 1);
    rom->name = duplicate_column(statement, 2);
    rom->file_path = duplicate_column(statement, 3);
    if (rom->file_path != NULL) {
        char image_path[4096];
        if (play_activity_image_path(rom->file_path, image_path, sizeof(image_path)) == 0)
            rom->image_path = strdup(image_path);
    }
    activity->play_count = sqlite3_column_int(statement, 4);
    activity->play_time_total = sqlite3_column_int(statement, 5);
    activity->play_time_average = sqlite3_column_int(statement, 6);
    activity->first_played_at = duplicate_column(statement, 7);
    activity->last_played_at = duplicate_column(statement, 8);
    if ((sqlite3_column_type(statement, 1) != SQLITE_NULL && rom->type == NULL) ||
        (sqlite3_column_type(statement, 2) != SQLITE_NULL && rom->name == NULL) ||
        (sqlite3_column_type(statement, 3) != SQLITE_NULL && rom->file_path == NULL) ||
        (sqlite3_column_type(statement, 7) != SQLITE_NULL && activity->first_played_at == NULL) ||
        (sqlite3_column_type(statement, 8) != SQLITE_NULL && activity->last_played_at == NULL)) {
        play_activity_free(activity);
        return NULL;
    }
    return activity;
}
