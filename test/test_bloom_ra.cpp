#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

extern "C" {
#include "../src/bloomRa/bloom_ra.h"
}

TEST(BloomRaTest, StatusIsVersionedAndOfflineWithoutConfiguration)
{
    BloomRaStatus status = {};
    bloom_ra_get_status(&status);
    EXPECT_EQ(1, status.schema);
    EXPECT_EQ(0, status.enabled);
    EXPECT_STREQ("not_configured", status.state);
    EXPECT_STREQ("not_implemented", status.catalog_status);
    EXPECT_EQ(0UL, status.indexed_games);
    EXPECT_EQ(0UL, status.identified_games);
}

TEST(BloomRaTest, ValidGameIdReturnsUnindexedWithoutInventingRaIdentity)
{
    const char *game_id =
        "bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    BloomRaGame game = {};
    char error[128] = {};
    ASSERT_EQ(0, bloom_ra_get_game(game_id, &game, error, sizeof(error))) << error;
    EXPECT_STREQ(game_id, game.game_id);
    EXPECT_STREQ("unindexed", game.status);
    EXPECT_EQ(0, game.has_ra_badge);
    EXPECT_EQ(0UL, game.achievement_count);
}

TEST(BloomRaTest, InvalidGameIdFailsClosed)
{
    BloomRaGame game = {};
    char error[128] = {};
    EXPECT_NE(0, bloom_ra_get_game("bloom-game-v1:not-a-hash", &game, error, sizeof(error)));
    EXPECT_STREQ("invalid Bloom GameID", error);
}

TEST(BloomRaTest, MissingOutputFailsClosed)
{
    const char *game_id =
        "bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    char error[128] = {};
    EXPECT_NE(0, bloom_ra_get_game(game_id, nullptr, error, sizeof(error)));
    EXPECT_STREQ("missing game output", error);
}

TEST(BloomRaTest, MapsCanonicalSystemsToAuthoritativeConsoleIds)
{
    uint32_t console_id = 0;
    ASSERT_EQ(0, bloom_ra_console_id("gba", &console_id));
    EXPECT_EQ(5U, console_id);
    ASSERT_EQ(0, bloom_ra_console_id("psx", &console_id));
    EXPECT_EQ(12U, console_id);
    ASSERT_EQ(0, bloom_ra_console_id("fds", &console_id));
    EXPECT_EQ(81U, console_id);
    EXPECT_NE(0, bloom_ra_console_id("pico8", &console_id));
}

TEST(BloomRaTest, HashesExactRomContentAndRejectsPathsOutsideRoot)
{
    char directory_template[] = "/tmp/bloom-ra-XXXXXX";
    const char *root = mkdtemp(directory_template);
    ASSERT_NE(nullptr, root);
    std::filesystem::path rom = std::filesystem::path(root) / "fixture.gba";
    std::ofstream(rom, std::ios::binary) << "Bloom RA fixture v1";
    char hash[33] = {};
    char error[128] = {};
    ASSERT_EQ(0, bloom_ra_hash_file("gba", rom.c_str(), root, hash, error, sizeof(error))) << error;
    EXPECT_STREQ("5049a8174a2a65954988d899ff3a03a6", hash);
    EXPECT_NE(0, bloom_ra_hash_file("gba", rom.c_str(), "/tmp/not-the-root", hash, error, sizeof(error)));
    std::filesystem::remove_all(root);
}

TEST(BloomRaTest, HashesCartridgeContentInsideZipArchive)
{
    static const unsigned char fixture[] = {
        0x50,0x4b,0x03,0x04,0x14,0x00,0x00,0x00,0x08,0x00,0xd7,0x98,0x13,0x5d,0x5c,0x28,
        0xca,0xa2,0x15,0x00,0x00,0x00,0x13,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x66,0x69,
        0x78,0x74,0x75,0x72,0x65,0x2e,0x67,0x62,0x61,0x73,0xca,0xc9,0xcf,0xcf,0x55,0x08,
        0x72,0x54,0x48,0xcb,0xac,0x28,0x29,0x2d,0x4a,0x55,0x28,0x33,0x04,0x00,0x50,0x4b,
        0x01,0x02,0x14,0x00,0x14,0x00,0x00,0x00,0x08,0x00,0xd7,0x98,0x13,0x5d,0x5c,0x28,
        0xca,0xa2,0x15,0x00,0x00,0x00,0x13,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x01,0x00,0x00,0x00,0x00,0x66,0x69,0x78,0x74,
        0x75,0x72,0x65,0x2e,0x67,0x62,0x61,0x50,0x4b,0x05,0x06,0x00,0x00,0x00,0x00,0x01,
        0x00,0x01,0x00,0x39,0x00,0x00,0x00,0x3e,0x00,0x00,0x00,0x00,0x00};
    char directory_template[] = "/tmp/bloom-ra-zip-XXXXXX";
    const char *root = mkdtemp(directory_template);
    ASSERT_NE(nullptr, root);
    std::filesystem::path archive = std::filesystem::path(root) / "fixture.zip";
    std::ofstream output(archive, std::ios::binary);
    output.write(reinterpret_cast<const char *>(fixture), sizeof(fixture));
    output.close();
    char hash[33] = {};
    char error[128] = {};
    ASSERT_EQ(0, bloom_ra_hash_file("gba", archive.c_str(), root, hash, error, sizeof(error))) << error;
    EXPECT_STREQ("5049a8174a2a65954988d899ff3a03a6", hash);
    std::filesystem::remove_all(root);
}

TEST(BloomRaTest, RejectsSymlinkedRomAtBloomBoundary)
{
    char directory_template[] = "/tmp/bloom-ra-link-XXXXXX";
    const char *root = mkdtemp(directory_template);
    ASSERT_NE(nullptr, root);
    std::filesystem::path rom = std::filesystem::path(root) / "fixture.gba";
    std::filesystem::path link = std::filesystem::path(root) / "linked.gba";
    std::ofstream(rom, std::ios::binary) << "Bloom RA fixture v1";
    std::filesystem::create_symlink(rom, link);
    char hash[33] = {};
    char error[128] = {};
    EXPECT_NE(0, bloom_ra_hash_file("gba", link.c_str(), root, hash, error, sizeof(error)));
    std::filesystem::remove_all(root);
}
