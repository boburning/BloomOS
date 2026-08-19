#include <gtest/gtest.h>

#include <sqlite3/sqlite3.h>

extern "C" {
#include "../src/bloomRa/bloom_ra_catalog.h"
#include "../src/bloomRa/bloom_ra_database.h"
}

namespace {

class BloomRaCatalogTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        ASSERT_EQ(SQLITE_OK, sqlite3_open(":memory:", &database_));
        ASSERT_EQ(SQLITE_OK, bloom_ra_database_migrate(database_));
        provider_ = bloom_ra_official_catalog_provider();
    }
    void TearDown() override { sqlite3_close(database_); }

    sqlite3 *database_ = nullptr;
    const BloomRaCatalogProvider *provider_ = nullptr;
};

TEST_F(BloomRaCatalogTest, ImportsOfficialGameListAndResolvesExactHash)
{
    const char *json =
        "[{\"Title\":\"Fixture Game\",\"ID\":1234,\"ConsoleID\":5,\"NumAchievements\":42,"
        "\"Hashes\":[\"5049a8174a2a65954988d899ff3a03a6\"]}]";
    ASSERT_STREQ("ra_web_game_list", provider_->name);
    ASSERT_EQ(SQLITE_OK, provider_->import_console(database_, 5, "fixture-v1", json));
    int game_id = 0, achievements = 0;
    ASSERT_EQ(SQLITE_OK,
              bloom_ra_catalog_resolve(database_, 5, "5049a8174a2a65954988d899ff3a03a6", &game_id, &achievements));
    EXPECT_EQ(1234, game_id);
    EXPECT_EQ(42, achievements);
}

TEST_F(BloomRaCatalogTest, EmptyOrCorruptRefreshPreservesKnownGoodGeneration)
{
    const char *valid =
        "[{\"Title\":\"Fixture Game\",\"ID\":1234,\"ConsoleID\":5,\"NumAchievements\":42,"
        "\"Hashes\":[\"5049a8174a2a65954988d899ff3a03a6\"]}]";
    ASSERT_EQ(SQLITE_OK, provider_->import_console(database_, 5, "fixture-v1", valid));
    EXPECT_EQ(SQLITE_CORRUPT, provider_->import_console(database_, 5, "empty", "[]"));
    EXPECT_EQ(SQLITE_CORRUPT,
              provider_->import_console(database_, 5, "broken", "[{\"ID\":2,\"ConsoleID\":5}]"));
    int game_id = 0, achievements = 0;
    EXPECT_EQ(SQLITE_OK,
              bloom_ra_catalog_resolve(database_, 5, "5049a8174a2a65954988d899ff3a03a6", &game_id, &achievements));
    EXPECT_EQ(1234, game_id);
}

TEST_F(BloomRaCatalogTest, RefreshReplacesOnlySelectedConsoleAtomically)
{
    const char *gba =
        "[{\"Title\":\"GBA\",\"ID\":5,\"ConsoleID\":5,\"NumAchievements\":1,"
        "\"Hashes\":[\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"]}]";
    const char *psx =
        "[{\"Title\":\"PSX\",\"ID\":12,\"ConsoleID\":12,\"NumAchievements\":2,"
        "\"Hashes\":[\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"]}]";
    ASSERT_EQ(SQLITE_OK, provider_->import_console(database_, 5, "gba-1", gba));
    ASSERT_EQ(SQLITE_OK, provider_->import_console(database_, 12, "psx-1", psx));
    ASSERT_EQ(SQLITE_OK, provider_->import_console(database_, 5, "gba-2", gba));
    int game_id = 0, achievements = 0;
    EXPECT_EQ(SQLITE_OK,
              bloom_ra_catalog_resolve(database_, 12, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", &game_id, &achievements));
    EXPECT_EQ(12, game_id);
}

} // namespace
