#include <gtest/gtest.h>

#include <filesystem>
#include <set>
#include <string>

#include <sqlite3/sqlite3.h>
#include <unistd.h>

extern "C" {
#include "../src/bloomGameId/bloom_game_id.h"
#include "../src/bloomLibrary/bloom_library_database.h"
#include "../src/bloomLibrary/bloom_library_query.h"
}

class BloomLibraryQueryTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root_ = std::filesystem::temp_directory_path() /
                ("bloom-library-query-" + std::to_string(getpid()) + "-" +
                 std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::create_directories(root_);
        ASSERT_EQ(SQLITE_OK,
                  bloom_library_database_open((root_ / "catalog.sqlite3").c_str(), &database_));
        execute("INSERT INTO systems VALUES"
                "('gba','GBA','GBA',NULL,'launch','gba',1,2,1),"
                "('gb','GB','GB',NULL,'launch','gb',1,2,1)");
    }

    void TearDown() override
    {
        if (database_ != nullptr)
            sqlite3_close(database_);
        std::filesystem::remove_all(root_);
    }

    void execute(const char *sql)
    {
        ASSERT_EQ(SQLITE_OK, sqlite3_exec(database_, sql, nullptr, nullptr, nullptr));
    }

    std::string add_game(const char *system, const char *path, const char *title, const char *sort,
                         int present = 1, const char *image = nullptr)
    {
        char absolute[640];
        snprintf(absolute, sizeof(absolute), "/mnt/SDCARD/Roms/%s", path);
        char game_id[BLOOM_GAME_ID_LENGTH + 1];
        char normalized[512];
        char error[128];
        EXPECT_EQ(0, bloom_game_id_create(system, absolute, game_id, sizeof(game_id), normalized,
                                          sizeof(normalized), error, sizeof(error)));
        sqlite3_stmt *statement = nullptr;
        int sql = sqlite3_prepare_v2(database_,
                                     "INSERT INTO games VALUES(?1,?2,?3,?4,?5,?6,7,8,?7)", -1,
                                     &statement, nullptr);
        EXPECT_EQ(SQLITE_OK, sql);
        if (sql != SQLITE_OK)
            return {};
        sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, system, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, path, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 4, title, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 5, sort, -1, SQLITE_TRANSIENT);
        if (image == nullptr)
            sqlite3_bind_null(statement, 6);
        else
            sqlite3_bind_text(statement, 6, image, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 7, present);
        EXPECT_EQ(SQLITE_DONE, sqlite3_step(statement));
        sqlite3_finalize(statement);
        return game_id;
    }

    std::filesystem::path root_;
    sqlite3 *database_ = nullptr;
};

TEST_F(BloomLibraryQueryTest, PagesEveryPresentGameWithoutDuplicates)
{
    std::set<std::string> expected = {
        add_game("gba", "GBA/Zoo.gba", "Zoo", "zoo"),
        add_game("gba", "GBA/Alpha.gba", "Alpha", "alpha", 1, "GBA/Imgs/Alpha.png"),
        add_game("gba", "GBA/Alpha Two.gba", "Alpha", "alpha"),
        add_game("gb", "GB/Beta.gb", "Beta", "beta"),
        add_game("gb", "GB/Gamma.gb", "Gamma", "gamma"),
    };
    add_game("gba", "GBA/Gone.gba", "Gone", "gone", 0);
    std::set<std::string> observed;
    std::string cursor;
    do {
        BloomLibraryGame games[2]{};
        BloomLibraryGamePage page{};
        ASSERT_EQ(SQLITE_OK,
                  bloom_library_query_games(database_, nullptr,
                                            cursor.empty() ? nullptr : cursor.c_str(), 2, games, 2,
                                            &page));
        ASSERT_GT(page.count, 0U);
        for (size_t index = 0; index < page.count; ++index)
            EXPECT_TRUE(observed.insert(games[index].bloom_game_id).second);
        cursor = page.has_more ? page.next_cursor : "";
        if (!page.has_more)
            break;
    } while (true);
    EXPECT_EQ(expected, observed);
}

TEST_F(BloomLibraryQueryTest, SystemFilterAndMetadataAreBounded)
{
    std::string first = add_game("gba", "GBA/Alpha.gba", "Alpha", "alpha", 1,
                                 "GBA/Imgs/Alpha.png");
    add_game("gba", "GBA/Beta.gba", "Beta", "beta");
    add_game("gb", "GB/Mono.gb", "Mono", "mono");
    BloomLibraryGame games[2]{};
    BloomLibraryGamePage page{};
    ASSERT_EQ(SQLITE_OK,
              bloom_library_query_games(database_, "gba", nullptr, 2, games, 2, &page));
    EXPECT_EQ(2U, page.count);
    EXPECT_FALSE(page.has_more);
    EXPECT_EQ(first, games[0].bloom_game_id);
    EXPECT_STREQ("GBA/Alpha.gba", games[0].normalized_rom_path);
    EXPECT_STREQ("GBA/Imgs/Alpha.png", games[0].image_path);
    EXPECT_STREQ("launch", games[0].launch_path);
    EXPECT_EQ(7, games[0].file_size);
    EXPECT_EQ(8, games[0].file_mtime);
}

