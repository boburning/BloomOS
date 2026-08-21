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
    EXPECT_EQ(0, health.recents);
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
            "INSERT INTO apps VALUES('bloom-app-v1:tweaks','Tweaks','App/Tweaks/launch.sh',NULL,1,2,1,'bloom-native');"
            "INSERT INTO favorites VALUES('bloom-game-v1:one',0);"
            "INSERT INTO recents VALUES('bloom-game-v1:one',0);"
            "UPDATE library_state SET generation=7,status='ready' WHERE id=1;");
    BloomLibraryHealth health{};
    ASSERT_EQ(SQLITE_OK, bloom_library_database_health(database, &health));
    EXPECT_EQ(7, health.generation);
    EXPECT_STREQ("ready", health.status);
    EXPECT_EQ(1, health.systems);
    EXPECT_EQ(1, health.games);
    EXPECT_EQ(1, health.apps);
    EXPECT_EQ(1, health.favorites);
    EXPECT_EQ(1, health.recents);
    sqlite3_close(database);
}

TEST_F(BloomLibraryDatabaseTest, SchemaOneAddsGlobalPagingIndexTransactionally)
{
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(path_.c_str(), &database));
    execute(database,
            "CREATE TABLE schema_version(version INTEGER NOT NULL);INSERT INTO schema_version VALUES(1);"
            "CREATE TABLE library_state(id INTEGER PRIMARY KEY CHECK(id=1),generation INTEGER NOT NULL "
            "DEFAULT 0,status TEXT NOT NULL CHECK(status IN('empty','ready','scanning','stale','error')),"
            "source TEXT NOT NULL);INSERT INTO library_state VALUES(1,4,'ready','onion');"
            "CREATE TABLE systems(system_id TEXT PRIMARY KEY,label TEXT NOT NULL,rom_path TEXT NOT NULL,"
            "img_path TEXT,launch_path TEXT NOT NULL,extensions TEXT NOT NULL,config_size INTEGER NOT NULL,"
            "config_mtime INTEGER NOT NULL,present INTEGER NOT NULL CHECK(present IN(0,1)));"
            "CREATE TABLE games(bloom_game_id TEXT PRIMARY KEY,system_id TEXT NOT NULL,"
            "normalized_rom_path TEXT NOT NULL,display_title TEXT NOT NULL,sort_title TEXT NOT NULL,"
            "image_path TEXT,file_size INTEGER NOT NULL,file_mtime INTEGER NOT NULL,present INTEGER NOT NULL "
            "CHECK(present IN(0,1)),UNIQUE(system_id,normalized_rom_path),FOREIGN KEY(system_id) "
            "REFERENCES systems(system_id));CREATE INDEX games_system_sort ON games(system_id,present,"
            "sort_title,bloom_game_id);CREATE TABLE apps(app_id TEXT PRIMARY KEY,label TEXT NOT NULL,"
            "launch_path TEXT NOT NULL,icon_path TEXT,config_size INTEGER NOT NULL,config_mtime INTEGER NOT NULL,"
            "present INTEGER NOT NULL CHECK(present IN(0,1)));CREATE INDEX apps_sort ON apps(present,label,app_id);"
            "CREATE TABLE favorites(bloom_game_id TEXT PRIMARY KEY,position INTEGER NOT NULL UNIQUE,"
            "FOREIGN KEY(bloom_game_id) REFERENCES games(bloom_game_id));CREATE TABLE legacy_items(kind TEXT "
            "NOT NULL CHECK(kind IN('favorite','recent')),position INTEGER NOT NULL,legacy_identity TEXT NOT NULL,"
            "status TEXT NOT NULL CHECK(status IN('matched','unmatched','duplicate','invalid')),bloom_game_id TEXT,"
            "PRIMARY KEY(kind,position),FOREIGN KEY(bloom_game_id) REFERENCES games(bloom_game_id));");
    execute(database,
            "INSERT INTO systems VALUES('gba','GBA','GBA',NULL,'launch','gba',1,2,1);"
            "INSERT INTO games VALUES('bloom-game-v1:one','gba','GBA/one.gba','One','one',NULL,3,4,1);");
    sqlite3_close(database);

    ASSERT_EQ(SQLITE_OK, bloom_library_database_open(path_.c_str(), &database));
    BloomLibraryHealth health{};
    ASSERT_EQ(SQLITE_OK, bloom_library_database_health(database, &health));
    EXPECT_EQ(4, health.schema_version);
    EXPECT_EQ(4, health.generation);
    EXPECT_EQ(1, health.games);
    sqlite3_stmt *statement = nullptr;
    ASSERT_EQ(SQLITE_OK,
              sqlite3_prepare_v2(database,
                                 "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND "
                                 "name='games_global_sort'",
                                 -1, &statement, nullptr));
    ASSERT_EQ(SQLITE_ROW, sqlite3_step(statement));
    EXPECT_EQ(1, sqlite3_column_int(statement, 0));
    sqlite3_finalize(statement);
    sqlite3_close(database);
}

TEST_F(BloomLibraryDatabaseTest, SchemaTwoAddsCanonicalRecentsWithoutLosingState)
{
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, bloom_library_database_open(path_.c_str(), &database));
    execute(database,
            "INSERT INTO systems VALUES('gba','GBA','GBA',NULL,'launch','gba',1,2,1);"
            "INSERT INTO games VALUES('bloom-game-v1:one','gba','GBA/one.gba','One','one',NULL,3,4,1);"
            "INSERT INTO favorites VALUES('bloom-game-v1:one',0);"
            "UPDATE schema_version SET version=2;DROP TABLE recents;"
            "ALTER TABLE apps DROP COLUMN compatibility;");
    sqlite3_close(database);

    ASSERT_EQ(SQLITE_OK, bloom_library_database_open(path_.c_str(), &database));
    BloomLibraryHealth health{};
    ASSERT_EQ(SQLITE_OK, bloom_library_database_health(database, &health));
    EXPECT_EQ(4, health.schema_version);
    EXPECT_EQ(1, health.games);
    EXPECT_EQ(1, health.favorites);
    EXPECT_EQ(0, health.recents);
    sqlite3_close(database);
}

TEST_F(BloomLibraryDatabaseTest, SchemaThreeClassifiesExistingAppsConservatively)
{
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, bloom_library_database_open(path_.c_str(), &database));
    execute(database,
            "INSERT INTO apps VALUES('legacy','Legacy','App/Legacy/launch.sh',NULL,1,2,1,'bloom-native');"
            "UPDATE schema_version SET version=3;ALTER TABLE apps DROP COLUMN compatibility;");
    sqlite3_close(database);

    ASSERT_EQ(SQLITE_OK, bloom_library_database_open(path_.c_str(), &database));
    sqlite3_stmt *statement = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_prepare_v2(database,
                                            "SELECT compatibility FROM apps WHERE app_id='legacy'",
                                            -1, &statement, nullptr));
    ASSERT_EQ(SQLITE_ROW, sqlite3_step(statement));
    EXPECT_STREQ("mainui-dependent",
                 reinterpret_cast<const char *>(sqlite3_column_text(statement, 0)));
    sqlite3_finalize(statement);
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

TEST_F(BloomLibraryDatabaseTest, MissingPagingIndexRefusesToOpen)
{
    sqlite3 *database = nullptr;
    ASSERT_EQ(SQLITE_OK, bloom_library_database_open(path_.c_str(), &database));
    sqlite3_close(database);
    ASSERT_EQ(SQLITE_OK, sqlite3_open(path_.c_str(), &database));
    execute(database, "DROP INDEX games_global_sort");
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
