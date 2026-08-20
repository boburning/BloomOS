#include <gtest/gtest.h>

#include <sqlite3/sqlite3.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
extern "C" {
#include "../src/bloomRa/bloom_ra.h"
#include "../src/bloomRa/bloom_ra_database.h"
}

namespace {

class BloomRaDatabaseTest : public ::testing::Test {
  protected:
    void SetUp() override { ASSERT_EQ(SQLITE_OK, sqlite3_open(":memory:", &database_)); }
    void TearDown() override { sqlite3_close(database_); }

    int scalar(const char *sql)
    {
        sqlite3_stmt *statement = nullptr;
        EXPECT_EQ(SQLITE_OK, sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr));
        EXPECT_EQ(SQLITE_ROW, sqlite3_step(statement));
        int value = sqlite3_column_int(statement, 0);
        sqlite3_finalize(statement);
        return value;
    }

    sqlite3 *database_ = nullptr;
};

TEST_F(BloomRaDatabaseTest, FreshMigrationCreatesVersionedCatalogAndLibrarySchema)
{
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_migrate(database_));
    int version = 0, indexed = -1, identified = -1;
    EXPECT_EQ(SQLITE_OK, bloom_ra_database_health(database_, &version, &indexed, &identified));
    EXPECT_EQ(2, version);
    EXPECT_EQ(0, indexed);
    EXPECT_EQ(0, identified);
    EXPECT_EQ(5, scalar("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN "
                        "('schema_version','catalog_state','ra_games','ra_hashes','library_games')"));
}

TEST_F(BloomRaDatabaseTest, MigrationIsTransactionalAndIdempotent)
{
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_migrate(database_));
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_migrate(database_));
    EXPECT_EQ(1, scalar("SELECT COUNT(*) FROM schema_version"));
    EXPECT_EQ(2, scalar("SELECT version FROM schema_version"));
}

TEST_F(BloomRaDatabaseTest, VersionOneUpgradeAddsPlaylistDependencySignalsAndPreservesRows)
{
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_migrate(database_));
    ASSERT_EQ(SQLITE_OK,
              sqlite3_exec(database_,
                           "INSERT INTO library_games VALUES"
                           "('bloom-game-v1:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','psx','PS/game.m3u',10,20,12,'hash',NULL,NULL,NULL,30,0,2,'unmatched',NULL,NULL);"
                           "ALTER TABLE library_games DROP COLUMN dependency_size;"
                           "ALTER TABLE library_games DROP COLUMN dependency_mtime;"
                           "UPDATE schema_version SET version=1;",
                           nullptr, nullptr, nullptr));
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_migrate(database_));
    EXPECT_EQ(2, scalar("SELECT version FROM schema_version"));
    EXPECT_EQ(1, scalar("SELECT COUNT(*) FROM library_games"));
    EXPECT_EQ(2, scalar("SELECT COUNT(*) FROM pragma_table_info('library_games') WHERE name LIKE 'dependency_%'"));
}

TEST_F(BloomRaDatabaseTest, RefusesNewerOrStructurallyIncompleteSchema)
{
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(database_, "CREATE TABLE schema_version(version INTEGER);INSERT INTO schema_version VALUES(99);",
                                      nullptr, nullptr, nullptr));
    EXPECT_EQ(SQLITE_MISMATCH, bloom_ra_database_migrate(database_));
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(database_, "UPDATE schema_version SET version=1", nullptr, nullptr, nullptr));
    EXPECT_EQ(SQLITE_CORRUPT, bloom_ra_database_migrate(database_));
}

TEST_F(BloomRaDatabaseTest, HealthCountsOnlyIdentifiedRowsWithoutExposingGameData)
{
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_migrate(database_));
    ASSERT_EQ(SQLITE_OK,
              sqlite3_exec(database_,
                           "INSERT INTO library_games VALUES"
                           "('bloom-game-v1:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','gba','GBA/a.gba',1,2,5,NULL,NULL,NULL,NULL,3,0,1,'unmatched',NULL,NULL),"
                           "('bloom-game-v1:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb','gba','GBA/b.gba',1,2,5,'hash',NULL,1,2,3,0,1,'identified',NULL,NULL);",
                           nullptr, nullptr, nullptr));
    int version = 0, indexed = 0, identified = 0;
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_health(database_, &version, &indexed, &identified));
    EXPECT_EQ(2, indexed);
    EXPECT_EQ(1, identified);
}

TEST_F(BloomRaDatabaseTest, CatalogStatusIsBoundedAndAllowlisted)
{
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_migrate(database_));
    char status[32] = {};
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_catalog_status(database_, status, sizeof(status)));
    EXPECT_STREQ("unavailable", status);
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(database_, "UPDATE catalog_state SET status='ready'", nullptr, nullptr, nullptr));
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_catalog_status(database_, status, sizeof(status)));
    EXPECT_STREQ("ready", status);
    ASSERT_EQ(SQLITE_OK,
              sqlite3_exec(database_, "UPDATE catalog_state SET status='private-path-or-secret'", nullptr, nullptr, nullptr));
    EXPECT_EQ(SQLITE_CORRUPT, bloom_ra_database_catalog_status(database_, status, sizeof(status)));
}

