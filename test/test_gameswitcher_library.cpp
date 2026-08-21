#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <sqlite3/sqlite3.h>
#include <unistd.h>

extern "C" {
#include "../src/bloomGameId/bloom_game_id.h"
#include "../src/bloomLibrary/bloom_library_database.h"
#include "../src/gameSwitcher/gameSwitcherLibrary.h"
}

class GameSwitcherLibraryTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root_ = std::filesystem::temp_directory_path() /
                ("bloom-gameswitcher-library-" + std::to_string(getpid()) + "-" +
                 std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::create_directories(root_);
        database_path_ = root_ / "catalog.sqlite3";
        ASSERT_EQ(SQLITE_OK, bloom_library_database_open(database_path_.c_str(), &database_));
        ASSERT_EQ(SQLITE_OK,
                  sqlite3_exec(database_,
                               "INSERT INTO systems(system_id,label,rom_path,img_path,launch_path,"
                               "extensions,config_size,config_mtime,present) VALUES"
                               "('gb','Game Boy','GB','GB/Imgs','Emu/GB/launch.sh','.gb',1,1,1);",
                               nullptr, nullptr, nullptr));
        add_game("One", "GB/One.gb", 0);
        add_game("Two", "GB/Two.gb", 1);
        sqlite3_close(database_);
        database_ = nullptr;
    }

    void TearDown() override
    {
        if (database_ != nullptr)
            sqlite3_close(database_);
        std::filesystem::remove_all(root_);
    }

    void add_game(const char *title, const char *path, int position)
    {
        char game_id[BLOOM_GAME_ID_LENGTH + 1] = {};
        char normalized[512] = {};
        char error[128] = {};
        std::string absolute = "/mnt/SDCARD/Roms/" + std::string(path);
        ASSERT_EQ(0, bloom_game_id_create("gb", absolute.c_str(), game_id, sizeof(game_id),
                                          normalized, sizeof(normalized), error, sizeof(error)));
        sqlite3_stmt *statement = nullptr;
        ASSERT_EQ(SQLITE_OK,
                  sqlite3_prepare_v2(database_,
                                     "INSERT INTO games(bloom_game_id,system_id,normalized_rom_path,"
                                     "display_title,sort_title,image_path,file_size,file_mtime,present)"
                                     "VALUES(?1,'gb',?2,?3,?3,?4,1,1,1)",
                                     -1, &statement, nullptr));
        sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, path, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, title, -1, SQLITE_TRANSIENT);
        std::string image = std::string("GB/Imgs/") + title + ".png";
        sqlite3_bind_text(statement, 4, image.c_str(), -1, SQLITE_TRANSIENT);
        ASSERT_EQ(SQLITE_DONE, sqlite3_step(statement));
        sqlite3_finalize(statement);
        ASSERT_EQ(SQLITE_OK,
                  sqlite3_prepare_v2(database_,
                                     "INSERT INTO recents(bloom_game_id,position) VALUES(?1,?2)",
                                     -1, &statement, nullptr));
        sqlite3_bind_text(statement, 1, game_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 2, position);
        ASSERT_EQ(SQLITE_DONE, sqlite3_step(statement));
        sqlite3_finalize(statement);
    }

    std::filesystem::path root_;
    std::filesystem::path database_path_;
    sqlite3 *database_ = nullptr;
};

TEST_F(GameSwitcherLibraryTest, ReadsCanonicalOrderAndRemovesWithoutLegacyState)
{
    GameSwitcherLibraryRecent recents[2] = {};
    size_t count = 0;
    ASSERT_EQ(0, gameswitcher_library_read_recents(database_path_.c_str(), 2, recents, 2, &count));
    ASSERT_EQ(2U, count);
    EXPECT_STREQ("One", recents[0].label);
    EXPECT_STREQ("/mnt/SDCARD/Roms/GB/One.gb", recents[0].rom_path);
    EXPECT_STREQ("/mnt/SDCARD/Roms/GB/Imgs/One.png", recents[0].image_path);
    EXPECT_STREQ("/mnt/SDCARD/Emu/GB/launch.sh", recents[0].launcher);

    const std::string removed_id = recents[0].game_id;
    ASSERT_EQ(0, gameswitcher_library_remove_recent(database_path_.c_str(), recents[0].game_id));
    ASSERT_EQ(0, gameswitcher_library_read_recents(database_path_.c_str(), 2, recents, 2, &count));
    ASSERT_EQ(1U, count);
    EXPECT_STREQ("Two", recents[0].label);
    EXPECT_NE(0, gameswitcher_library_remove_recent(database_path_.c_str(), removed_id.c_str()));
}

TEST_F(GameSwitcherLibraryTest, RejectsUnsafeOrUnboundedRequests)
{
    GameSwitcherLibraryRecent recent = {};
    size_t count = 0;
    EXPECT_NE(0, gameswitcher_library_read_recents(database_path_.c_str(), 0, &recent, 1, &count));
    EXPECT_NE(0, gameswitcher_library_read_recents(database_path_.c_str(), 2, &recent, 1, &count));
    EXPECT_NE(0, gameswitcher_library_remove_recent(database_path_.c_str(), "invalid"));
}
