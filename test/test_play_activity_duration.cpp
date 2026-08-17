#include <gtest/gtest.h>
#include <sqlite3.h>

extern "C" {
#include "../src/playActivity/playActivityDuration.h"
}

class PlayActivityDurationTest : public testing::Test {
  protected:
    sqlite3 *database = nullptr;

    void SetUp() override
    {
        ASSERT_EQ(sqlite3_open(":memory:", &database), SQLITE_OK);
        ASSERT_EQ(sqlite3_exec(database,
                               "CREATE TABLE play_activity(rom_id INTEGER, play_time INTEGER, created_at INTEGER, "
                               "updated_at INTEGER);"
                               "INSERT INTO play_activity VALUES(7,NULL,1000,NULL);"
                               "INSERT INTO play_activity VALUES(7,NULL,2000,NULL);",
                               nullptr, nullptr, nullptr),
                  SQLITE_OK);
    }

    void TearDown() override { sqlite3_close(database); }
};

TEST_F(PlayActivityDurationTest, ClosesOnlyLatestOpenRowWithExplicitElapsedTime)
{
    int updated = -1;
    ASSERT_EQ(play_activity_set_latest_duration(database, 7, 42, 50, &updated), SQLITE_OK);
    EXPECT_EQ(updated, 1);

    sqlite3_stmt *statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(database, "SELECT play_time,updated_at FROM play_activity ORDER BY rowid", -1,
                                 &statement, nullptr),
              SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_type(statement, 0), SQLITE_NULL);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(statement, 0), 42);
    EXPECT_EQ(sqlite3_column_int(statement, 1), 50);
    sqlite3_finalize(statement);
}

TEST_F(PlayActivityDurationTest, WallClockValueCannotChangeExplicitDuration)
{
    int updated = -1;
    ASSERT_EQ(play_activity_set_latest_duration(database, 7, 9, 1, &updated), SQLITE_OK);
    EXPECT_EQ(updated, 1);

    sqlite3_stmt *statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(database, "SELECT play_time FROM play_activity WHERE play_time IS NOT NULL", -1,
                                 &statement, nullptr),
              SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(statement, 0), 9);
    sqlite3_finalize(statement);
}

TEST_F(PlayActivityDurationTest, RefusesInvalidDurationsAndReportsMissingOpenRows)
{
    int updated = -1;
    EXPECT_EQ(play_activity_set_latest_duration(database, 7, -1, 50, &updated), SQLITE_MISUSE);
    EXPECT_EQ(play_activity_set_latest_duration(database, 99, 5, 50, &updated), SQLITE_OK);
    EXPECT_EQ(updated, 0);
}