TEST(BloomRaDatabaseOpenTest, ConfiguresDurableDatabaseAndRejectsSymlinkBoundary)
{
    auto root = std::filesystem::temp_directory_path() /
                ("bloom-ra-db-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    auto path = root / "catalog.sqlite3";
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_open(path.c_str(), &database));
    ASSERT_NE(nullptr, database);
    sqlite3_stmt *statement = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_prepare_v2(database, "PRAGMA journal_mode", -1, &statement, nullptr));
    ASSERT_EQ(SQLITE_ROW, sqlite3_step(statement));
    EXPECT_STREQ("wal", reinterpret_cast<const char *>(sqlite3_column_text(statement, 0)));
    sqlite3_finalize(statement);
    sqlite3_close(database);

    auto link = root / "linked.sqlite3";
    std::filesystem::create_symlink(path, link);
    database = nullptr;
    EXPECT_EQ(SQLITE_CANTOPEN, bloom_ra_database_open(link.c_str(), &database));
    EXPECT_EQ(nullptr, database);
    std::filesystem::remove_all(root);
}

TEST_F(BloomRaDatabaseTest, BadgeRequiresExactOfficialGameWithAchievements)
{
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_migrate(database_));
    const char *game_id =
        "bloom-game-v1:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    ASSERT_EQ(SQLITE_OK,
              sqlite3_exec(database_,
                           "INSERT INTO library_games VALUES"
                           "('bloom-game-v1:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','gba','GBA/a.gba',1,2,5,'hash',1234,1,42,3,1,1,'identified',NULL,NULL)",
                           nullptr, nullptr, nullptr));
    BloomRaGame game = {};
    char error[128] = {};
    ASSERT_EQ(0, bloom_ra_get_game_from_database(database_, game_id, &game, error, sizeof(error))) << error;
    EXPECT_EQ(1, game.has_ra_badge);
    EXPECT_EQ(1234, game.ra_game_id);
    EXPECT_EQ(42UL, game.achievement_count);

    ASSERT_EQ(SQLITE_OK,
              sqlite3_exec(database_, "UPDATE library_games SET achievement_count=0 WHERE bloom_game_id LIKE 'bloom-game-v1:%'",
                           nullptr, nullptr, nullptr));
    ASSERT_EQ(0, bloom_ra_get_game_from_database(database_, game_id, &game, error, sizeof(error)));
    EXPECT_EQ(0, game.has_ra_badge);
}

TEST_F(BloomRaDatabaseTest, CachedValidBadgeSurvivesStaleCatalogStatus)
{
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_migrate(database_));
    const char *game_id =
        "bloom-game-v1:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    ASSERT_EQ(SQLITE_OK,
              sqlite3_exec(database_,
                           "INSERT INTO library_games VALUES"
                           "('bloom-game-v1:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb','snes','SFC/b.sfc',1,2,3,'hash',22,1,5,3,1,1,'stale',NULL,NULL)",
                           nullptr, nullptr, nullptr));
    BloomRaGame game = {};
    char error[128] = {};
    ASSERT_EQ(0, bloom_ra_get_game_from_database(database_, game_id, &game, error, sizeof(error))) << error;
    EXPECT_STREQ("stale", game.status);
    EXPECT_EQ(1, game.has_ra_badge);
}

static int collect_game(const char *game_id, const char *system_id, void *context)
{
    auto *items = static_cast<std::vector<std::string> *>(context);
    items->push_back(std::string(system_id) + ":" + game_id);
    return 0;
}

TEST_F(BloomRaDatabaseTest, SmartCollectionDerivesOnlyExactBadgeMembers)
{
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_migrate(database_));
    ASSERT_EQ(SQLITE_OK,
              sqlite3_exec(database_,
                           "INSERT INTO library_games VALUES"
                           "('bloom-game-v1:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','snes','SFC/a.sfc',1,2,3,'a',10,1,5,3,1,1,'identified',NULL,NULL),"
                           "('bloom-game-v1:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb','gba','GBA/b.gba',1,2,5,'b',11,1,8,3,1,1,'stale',NULL,NULL),"
                           "('bloom-game-v1:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc','gba','GBA/c.gba',1,2,5,'c',12,0,8,3,1,1,'identified',NULL,NULL),"
                           "('bloom-game-v1:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd','gba','GBA/d.gba',1,2,5,'d',13,1,0,3,1,1,'identified',NULL,NULL)",
                           nullptr, nullptr, nullptr));
    std::vector<std::string> items;
    unsigned long count = 0;
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_collection(database_, collect_game, &items, &count));
    ASSERT_EQ(2UL, count);
    ASSERT_EQ(2UL, items.size());
    EXPECT_EQ("gba:bloom-game-v1:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", items[0]);
    EXPECT_EQ("snes:bloom-game-v1:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", items[1]);
}

} // namespace
