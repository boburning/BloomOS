#include <gtest/gtest.h>

extern "C" {
#include "../src/bloomShell/bloom_shell_games.h"
}

static BloomLibrarySystem make_system(const char *id, const char *label)
{
    BloomLibrarySystem system{};
    snprintf(system.system_id, sizeof(system.system_id), "%s", id);
    snprintf(system.label, sizeof(system.label), "%s", label);
    return system;
}

TEST(BloomShellGames, SwitchesSystemsWithoutResettingIndependentSelection)
{
    BloomShellGamesBrowser browser{};
    bloom_shell_games_init(&browser);
    BloomLibrarySystem gb = make_system("gb", "Game Boy");
    BloomLibrarySystem gba = make_system("gba", "Game Boy Advance");
    ASSERT_EQ(0, bloom_shell_games_add(&browser, &gb, 0, 20, "gambatte_libretro.so"));
    ASSERT_EQ(0, bloom_shell_games_add(&browser, &gba, 20, 30, "gpsp_libretro.so"));
    for (int step = 0; step < 4; ++step)
        bloom_shell_games_step(&browser, 1, 6);
    EXPECT_EQ(4U, bloom_shell_games_current(&browser)->focus.selected);
    bloom_shell_games_switch(&browser, 1);
    for (int step = 0; step < 7; ++step)
        bloom_shell_games_step(&browser, 1, 6);
    EXPECT_EQ(7U, bloom_shell_games_current(&browser)->focus.selected);
    bloom_shell_games_switch(&browser, -1);
    EXPECT_EQ(4U, bloom_shell_games_current(&browser)->focus.selected);
}

TEST(BloomShellGames, DeliberateHoldAcceleratesAndReleaseResets)
{
    BloomShellGamesBrowser browser{};
    bloom_shell_games_init(&browser);
    BloomLibrarySystem gb = make_system("gb", "Game Boy");
    ASSERT_EQ(0, bloom_shell_games_add(&browser, &gb, 0, 100, "gambatte_libretro.so"));
    for (int repeat = 0; repeat < 11; ++repeat)
        bloom_shell_games_step(&browser, 1, 8);
    EXPECT_EQ(13U, bloom_shell_games_current(&browser)->focus.selected);
    bloom_shell_games_release(&browser);
    bloom_shell_games_step(&browser, 1, 8);
    EXPECT_EQ(14U, bloom_shell_games_current(&browser)->focus.selected);
}

TEST(BloomShellGames, RejectsEmptyOrUnboundedSystems)
{
    BloomShellGamesBrowser browser{};
    bloom_shell_games_init(&browser);
    BloomLibrarySystem system = make_system("gb", "Game Boy");
    EXPECT_NE(0, bloom_shell_games_add(&browser, &system, 0, 0, "gambatte_libretro.so"));
    EXPECT_EQ(nullptr, bloom_shell_games_current(&browser));
}

TEST(BloomShellGames, DefaultCoresCoverTheProvenStructuredSystemsOnly)
{
    EXPECT_STREQ("gambatte_libretro.so", bloom_shell_games_default_core("gb"));
    EXPECT_STREQ("gpsp_libretro.so", bloom_shell_games_default_core("gba"));
    EXPECT_STREQ("fceumm_libretro.so", bloom_shell_games_default_core("nes"));
    EXPECT_STREQ("fake08_libretro.so", bloom_shell_games_default_core("pico8"));
    EXPECT_STREQ("scummvm_libretro.so", bloom_shell_games_default_core("scummvm"));
    EXPECT_EQ(nullptr, bloom_shell_games_default_core("ports"));
    EXPECT_EQ(nullptr, bloom_shell_games_default_core(nullptr));
}
