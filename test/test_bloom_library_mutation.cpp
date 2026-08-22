#include "gtest/gtest.h"

#include <filesystem>

extern "C" {
#include "../src/bloomLibrary/bloom_library_database.h"
#include "../src/bloomLibrary/bloom_library_mutation.h"
}

class BloomLibraryMutationTest : public testing::Test {
  protected:
    void SetUp() override
    {
        root_ = std::filesystem::temp_directory_path() / "bloom-library-mutation";
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
        ASSERT_EQ(SQLITE_OK,
                  bloom_library_database_open((root_ / "catalog.sqlite3").c_str(), &database_));
        ASSERT_EQ(SQLITE_OK, sqlite3_exec(database_,
                                          "INSERT INTO systems VALUES('gba','GBA','GBA',NULL,'launch',"
                                          "'gba',1,1,1);"
                                          "INSERT INTO games VALUES('bloom-game-v1:"
                                          "1111111111111111111111111111111111111111111111111111111111111111',"
                                          "'gba','GBA/One.gba','One','one',NULL,1,1,1);"
                                          "INSERT INTO games VALUES('bloom-game-v1:"
                                          "2222222222222222222222222222222222222222222222222222222222222222',"
                                          "'gba','GBA/Two.gba','Two','two',NULL,1,1,1);",
                                          nullptr, nullptr, nullptr));
    }

    void TearDown() override
    {
        sqlite3_close(database_);
        std::filesystem::remove_all(root_);
    }

    sqlite3 *database_ = nullptr;
    std::filesystem::path root_;
};

TEST_F(BloomLibraryMutationTest, AddsIdempotentlyAndRemovesWithCompactPositions)
{
    const char *one = "bloom-game-v1:1111111111111111111111111111111111111111111111111111111111111111";
    const char *two = "bloom-game-v1:2222222222222222222222222222222222222222222222222222222222222222";
    int changed = -1;
    ASSERT_EQ(SQLITE_OK, bloom_library_favorite_set(database_, one, 1, &changed));
    EXPECT_EQ(1, changed);
    ASSERT_EQ(SQLITE_OK, bloom_library_favorite_set(database_, one, 1, &changed));
    EXPECT_EQ(0, changed);
    ASSERT_EQ(SQLITE_OK, bloom_library_favorite_set(database_, two, 1, &changed));
    EXPECT_EQ(1, changed);
    ASSERT_EQ(SQLITE_OK, bloom_library_favorite_set(database_, one, 0, &changed));
    EXPECT_EQ(1, changed);
    sqlite3_stmt *statement = nullptr;
    ASSERT_EQ(SQLITE_OK,
              sqlite3_prepare_v2(database_, "SELECT bloom_game_id,position FROM favorites", -1,
                                 &statement, nullptr));
    ASSERT_EQ(SQLITE_ROW, sqlite3_step(statement));
    EXPECT_STREQ(two, reinterpret_cast<const char *>(sqlite3_column_text(statement, 0)));
    EXPECT_EQ(0, sqlite3_column_int(statement, 1));
    EXPECT_EQ(SQLITE_DONE, sqlite3_step(statement));
    sqlite3_finalize(statement);
}

TEST_F(BloomLibraryMutationTest, RejectsInvalidOrUnknownGames)
{
    int changed = -1;
    EXPECT_EQ(SQLITE_MISUSE, bloom_library_favorite_set(database_, "not-a-game", 1, &changed));
    const char *missing =
        "bloom-game-v1:3333333333333333333333333333333333333333333333333333333333333333";
    EXPECT_EQ(SQLITE_NOTFOUND, bloom_library_favorite_set(database_, missing, 1, &changed));
    EXPECT_EQ(0, changed);
}
