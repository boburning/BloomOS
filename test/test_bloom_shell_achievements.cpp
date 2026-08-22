#include "gtest/gtest.h"

extern "C" {
#include "../src/bloomShell/bloom_shell_achievements.h"
}

#include <filesystem>
#include <sqlite3/sqlite3.h>
#include <string>
#include <unistd.h>

namespace {
constexpr const char *supported =
    "bloom-game-v1:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr const char *unsupported =
    "bloom-game-v1:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
} // namespace

TEST(BloomShellAchievements, LoadsOnlyExactOfficialSupportedGames)
{
    auto path = std::filesystem::temp_directory_path() /
                ("bloom-shell-ra-" + std::to_string(getpid()) + ".sqlite3");
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(path.c_str(), &database));
    ASSERT_EQ(SQLITE_OK,
              sqlite3_exec(database,
                           "CREATE TABLE library_games(bloom_game_id TEXT PRIMARY KEY,"
                           "ra_game_id INTEGER,achievement_count INTEGER,official_set INTEGER,"
                           "status TEXT);",
                           nullptr, nullptr, nullptr));
    std::string insert =
        "INSERT INTO library_games VALUES('" + std::string(supported) +
        "',123,4,1,'identified'),('" + std::string(unsupported) +
        "',456,0,1,'identified');";
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(database, insert.c_str(), nullptr, nullptr, nullptr));
    sqlite3_close(database);

    BloomShellAchievementIndex index = {};
    ASSERT_EQ(0, bloom_shell_achievements_load(&index, path.c_str(), 1));
    EXPECT_EQ(1U, index.count);
    EXPECT_EQ(1, bloom_shell_achievements_contains(&index, supported));
    EXPECT_EQ(0, bloom_shell_achievements_contains(&index, unsupported));
    bloom_shell_achievements_destroy(&index);
    std::filesystem::remove(path);
}

TEST(BloomShellAchievements, MissingOrMalformedIndexNeverBlocksBrowsing)
{
    BloomShellAchievementIndex index = {};
    ASSERT_EQ(0, bloom_shell_achievements_load(&index, "/tmp/bloom-missing-ra-index", 8));
    EXPECT_EQ(0U, index.count);

    auto path = std::filesystem::temp_directory_path() /
                ("bloom-shell-ra-malformed-" + std::to_string(getpid()) + ".sqlite3");
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(path.c_str(), &database));
    sqlite3_close(database);
    ASSERT_EQ(0, bloom_shell_achievements_load(&index, path.c_str(), 8));
    EXPECT_EQ(0U, index.count);
    bloom_shell_achievements_destroy(&index);
    std::filesystem::remove(path);
}
