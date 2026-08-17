#include <gtest/gtest.h>

#include <string>

extern "C" {
#include "../src/bloomGameId/bloom_game_id.h"
}

namespace {

TEST(BloomGameIdTest, CreatesStableVersionedIdentityAndDebugPath)
{
    char game_id[BLOOM_GAME_ID_LENGTH + 1] = {};
    char relative[4096] = {};
    char error[256] = {};
    ASSERT_EQ(0, bloom_game_id_create("gb", "/mnt/SDCARD/Roms//GB/./Pokemon Red (USA).zip", game_id,
                                      sizeof(game_id), relative, sizeof(relative), error, sizeof(error)))
        << error;
    EXPECT_STREQ("GB/Pokemon Red (USA).zip", relative);
    EXPECT_STREQ("bloom-game-v1:c959a509315c5cb92585e257f6bd8ce1745efc0ce0e4cbf88f11f9f02fe1fdd4", game_id);
    EXPECT_TRUE(bloom_game_id_valid(game_id));
}

TEST(BloomGameIdTest, SystemAndRelativePathBothAffectIdentity)
{
    char first[BLOOM_GAME_ID_LENGTH + 1], second[BLOOM_GAME_ID_LENGTH + 1], relative[4096], error[256] = {};
    ASSERT_EQ(0, bloom_game_id_create("gb", "/mnt/SDCARD/Roms/GB/Test.zip", first, sizeof(first), relative,
                                      sizeof(relative), error, sizeof(error)));
    ASSERT_EQ(0, bloom_game_id_create("gbc", "/mnt/SDCARD/Roms/GB/Test.zip", second, sizeof(second), relative,
                                      sizeof(relative), error, sizeof(error)));
    EXPECT_STRNE(first, second);
}

TEST(BloomGameIdTest, RejectsOutsideTraversalAndDirectoryPaths)
{
    char game_id[BLOOM_GAME_ID_LENGTH + 1], relative[4096], error[256] = {};
    EXPECT_NE(0, bloom_game_id_create("gb", "/mnt/SDCARD/Roms/GB/../GBC/Test.zip", game_id, sizeof(game_id),
                                      relative, sizeof(relative), error, sizeof(error)));
    EXPECT_NE(0, bloom_game_id_create("gb", "/tmp/Test.zip", game_id, sizeof(game_id), relative, sizeof(relative),
                                      error, sizeof(error)));
    EXPECT_NE(0, bloom_game_id_create("gb", "/mnt/SDCARD/Roms/GB/", game_id, sizeof(game_id), relative,
                                      sizeof(relative), error, sizeof(error)));
}

TEST(BloomGameIdTest, RejectsMalformedIdentifiers)
{
    EXPECT_FALSE(bloom_game_id_valid("bloom-game-v1:test"));
    EXPECT_FALSE(bloom_game_id_valid(
        "bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeF"));
}

} // namespace
