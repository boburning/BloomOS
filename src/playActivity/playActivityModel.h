#ifndef PLAY_ACTIVITY_MODEL_H
#define PLAY_ACTIVITY_MODEL_H

#include <sqlite3/sqlite3.h>
#include <stddef.h>

typedef struct ROM ROM;
typedef struct PlayActivity PlayActivity;
typedef struct PlayActivities PlayActivities;

struct ROM {
    int id;
    char *type;
    char *name;
    char *file_path;
    char *image_path;
};

struct PlayActivity {
    ROM *rom;
    int play_count;
    int play_time_total;
    int play_time_average;
    char *first_played_at;
    char *last_played_at;
};

struct PlayActivities {
    PlayActivity **play_activity;
    int count;
    int play_time_total;
};

int play_activity_image_path(const char *rom_file, char *output, size_t output_size);
PlayActivity *play_activity_read_row(sqlite3_stmt *statement);
void play_activity_free(PlayActivity *activity);

#endif