TEST_F(BloomLibraryQueryTest, InvalidAndStaleCursorsFailClosed)
{
    std::string stale = add_game("gba", "GBA/Gone.gba", "Gone", "gone", 0);
    BloomLibraryGame game{};
    BloomLibraryGamePage page{};
    EXPECT_EQ(SQLITE_MISUSE,
              bloom_library_query_games(database_, "../gba", nullptr, 1, &game, 1, &page));
    EXPECT_EQ(SQLITE_MISUSE,
              bloom_library_query_games(database_, nullptr, "bad", 1, &game, 1, &page));
    EXPECT_EQ(SQLITE_MISUSE,
              bloom_library_query_games(database_, nullptr, nullptr, 0, &game, 1, &page));
    EXPECT_EQ(SQLITE_MISUSE,
              bloom_library_query_games(database_, nullptr, nullptr, 101, &game, 1, &page));
    EXPECT_EQ(SQLITE_NOTFOUND,
              bloom_library_query_games(database_, nullptr, stale.c_str(), 1, &game, 1, &page));
}

TEST_F(BloomLibraryQueryTest, RecentsPreserveCanonicalOrderAndFilterMissingGames)
{
    std::string newest = add_game("gb", "GB/New.gb", "New", "new");
    std::string second = add_game("gba", "GBA/Second.gba", "Second", "second");
    std::string older = add_game("gb", "GB/Older.gb", "Older", "older");
    std::string missing = add_game("gb", "GB/Missing.gb", "Missing", "missing", 0);
    execute(("INSERT INTO recents VALUES('" + newest + "',0),('" + second +
             "',1),('" + missing + "',2),('" + older + "',3)")
                .c_str());

    BloomLibraryGame games[4]{};
    size_t count = 0;
    ASSERT_EQ(SQLITE_OK,
              bloom_library_query_recents(database_, nullptr, 4, games, 4, &count));
    ASSERT_EQ(3U, count);
    EXPECT_EQ(newest, games[0].bloom_game_id);
    EXPECT_EQ(second, games[1].bloom_game_id);
    EXPECT_EQ(older, games[2].bloom_game_id);

    ASSERT_EQ(SQLITE_OK,
              bloom_library_query_recents(database_, "gb", 4, games, 4, &count));
    ASSERT_EQ(2U, count);
    EXPECT_EQ(newest, games[0].bloom_game_id);
    EXPECT_EQ(older, games[1].bloom_game_id);
}

TEST_F(BloomLibraryQueryTest, RecentsRejectUnboundedOrUnsafeRequests)
{
    BloomLibraryGame game{};
    size_t count = 7;
    EXPECT_EQ(SQLITE_MISUSE,
              bloom_library_query_recents(database_, "../gb", 1, &game, 1, &count));
    EXPECT_EQ(SQLITE_MISUSE,
              bloom_library_query_recents(database_, nullptr, 0, &game, 1, &count));
    EXPECT_EQ(SQLITE_MISUSE,
              bloom_library_query_recents(database_, nullptr, 101, &game, 101, &count));
    EXPECT_EQ(SQLITE_MISUSE,
              bloom_library_query_favorites(database_, "../gb", 1, &game, 1, &count));
    EXPECT_EQ(SQLITE_MISUSE,
              bloom_library_query_favorites(database_, nullptr, 101, &game, 101, &count));
}

TEST_F(BloomLibraryQueryTest, FavoritesPreserveCanonicalOrderAndSystemFilter)
{
    std::string first = add_game("gba", "GBA/First.gba", "First", "first");
    std::string second = add_game("gb", "GB/Second.gb", "Second", "second");
    std::string third = add_game("gb", "GB/Third.gb", "Third", "third");
    execute(("INSERT INTO favorites VALUES('" + first + "',0),('" + second + "',1),('" +
             third + "',2)")
                .c_str());

    BloomLibraryGame games[3]{};
    size_t count = 0;
    ASSERT_EQ(SQLITE_OK,
              bloom_library_query_favorites(database_, "gb", 3, games, 3, &count));
    ASSERT_EQ(2U, count);
    EXPECT_EQ(second, games[0].bloom_game_id);
    EXPECT_EQ(third, games[1].bloom_game_id);
}

TEST_F(BloomLibraryQueryTest, AppsAreBoundedSortedAndPresentOnly)
{
    execute("INSERT INTO apps VALUES"
            "('settings','Settings','App/Settings/launch.sh',NULL,1,2,1),"
            "('activity','Activity','App/Activity/launch.sh','activity.png',1,2,1),"
            "('gone','Gone','App/Gone/launch.sh',NULL,1,2,0)");
    BloomLibraryApp apps[3]{};
    size_t count = 0;
    ASSERT_EQ(SQLITE_OK, bloom_library_query_apps(database_, 3, apps, 3, &count));
    ASSERT_EQ(2U, count);
    EXPECT_STREQ("activity", apps[0].app_id);
    EXPECT_STREQ("Activity", apps[0].label);
    EXPECT_STREQ("App/Activity/launch.sh", apps[0].launch_path);
    EXPECT_STREQ("activity.png", apps[0].icon_path);
    EXPECT_STREQ("settings", apps[1].app_id);
    EXPECT_STREQ("", apps[1].icon_path);
}

TEST_F(BloomLibraryQueryTest, AppsRejectUnboundedRequestsAndMalformedRows)
{
    BloomLibraryApp app{};
    size_t count = 7;
    EXPECT_EQ(SQLITE_MISUSE, bloom_library_query_apps(database_, 0, &app, 1, &count));
    EXPECT_EQ(SQLITE_MISUSE, bloom_library_query_apps(database_, 101, &app, 1, &count));
    execute("INSERT INTO apps VALUES('bad','Bad','',NULL,1,2,1)");
    EXPECT_EQ(SQLITE_CORRUPT, bloom_library_query_apps(database_, 1, &app, 1, &count));
}
