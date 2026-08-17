#include <gtest/gtest.h>

extern "C" {
#include "../src/bloomGameId/bloom_game_id.h"
#include "../src/gameSwitcher/gameSwitcherIdentity.h"
}

TEST(GameSwitcherIdentity, MapsOnlyDeviceProvenLaunchersToCanonicalGameIds)
{
    struct example {
        const char *folder;
        const char *system_id;
    } examples[] = {{"GB", "gb"}, {"GBC", "gbc"}, {"GBA", "gba"}, {"FC", "nes"}, {"SFC", "snes"}, {"PS", "psx"}};

    for (const auto &example : examples) {
        std::string launcher = std::string("/mnt/SDCARD/Emu/") + example.folder + "/launch.sh";
        std::string rom = std::string("/mnt/SDCARD/Roms/") + example.folder + "/Game.zip";
        char actual[BLOOM_GAME_ID_LENGTH + 1] = {};
        char expected[BLOOM_GAME_ID_LENGTH + 1] = {};
        char relative[4096] = {};
        char error[128] = {};
        ASSERT_EQ(gameswitcher_game_id(launcher.c_str(), rom.c_str(), actual, sizeof(actual)), 0);
        ASSERT_EQ(bloom_game_id_create(example.system_id, rom.c_str(), expected, sizeof(expected), relative,
                                       sizeof(relative), error, sizeof(error)),
                  0);
        EXPECT_STREQ(actual, expected);
    }
}

TEST(GameSwitcherIdentity, DefersUnknownLaunchersAndRejectsUnsafeRomPaths)
{
    char game_id[BLOOM_GAME_ID_LENGTH + 1] = {};
    EXPECT_NE(gameswitcher_game_id("/mnt/SDCARD/Emu/ARCADE/launch.sh", "/mnt/SDCARD/Roms/ARCADE/Game.zip",
                                   game_id, sizeof(game_id)),
              0);
    EXPECT_NE(gameswitcher_game_id("/mnt/SDCARD/Emu/GB/launch.sh", "/mnt/SDCARD/Roms/GB/../Secret.zip", game_id,
                                   sizeof(game_id)),
              0);
}

TEST(GameSwitcherIdentity, CreatesCollisionResistantScreenshotPathFromCanonicalDigest)
{
    const char *game_id =
        "bloom-game-v1:2d514749ed2f60ba7a6583d7e36483b113005fd788bab176fc9941256551ad71";
    char path[256] = {};
    ASSERT_EQ(gameswitcher_romscreen_path(game_id, path, sizeof(path)), 0);
    EXPECT_STREQ(path,
                 "/mnt/SDCARD/Saves/CurrentProfile/romScreens/"
                 "2d514749ed2f60ba7a6583d7e36483b113005fd788bab176fc9941256551ad71.png");

    EXPECT_NE(gameswitcher_romscreen_path("legacy-id", path, sizeof(path)), 0);
    char too_small[8] = {};
    EXPECT_NE(gameswitcher_romscreen_path(game_id, too_small, sizeof(too_small)), 0);
}
