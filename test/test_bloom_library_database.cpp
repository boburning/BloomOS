#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <sqlite3/sqlite3.h>
#include <unistd.h>

extern "C" {
#include "../src/bloomLibrary/bloom_library_database.h"
}

class BloomLibraryDatabaseTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root_ = std::filesystem::temp_directory_path() /
                ("bloom-library-" + std::to_string(getpid()) + "-" +
                 std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::create_directories(root_);
        path_ = root_ / "catalog.sqlite3";
    }

    void TearDown() override { std::filesystem::remove_all(root_); }

    static void execute(sqlite3 *database, const char *sql)
    {
        ASSERT_EQ(SQLITE_OK, sqlite3_exec(database, sql, nullptr, nullptr, nullptr));
    }

    std::filesystem::path root_;
    std::filesystem::path path_;
};

TEST_F(BloomLibraryDatabaseTest, FreshDatabaseCreatesCompleteEmptySchema)
{
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, bloom_library_database_open(path_.c_str(), &database));
    BloomLibraryHealth health{};
    ASSERT_EQ(SQLITE_OK, bloom_library_database_health(database, &health));
    EXPECT_EQ(BLOOM_LIBRARY_DATABASE_SCHEMA_VERSION, health.schema_version);
    EXPECT_EQ(0, health.generation);
    EXPECT_STREQ("empty", health.status);
    EXPECT_EQ(0, health.systems);
    EXPECT_EQ(0, health.games);
    EXPECT_EQ(0, health.apps);
    EXPECT_EQ(0, health.favorites);
    sqlite3_close(database);

    ASSERT_EQ(SQLITE_OK, bloom_library_database_open(path_.c_str(), &database));
    ASSERT_EQ(SQLITE_OK, bloom_library_database_health(database, &health));
    sqlite3_close(database);
}

TEST_F(BloomLibraryDatabaseTest, HealthCountsOnlyPresentIndexedContent)
{
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, bloom_library_database_open(path_.c_str(), &database));
    execute(database,
            "INSERT INTO systems VALUES('gba','GBA','GBA','GBA/Imgs','launch.sh','gba|zip',1,2,1);"
            "INSERT INTO systems VALUES('old','Old','OLD',NULL,'launch.sh','bin',1,2,0);"
            "INSERT INTO games VALUES('bloom-game-v1:one','gba','GBA/one.gba','One','one',NULL,3,4,1);"
            "INSERT INTO games VALUES('bloom-game-v1:gone','gba','GBA/gone.gba','Gone','gone',NULL,3,4,0);"
            "INSERT INTO apps VALUES('bloom-app-v1:tweaks','Tweaks','App/Tweaks/launch.sh',NULL,1,2,1);"
            "INSERT INTO favorites VALUES('bloom-game-v1:one',0);"
            "UPDATE library_state SET generation=7,status='ready' WHERE id=1;");
    BloomLibraryHealth health{};
    ASSERT_EQ(SQLITE_OK, bloom_library_database_health(database, &health));
    EXPECT_EQ(7, health.generation);
    EXPECT_STREQ("ready", health.status);
    EXPECT_EQ(1, health.systems);
    EXPECT_EQ(1, health.games);
    EXPECT_EQ(1, health.apps);
    EXPECT_EQ(1, health.favorites);
    sqlite3_close(database);
}

TEST_F(BloomLibraryDatabaseTest, NewerSchemaRefusesToOpen)
{
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(path_.c_str(), &database));
    execute(database, "CREATE TABLE schema_version(version INTEGER NOT NULL);"
                      "INSERT INTO schema_version VALUES(99);");
    sqlite3_close(database);
    database = nullptr;
    EXPECT_EQ(SQLITE_MISMATCH, bloom_library_database_open(path_.c_str(), &database));
    EXPECT_EQ(nullptr, database);
}

TEST_F(BloomLibraryDatabaseTest, CorruptKnownSchemaRefusesToOpen)
{
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(path_.c_str(), &database));
    execute(database, "CREATE TABLE schema_version(version INTEGER NOT NULL);"
                      "INSERT INTO schema_version VALUES(1);");
    sqlite3_close(database);
    database = nullptr;
    EXPECT_EQ(SQLITE_CORRUPT, bloom_library_database_open(path_.c_str(), &database));
    EXPECT_EQ(nullptr, database);
}

TEST_F(BloomLibraryDatabaseTest, SymlinkedDatabasePathFailsClosed)
{
    auto outside = root_ / "outside.sqlite3";
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(outside.c_str(), &database));
    sqlite3_close(database);
    std::filesystem::create_symlink(outside, path_);
    database = nullptr;
    EXPECT_EQ(SQLITE_CANTOPEN, bloom_library_database_open(path_.c_str(), &database));
    EXPECT_EQ(nullptr, database);
}
