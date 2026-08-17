#include <gtest/gtest.h>

#include <sqlite3/sqlite3.h>

#include <chrono>
#include <filesystem>

extern "C" {
#include "../src/playActivity/playActivityMigrations.h"
}

namespace {

class PlayActivityMigrationTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root_ = std::filesystem::temp_directory_path() /
                ("bloom-activity-migration-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root_);
        ASSERT_EQ(SQLITE_OK, sqlite3_open((root_ / "activity.sqlite").c_str(), &database_));
        ASSERT_EQ(SQLITE_OK,
                  sqlite3_exec(database_,
                               "CREATE TABLE rom(id INTEGER PRIMARY KEY,type TEXT,name TEXT,file_path TEXT,image_path TEXT);"
                               "CREATE TABLE play_activity(rom_id INTEGER,play_time INTEGER,created_at INTEGER,updated_at INTEGER);"
                               "INSERT INTO rom(id,type,name,file_path) VALUES(1,'GB','Test','GB/Test.zip');",
                               nullptr, nullptr, nullptr));
    }

    void TearDown() override
    {
        if (database_ != nullptr)
            sqlite3_close(database_);
        std::filesystem::remove_all(root_);
    }

    int scalar(const char *sql)
    {
        sqlite3_stmt *statement = nullptr;
        EXPECT_EQ(SQLITE_OK, sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr));
        EXPECT_EQ(SQLITE_ROW, sqlite3_step(statement));
        int value = sqlite3_column_int(statement, 0);
        sqlite3_finalize(statement);
        return value;
    }

    std::filesystem::path root_;
    sqlite3 *database_ = nullptr;
};

TEST_F(PlayActivityMigrationTest, AddsVersionHistoryAndNullableCanonicalIdentityWithBackup)
{
    auto backup = root_ / "pre-v1.sqlite";
    ASSERT_EQ(SQLITE_OK, play_activity_schema_migrate(database_, backup.c_str()));
    int version = 0;
    EXPECT_EQ(SQLITE_OK, play_activity_schema_version(database_, &version));
    EXPECT_EQ(1, version);
    EXPECT_EQ(1, scalar("SELECT COUNT(*) FROM migration_history WHERE version=1"));
    EXPECT_EQ(1, scalar("SELECT COUNT(*) FROM pragma_table_info('rom') WHERE name='game_id'"));
    EXPECT_EQ(1, scalar("SELECT COUNT(*) FROM rom WHERE id=1 AND game_id IS NULL"));
    ASSERT_TRUE(std::filesystem::exists(backup));

    sqlite3 *backup_database = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open_v2(backup.c_str(), &backup_database, SQLITE_OPEN_READONLY, nullptr));
    sqlite3_stmt *statement = nullptr;
    ASSERT_EQ(SQLITE_OK,
              sqlite3_prepare_v2(backup_database, "SELECT COUNT(*) FROM pragma_table_info('rom') WHERE name='game_id'",
                                 -1, &statement, nullptr));
    ASSERT_EQ(SQLITE_ROW, sqlite3_step(statement));
    EXPECT_EQ(0, sqlite3_column_int(statement, 0));
    sqlite3_finalize(statement);
    sqlite3_close(backup_database);
}

TEST_F(PlayActivityMigrationTest, IsIdempotent)
{
    auto backup = root_ / "pre-v1.sqlite";
    ASSERT_EQ(SQLITE_OK, play_activity_schema_migrate(database_, backup.c_str()));
    auto backup_size = std::filesystem::file_size(backup);
    ASSERT_EQ(SQLITE_OK, play_activity_schema_migrate(database_, backup.c_str()));
    EXPECT_EQ(1, scalar("SELECT COUNT(*) FROM migration_history"));
    EXPECT_EQ(backup_size, std::filesystem::file_size(backup));
}

TEST_F(PlayActivityMigrationTest, RefusesANewerSchema)
{
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(database_, "CREATE TABLE schema_version(version INTEGER NOT NULL);"
                                                 "INSERT INTO schema_version VALUES(99);",
                                      nullptr, nullptr, nullptr));
    EXPECT_EQ(SQLITE_MISMATCH, play_activity_schema_migrate(database_, nullptr));
}

TEST_F(PlayActivityMigrationTest, RefusesIncompleteVersionOneMetadata)
{
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(database_, "ALTER TABLE rom ADD COLUMN game_id TEXT;"
                                                 "CREATE TABLE schema_version(version INTEGER NOT NULL);"
                                                 "INSERT INTO schema_version VALUES(1);"
                                                 "CREATE TABLE migration_history(version INTEGER PRIMARY KEY,name TEXT);",
                                      nullptr, nullptr, nullptr));
    EXPECT_EQ(SQLITE_CORRUPT, play_activity_schema_migrate(database_, nullptr));
}

} // namespace
