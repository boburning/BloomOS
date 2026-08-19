#include "bloom_ra_catalog.h"

#include <cjson/cJSON.h>

#include <string.h>

static int bind_game(sqlite3_stmt *statement, int id, int console_id, const char *title, int achievements)
{
    sqlite3_bind_int(statement, 1, id);
    sqlite3_bind_int(statement, 2, console_id);
    sqlite3_bind_text(statement, 3, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 4, achievements);
    int result = sqlite3_step(statement);
    sqlite3_reset(statement);
    sqlite3_clear_bindings(statement);
    return result == SQLITE_DONE ? SQLITE_OK : result;
}

static int bind_hash(sqlite3_stmt *statement, int console_id, const char *hash, int game_id)
{
    if (strlen(hash) != 32)
        return SQLITE_CORRUPT;
    for (const unsigned char *character = (const unsigned char *)hash; *character; character++)
        if (!((*character >= '0' && *character <= '9') || (*character >= 'a' && *character <= 'f')))
            return SQLITE_CORRUPT;
    sqlite3_bind_int(statement, 1, console_id);
    sqlite3_bind_text(statement, 2, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 3, game_id);
    int result = sqlite3_step(statement);
    sqlite3_reset(statement);
    sqlite3_clear_bindings(statement);
    return result == SQLITE_DONE ? SQLITE_OK : result;
}

static int official_import_console(sqlite3 *database, int console_id, const char *revision, const char *json)
{
    if (database == NULL || console_id <= 0 || revision == NULL || revision[0] == '\0' || json == NULL)
        return SQLITE_MISUSE;
    cJSON *root = cJSON_Parse(json);
    if (!cJSON_IsArray(root) || cJSON_GetArraySize(root) == 0) {
        cJSON_Delete(root);
        return SQLITE_CORRUPT;
    }
    int result = sqlite3_exec(database, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    sqlite3_stmt *game_statement = NULL;
    sqlite3_stmt *hash_statement = NULL;
    if (result == SQLITE_OK) {
        sqlite3_stmt *delete_statement = NULL;
        result = sqlite3_prepare_v2(database, "DELETE FROM ra_hashes WHERE ra_console_id=?1", -1, &delete_statement, NULL);
        if (result == SQLITE_OK) {
            sqlite3_bind_int(delete_statement, 1, console_id);
            result = sqlite3_step(delete_statement) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(database);
        }
        sqlite3_finalize(delete_statement);
    }
    if (result == SQLITE_OK)
        result = sqlite3_prepare_v2(database,
                                    "INSERT INTO ra_games(ra_game_id,ra_console_id,title,official_set,achievement_count) "
                                    "VALUES(?1,?2,?3,1,?4) ON CONFLICT(ra_game_id) DO UPDATE SET "
                                    "ra_console_id=excluded.ra_console_id,title=excluded.title,official_set=1,"
                                    "achievement_count=excluded.achievement_count",
                                    -1, &game_statement, NULL);
    if (result == SQLITE_OK)
        result = sqlite3_prepare_v2(database,
                                    "INSERT INTO ra_hashes(ra_console_id,ra_content_hash,ra_game_id) VALUES(?1,?2,?3)",
                                    -1, &hash_statement, NULL);
    int imported = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root)
    {
        cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "ID");
        cJSON *item_console = cJSON_GetObjectItemCaseSensitive(item, "ConsoleID");
        cJSON *title = cJSON_GetObjectItemCaseSensitive(item, "Title");
        cJSON *achievements = cJSON_GetObjectItemCaseSensitive(item, "NumAchievements");
        cJSON *hashes = cJSON_GetObjectItemCaseSensitive(item, "Hashes");
        if (result != SQLITE_OK)
            break;
        if (!cJSON_IsNumber(id) || !cJSON_IsNumber(item_console) || item_console->valueint != console_id ||
            !cJSON_IsString(title) || title->valuestring[0] == '\0' || !cJSON_IsNumber(achievements) ||
            achievements->valueint <= 0 || !cJSON_IsArray(hashes) || cJSON_GetArraySize(hashes) == 0) {
            result = SQLITE_CORRUPT;
            break;
        }
        result = bind_game(game_statement, id->valueint, console_id, title->valuestring, achievements->valueint);
        cJSON *hash = NULL;
        cJSON_ArrayForEach(hash, hashes)
        {
            if (result != SQLITE_OK || !cJSON_IsString(hash)) {
                result = SQLITE_CORRUPT;
                break;
            }
            result = bind_hash(hash_statement, console_id, hash->valuestring, id->valueint);
        }
        imported++;
    }
    sqlite3_finalize(hash_statement);
    sqlite3_finalize(game_statement);
    if (result == SQLITE_OK) {
        sqlite3_stmt *cleanup = NULL;
        result = sqlite3_prepare_v2(
            database,
            "DELETE FROM ra_games WHERE ra_console_id=?1 "
            "AND NOT EXISTS(SELECT 1 FROM ra_hashes h WHERE h.ra_game_id=ra_games.ra_game_id) "
            "AND NOT EXISTS(SELECT 1 FROM library_games l WHERE l.ra_game_id=ra_games.ra_game_id)",
            -1, &cleanup, NULL);
        if (result == SQLITE_OK) {
            sqlite3_bind_int(cleanup, 1, console_id);
            result = sqlite3_step(cleanup) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(database);
        }
        sqlite3_finalize(cleanup);
    }
    if (result == SQLITE_OK && imported > 0) {
        sqlite3_stmt *state = NULL;
        result = sqlite3_prepare_v2(
            database,
            "UPDATE catalog_state SET catalog_generation=catalog_generation+1,provider='ra_web_game_list',"
            "provider_revision=?1,status='ready',refreshed_at=strftime('%s','now'),last_success_at=strftime('%s','now')",
            -1, &state, NULL);
        if (result == SQLITE_OK) {
            sqlite3_bind_text(state, 1, revision, -1, SQLITE_TRANSIENT);
            result = sqlite3_step(state) == SQLITE_DONE ? SQLITE_OK : sqlite3_errcode(database);
        }
        sqlite3_finalize(state);
    }
    if (result == SQLITE_OK)
        result = sqlite3_exec(database, "COMMIT", NULL, NULL, NULL);
    if (result != SQLITE_OK)
        sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
    cJSON_Delete(root);
    return result;
}

