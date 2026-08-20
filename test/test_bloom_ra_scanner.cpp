#include <gtest/gtest.h>

#include <sqlite3/sqlite3.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

extern "C" {
#include "../src/bloomRa/bloom_ra.h"
#include "../src/bloomRa/bloom_ra_catalog.h"
#include "../src/bloomRa/bloom_ra_database.h"
#include "../src/bloomRa/bloom_ra_scanner.h"
}

namespace {

int playlist_hash_calls;
int playlist_hasher(unsigned int console_id, const char *, char hash[33])
{
    ++playlist_hash_calls;
    if (console_id != 12)
        return 0;
    std::snprintf(hash, 33, "%s", "6d8c25fc66fe48250957c0af0639d376");
    return 1;
}

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
        bloom_ra_set_disc_hasher_for_test(nullptr);
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

TEST_F(BloomRaScannerTest, CancelMarkerInterruptsAtTheNextScanCheckpoint)
{
    auto cancel = root_ / "scan.cancel";
    std::ofstream(cancel) << "";
    BloomRaScanStats stats = {};
    EXPECT_EQ(SQLITE_INTERRUPT,
              bloom_ra_scan_interruption_for_test(nullptr, cancel.c_str(), &stats));
    EXPECT_EQ(1, stats.canceled);
    EXPECT_EQ(0, stats.paused);
}

TEST_F(BloomRaScannerTest, ActiveSessionPausesAtTheNextScanCheckpoint)
{
    auto session = root_ / "session.state";
    std::ofstream(session) << "RUNNING\n";
    BloomRaScanStats stats = {};
    EXPECT_EQ(SQLITE_BUSY,
              bloom_ra_scan_interruption_for_test(session.c_str(), nullptr, &stats));
    EXPECT_EQ(0, stats.canceled);
    EXPECT_EQ(1, stats.paused);
}

TEST_F(BloomRaScannerTest, PlaylistSkipsOnlyWhileItsFirstDiscSignalsAreUnchanged)
{
    auto disc = root_ / "disc1.chd";
    auto playlist = root_ / "game.m3u";
    std::ofstream(disc, std::ios::binary) << "synthetic disc v1";
    std::ofstream(playlist) << "disc1.chd\n";
    playlist_hash_calls = 0;
    bloom_ra_set_disc_hasher_for_test(playlist_hasher);
    BloomRaScanResult result = {};
    ASSERT_EQ(SQLITE_OK,
              bloom_ra_scan_game(database_, game_id, "psx", playlist.c_str(), root_.c_str(), "PS/game.m3u", 0,
                                 &result));
    EXPECT_EQ(0, result.skipped);
    EXPECT_EQ(1, playlist_hash_calls);
    ASSERT_EQ(SQLITE_OK,
              bloom_ra_scan_game(database_, game_id, "psx", playlist.c_str(), root_.c_str(), "PS/game.m3u", 0,
                                 &result));
    EXPECT_EQ(1, result.skipped);
    EXPECT_EQ(1, playlist_hash_calls);
    auto original_time = std::filesystem::last_write_time(disc);
    std::ofstream(disc, std::ios::binary | std::ios::trunc) << "synthetic disc v2 with changed size";
    std::filesystem::last_write_time(disc, original_time + std::chrono::seconds(2));
    ASSERT_EQ(SQLITE_OK,
              bloom_ra_scan_game(database_, game_id, "psx", playlist.c_str(), root_.c_str(), "PS/game.m3u", 0,
                                 &result));
    EXPECT_EQ(0, result.skipped);
    EXPECT_EQ(2, playlist_hash_calls);
    bloom_ra_set_disc_hasher_for_test(nullptr);
}

} // namespace
