#include <gtest/gtest.h>

#include <sqlite3/sqlite3.h>

#include <chrono>
#include <filesystem>
extern "C" {
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
    EXPECT_EQ(1, version);
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
    EXPECT_EQ(1, scalar("SELECT version FROM schema_version"));
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
                           "('bloom-game-v1:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','gba','GBA/a.gba',1,2,5,NULL,NULL,NULL,NULL,3,0,1,'unmatched'),"
                           "('bloom-game-v1:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb','gba','GBA/b.gba',1,2,5,'hash',NULL,1,2,3,0,1,'identified');",
                           nullptr, nullptr, nullptr));
    int version = 0, indexed = 0, identified = 0;
    ASSERT_EQ(SQLITE_OK, bloom_ra_database_health(database_, &version, &indexed, &identified));
    EXPECT_EQ(2, indexed);
    EXPECT_EQ(1, identified);
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

} // namespace
