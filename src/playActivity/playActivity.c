#include "./playActivity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void printUsage()
{
    printf("Usage: playActivity list             -> List all play activities\n"
           "       playActivity start [rom_path] -> Launch the counter for this rom\n"
           "       playActivity resume           -> Resume the last rom as a new play activity\n"
           "       playActivity stop [rom_path]  -> Stop the counter for this rom\n"
           "       playActivity stop_all         -> Stop the counter for all roms\n"
           "       playActivity migrate          -> Migrate the old database (prior to Onion 4.2.0) to SQLite\n"
           "       playActivity fix_paths        -> Change all absolute paths to relative paths\n"
           "       playActivity schema           -> Print the Bloom database schema version\n"
           "       playActivity backfill-game-ids [--dry-run] -> Add deterministic canonical identities\n"
           "       playActivity health           -> Run read-only database health checks\n");
}

int main(int argc, char *argv[])
{
    log_setName("play_activity");

    if (argc <= 1) {
        printUsage();
        return EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "start") == 0) {
            if (i + 1 < argc) {
                play_activity_start(argv[++i]);
            }
            else {
                printf("Error: Missing rom_path argument\n");
                printUsage();
                return EXIT_FAILURE;
            }
        }
        else if (strcmp(argv[i], "resume") == 0) {
            play_activity_resume();
        }
        else if (strcmp(argv[i], "stop") == 0) {
            if (i + 1 < argc) {
                play_activity_stop(argv[++i]);
            }
            else {
                printf("Error: Missing rom_path argument\n");
                printUsage();
                return EXIT_FAILURE;
            }
        }
        else if (strcmp(argv[i], "stop_all") == 0) {
            play_activity_stop_all();
        }
        else if (strcmp(argv[i], "migrate") == 0) {
            migrateDB();
        }
        else if (strcmp(argv[i], "fix_paths") == 0) {
            play_activity_fix_paths();
        }
        else if (strcmp(argv[i], "list") == 0) {
            play_activity_list_all();
        }
        else if (strcmp(argv[i], "schema") == 0) {
            play_activity_db_open();
            if (play_activity_db == NULL) {
                fprintf(stderr, "Cannot open play activity database\n");
                return EXIT_FAILURE;
            }
            int version = 0;
            int result = play_activity_schema_version(play_activity_db, &version);
            play_activity_db_close();
            if (result != SQLITE_OK) {
                fprintf(stderr, "Cannot read play activity schema: %s\n", sqlite3_errstr(result));
                return EXIT_FAILURE;
            }
            printf("{\"schema\":1,\"database_schema_version\":%d}\n", version);
        }
        else if (strcmp(argv[i], "backfill-game-ids") == 0) {
            int dry_run = 0;
            if (i + 1 < argc && strcmp(argv[i + 1], "--dry-run") == 0) {
                dry_run = 1;
                i++;
            }
            play_activity_db_open();
            if (play_activity_db == NULL) {
                fprintf(stderr, "Cannot open play activity database\n");
                return EXIT_FAILURE;
            }
            struct play_activity_identity_result identity_result;
            int result = play_activity_backfill_game_ids(play_activity_db, dry_run, &identity_result);
            play_activity_db_close();
            if (result != SQLITE_OK) {
                fprintf(stderr, "Cannot backfill Play Activity GameIDs: %s\n", sqlite3_errstr(result));
                return EXIT_FAILURE;
            }
            printf("{\"schema\":1,\"mode\":\"%s\",\"eligible\":%d,\"updated\":%d,\"deferred\":%d}\n",
                   dry_run ? "dry-run" : "apply", identity_result.updated, dry_run ? 0 : identity_result.updated,
                   identity_result.deferred);
        }
        else if (strcmp(argv[i], "health") == 0) {
            play_activity_db_open();
            if (play_activity_db == NULL) {
                fprintf(stderr, "Cannot open play activity database\n");
                return EXIT_FAILURE;
            }
            struct play_activity_health health;
            int result = play_activity_health_check(play_activity_db, &health);
            play_activity_db_close();
            if (result != SQLITE_OK) {
                fprintf(stderr, "Cannot check Play Activity health: %s\n", sqlite3_errstr(result));
                return EXIT_FAILURE;
            }
            int healthy = health.quick_check_ok && health.orphan_activities == 0 && health.negative_durations == 0;
            printf("{\"schema\":1,\"healthy\":%s,\"database_schema_version\":%d,\"quick_check\":\"%s\","
                   "\"orphan_activities\":%d,\"negative_durations\":%d,\"active_sessions\":%d,"
                   "\"unidentified_roms\":%d}\n",
                   healthy ? "true" : "false", health.schema_version, health.quick_check_ok ? "ok" : "failed",
                   health.orphan_activities, health.negative_durations, health.active_sessions,
                   health.unidentified_roms);
            if (!healthy)
                return EXIT_FAILURE;
        }
        else {
            printf("Error: Invalid argument '%s'\n", argv[1]);
            printUsage();
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
