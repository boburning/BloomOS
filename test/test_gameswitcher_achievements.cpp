#include <gtest/gtest.h>

#include <filesystem>
#include <sqlite3.h>
#include <unistd.h>

extern "C" {
#include "../src/gameSwitcher/gameSwitcherAchievements.h"
}

namespace {
const char *game_id = "bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

void exec(sqlite3 *database, const char *sql)
{
    char *message = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(database, sql, nullptr, nullptr, &message)) << (message ? message : "");
    sqlite3_free(message);
}
} // namespace

TEST(GameSwitcherAchievements, ReadsOnlyExactOfficialGameBadgeMetadata)
{
    char path[] = "/tmp/bloom-gs-ra-XXXXXX";
    int descriptor = mkstemp(path);
    ASSERT_GE(descriptor, 0);
    close(descriptor);
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(path, &database));
    exec(database,
         "CREATE TABLE library_games(bloom_game_id TEXT PRIMARY KEY,ra_game_id INTEGER,official_set INTEGER,"
         "achievement_count INTEGER);"
         "INSERT INTO library_games VALUES('bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef',"
         "1234,1,42);");
    sqlite3_close(database);

    GameSwitcherAchievements achievements = {};
    char error[128] = {};
    ASSERT_EQ(0, gameswitcher_achievements_lookup(path, game_id, &achievements, error, sizeof(error))) << error;
    EXPECT_EQ(1, achievements.has_ra_badge);
    EXPECT_EQ(1234, achievements.ra_game_id);
    EXPECT_EQ(42UL, achievements.achievement_count);
    gameswitcher_achievements_close();
    std::filesystem::remove(path);
}

TEST(GameSwitcherAchievements, FailsClosedForUnqualifiedOrMissingMetadata)
{
    GameSwitcherAchievements achievements = {1, 1234, 42};
    EXPECT_EQ(0, gameswitcher_achievements_lookup("/tmp/bloom-no-ra-index", game_id, &achievements, nullptr, 0));
    EXPECT_EQ(0, achievements.has_ra_badge);
    EXPECT_NE(0, gameswitcher_achievements_lookup("/tmp/bloom-no-ra-index", "invalid", &achievements, nullptr, 0));
}
