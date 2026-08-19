#include <gtest/gtest.h>

#include <sqlite3/sqlite3.h>

#include <chrono>
#include <filesystem>
#include <fstream>

extern "C" {
#include "../src/bloomRa/bloom_ra_catalog.h"
#include "../src/bloomRa/bloom_ra_database.h"
#include "../src/bloomRa/bloom_ra_scanner.h"
}

namespace {

class BloomRaScannerTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root_ = std::filesystem::temp_directory_path() /
                ("bloom-ra-scan-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root_);
        rom_ = root_ / "fixture.gba";
        std::ofstream(rom_, std::ios::binary) << "Bloom RA fixture v1";
        ASSERT_EQ(SQLITE_OK, sqlite3_open(":memory:", &database_));
        ASSERT_EQ(SQLITE_OK, bloom_ra_database_migrate(database_));
        const char *catalog =
            "[{\"Title\":\"Fixture\",\"ID\":1234,\"ConsoleID\":5,\"NumAchievements\":42,"
            "\"Hashes\":[\"5049a8174a2a65954988d899ff3a03a6\"]}]";
        ASSERT_EQ(SQLITE_OK,
                  bloom_ra_official_catalog_provider()->import_console(database_, 5, "fixture-v1", catalog));
    }
    void TearDown() override
    {
        sqlite3_close(database_);
        std::filesystem::remove_all(root_);
    }

    static constexpr const char *game_id =
        "bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    std::filesystem::path root_;
    std::filesystem::path rom_;
    sqlite3 *database_ = nullptr;
};

TEST_F(BloomRaScannerTest, IdentifiesExactContentThenSkipsUnchangedFile)
{
    BloomRaScanResult result = {};
    ASSERT_EQ(SQLITE_OK,
              bloom_ra_scan_game(database_, game_id, "gba", rom_.c_str(), root_.c_str(), "GBA/fixture.gba", 0,
                                 &result));
    EXPECT_EQ(1, result.identified);
    EXPECT_STREQ("identified", result.status);
    EXPECT_EQ(0, result.skipped);

    ASSERT_EQ(SQLITE_OK,
              bloom_ra_scan_game(database_, game_id, "gba", rom_.c_str(), root_.c_str(), "GBA/fixture.gba", 0,
                                 &result));
    EXPECT_EQ(1, result.skipped);
    EXPECT_EQ(1, result.identified);
}

TEST_F(BloomRaScannerTest, ChangedMtimeInvalidatesPriorIdentification)
{
    BloomRaScanResult result = {};
    ASSERT_EQ(SQLITE_OK,
              bloom_ra_scan_game(database_, game_id, "gba", rom_.c_str(), root_.c_str(), "GBA/fixture.gba", 0,
                                 &result));
    auto original_time = std::filesystem::last_write_time(rom_);
    std::ofstream(rom_, std::ios::binary | std::ios::trunc) << "Bloom RA fixture v2";
    std::filesystem::last_write_time(rom_, original_time + std::chrono::seconds(2));
    ASSERT_EQ(SQLITE_OK,
              bloom_ra_scan_game(database_, game_id, "gba", rom_.c_str(), root_.c_str(), "GBA/fixture.gba", 0,
                                 &result));
    EXPECT_EQ(0, result.skipped);
    EXPECT_EQ(0, result.identified);
    EXPECT_STREQ("unmatched", result.status);
}

TEST_F(BloomRaScannerTest, UnsupportedSystemIsPersistedWithoutHashing)
{
    BloomRaScanResult result = {};
    ASSERT_EQ(SQLITE_OK,
              bloom_ra_scan_game(database_, game_id, "pico8", rom_.c_str(), root_.c_str(), "PICO/fixture.p8", 0,
                                 &result));
    EXPECT_STREQ("unsupported_system", result.status);
    EXPECT_EQ(0, result.identified);
}

} // namespace
