#include <gtest/gtest.h>

#include <sqlite3/sqlite3.h>

extern "C" {
#include "../src/playActivity/playActivityHealth.h"
}

namespace {

class PlayActivityHealthTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        ASSERT_EQ(SQLITE_OK, sqlite3_open(":memory:", &database_));
        ASSERT_EQ(SQLITE_OK,
                  sqlite3_exec(database_,
                               "CREATE TABLE schema_version(version INTEGER NOT NULL);INSERT INTO schema_version VALUES(1);"
                               "CREATE TABLE migration_history(version INTEGER PRIMARY KEY,name TEXT NOT NULL);"
                               "INSERT INTO migration_history VALUES(1,'canonical_game_id_column');"
                               "CREATE TABLE rom(id INTEGER PRIMARY KEY,file_path TEXT,game_id TEXT);"
                               "CREATE TABLE play_activity(rom_id INTEGER,play_time INTEGER);",
                               nullptr, nullptr, nullptr));
    }
    void TearDown() override { sqlite3_close(database_); }
    sqlite3 *database_ = nullptr;
};

TEST_F(PlayActivityHealthTest, ReportsHealthyDatabaseAndOperationalCounts)
{
    ASSERT_EQ(SQLITE_OK,
              sqlite3_exec(database_, "INSERT INTO rom VALUES(1,'GB/Test.zip',NULL);"
                                      "INSERT INTO rom VALUES(2,'GB/Done.zip','bloom-game-v1:test');"
                                      "INSERT INTO play_activity VALUES(1,NULL);"
                                      "INSERT INTO play_activity VALUES(2,30);",
                           nullptr, nullptr, nullptr));
    play_activity_health health = {};
    ASSERT_EQ(SQLITE_OK, play_activity_health_check(database_, &health));
    EXPECT_EQ(1, health.schema_version);
    EXPECT_EQ(1, health.quick_check_ok);
    EXPECT_EQ(0, health.orphan_activities);
    EXPECT_EQ(0, health.negative_durations);
    EXPECT_EQ(1, health.active_sessions);
    EXPECT_EQ(1, health.unidentified_roms);
}

TEST_F(PlayActivityHealthTest, DetectsOrphansAndNegativeDurations)
{
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(database_, "INSERT INTO play_activity VALUES(99,-5);", nullptr, nullptr, nullptr));
    play_activity_health health = {};
    ASSERT_EQ(SQLITE_OK, play_activity_health_check(database_, &health));
    EXPECT_EQ(1, health.orphan_activities);
    EXPECT_EQ(1, health.negative_durations);
}

TEST_F(PlayActivityHealthTest, RefusesIncompleteSchemaMetadata)
{
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(database_, "DELETE FROM migration_history", nullptr, nullptr, nullptr));
    play_activity_health health = {};
    EXPECT_EQ(SQLITE_CORRUPT, play_activity_health_check(database_, &health));
}

} // namespace