const BloomRaCatalogProvider *bloom_ra_official_catalog_provider(void)
{
    static const BloomRaCatalogProvider provider = {"ra_web_game_list", official_import_console};
    return &provider;
}

int bloom_ra_catalog_resolve(sqlite3 *database, int console_id, const char *content_hash, int *ra_game_id,
                             int *achievement_count)
{
    if (database == NULL || content_hash == NULL || ra_game_id == NULL || achievement_count == NULL)
        return SQLITE_MISUSE;
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(
        database,
        "SELECT g.ra_game_id,g.achievement_count FROM ra_hashes h JOIN ra_games g ON g.ra_game_id=h.ra_game_id "
        "WHERE h.ra_console_id=?1 AND h.ra_content_hash=?2 AND g.official_set=1 AND g.achievement_count>0",
        -1, &statement, NULL);
    if (result == SQLITE_OK) {
        sqlite3_bind_int(statement, 1, console_id);
        sqlite3_bind_text(statement, 2, content_hash, -1, SQLITE_STATIC);
        int step = sqlite3_step(statement);
        if (step == SQLITE_ROW) {
            *ra_game_id = sqlite3_column_int(statement, 0);
            *achievement_count = sqlite3_column_int(statement, 1);
        }
        else {
            result = step == SQLITE_DONE ? SQLITE_NOTFOUND : step;
        }
    }
    sqlite3_finalize(statement);
    return result;
}
