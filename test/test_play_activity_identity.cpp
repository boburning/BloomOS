#include <gtest/gtest.h>

#include <sqlite3/sqlite3.h>

extern "C" {
#include "../src/playActivity/playActivityIdentity.h"
}

namespace {

class PlayActivityIdentityTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        ASSERT_EQ(SQLITE_OK, sqlite3_open(":memory:", &database_));
        ASSERT_EQ(SQLITE_OK,
                  sqlite3_exec(database_, "CREATE TABLE rom(id INTEGER PRIMARY KEY,file_path TEXT,game_id TEXT);"
                                          "CREATE UNIQUE INDEX rom_game_id_index ON rom(game_id) WHERE game_id IS NOT NULL;",
                               nullptr, nullptr, nullptr));
    }
    void TearDown() override { sqlite3_close(database_); }
    void insert(int id, const char *path)
    {
        sqlite3_stmt *statement = nullptr;
        ASSERT_EQ(SQLITE_OK, sqlite3_prepare_v2(database_, "INSERT INTO rom(id,file_path) VALUES(?1,?2)", -1,
                                                &statement, nullptr));
        sqlite3_bind_int(statement, 1, id);
        sqlite3_bind_text(statement, 2, path, -1, SQLITE_STATIC);
        ASSERT_EQ(SQLITE_DONE, sqlite3_step(statement));
        sqlite3_finalize(statement);
    }
    int identified()
    {
        sqlite3_stmt *statement = nullptr;
        EXPECT_EQ(SQLITE_OK, sqlite3_prepare_v2(database_, "SELECT COUNT(*) FROM rom WHERE game_id IS NOT NULL", -1,
                                                &statement, nullptr));
        EXPECT_EQ(SQLITE_ROW, sqlite3_step(statement));
        int value = sqlite3_column_int(statement, 0);
        sqlite3_finalize(statement);
        return value;
    }
    sqlite3 *database_ = nullptr;
};

TEST_F(PlayActivityIdentityTest, DryRunCountsOnlyDeterministicProvenSystems)
{
    insert(1, "GB/Pokemon Red.zip");
    insert(2, "PS/Power Play.chd");
    insert(3, "ARCADE/Unknown.zip");
    play_activity_identity_result result = {};
    ASSERT_EQ(SQLITE_OK, play_activity_backfill_game_ids(database_, 1, &result));
    EXPECT_EQ(2, result.updated);
    EXPECT_EQ(1, result.deferred);
    EXPECT_EQ(0, identified());
}

TEST_F(PlayActivityIdentityTest, AppliesCanonicalIdsAndIsIdempotent)
{
    insert(1, "FC/Test.zip");
    insert(2, "/mnt/SDCARD/Roms/SFC/Test.zip");
    play_activity_identity_result result = {};
    ASSERT_EQ(SQLITE_OK, play_activity_backfill_game_ids(database_, 0, &result));
    EXPECT_EQ(2, result.updated);
    EXPECT_EQ(0, result.deferred);
    EXPECT_EQ(2, identified());
    ASSERT_EQ(SQLITE_OK, play_activity_backfill_game_ids(database_, 0, &result));
    EXPECT_EQ(0, result.updated);
    EXPECT_EQ(0, result.deferred);
}

TEST_F(PlayActivityIdentityTest, DuplicateLegacyRowsRemainUnmodifiedAndDeferred)
{
    insert(1, "GB/Duplicate.zip");
    insert(2, "GB/Duplicate.zip");
    insert(3, "GBA/Unique.zip");
    play_activity_identity_result result = {};
    ASSERT_EQ(SQLITE_OK, play_activity_backfill_game_ids(database_, 0, &result));
    EXPECT_EQ(1, result.updated);
    EXPECT_EQ(2, result.deferred);
    EXPECT_EQ(1, identified());
}

TEST_F(PlayActivityIdentityTest, ExistingIdentityConflictDoesNotBlockUnrelatedCandidates)
{
    insert(1, "GB/Already.zip");
    play_activity_identity_result initial = {};
    ASSERT_EQ(SQLITE_OK, play_activity_backfill_game_ids(database_, 0, &initial));
    insert(2, "GB/Already.zip");
    insert(3, "GBC/Unique.zip");
    play_activity_identity_result result = {};
    ASSERT_EQ(SQLITE_OK, play_activity_backfill_game_ids(database_, 0, &result));
    EXPECT_EQ(1, result.updated);
    EXPECT_EQ(1, result.deferred);
    EXPECT_EQ(2, identified());
}

} // namespace
