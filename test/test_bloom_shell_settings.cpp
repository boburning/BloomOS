#include <gtest/gtest.h>

#include <string>
#include <vector>

extern "C" {
#include "../src/bloomShell/bloom_shell_settings.h"
}

static std::vector<std::string> settings(const BloomShellCapabilities &capabilities)
{
    std::vector<std::string> result;
    for (size_t row = 0; row < bloom_shell_settings_count(&capabilities); ++row)
        result.emplace_back(bloom_shell_settings_label(&capabilities, row));
    return result;
}

static std::vector<std::string> quick(const BloomShellCapabilities &capabilities)
{
    std::vector<std::string> result;
    for (size_t row = 0; row < bloom_shell_quick_settings_count(&capabilities); ++row)
        result.emplace_back(bloom_shell_quick_settings_label(&capabilities, row));
    return result;
}

TEST(BloomShellSettings, OriginalMiniHidesNetworkAndDeveloperControls)
{
    BloomShellCapabilities capabilities{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(283, 0, &capabilities));
    EXPECT_EQ((std::vector<std::string>{"Display", "Audio", "Controls", "Gameplay",
                                        "RetroAchievements", "Appearance", "System"}),
              settings(capabilities));
    EXPECT_EQ((std::vector<std::string>{"Brightness", "Volume / Mute", "Battery"}),
              quick(capabilities));
}

TEST(BloomShellSettings, PlusShowsWifiWithoutFlipOrDeveloperControls)
{
    BloomShellCapabilities capabilities{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(354, 0, &capabilities));
    EXPECT_TRUE(capabilities.wifi);
    EXPECT_FALSE(capabilities.flip);
    EXPECT_EQ((std::vector<std::string>{"Brightness", "Volume / Mute", "Wi-Fi", "Battery"}),
              quick(capabilities));
    EXPECT_EQ("Network", std::string(bloom_shell_settings_label(&capabilities, 4)));
    EXPECT_EQ(nullptr, bloom_shell_settings_label(&capabilities, 8));
}

TEST(BloomShellSettings, FlipAndDeveloperModeExposeOnlyTheirCapabilities)
{
    BloomShellCapabilities capabilities{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(285, 1, &capabilities));
    EXPECT_TRUE(capabilities.wifi);
    EXPECT_TRUE(capabilities.flip);
    EXPECT_TRUE(capabilities.developer_mode);
    EXPECT_EQ("Advanced", std::string(bloom_shell_settings_label(&capabilities, 8)));
}

TEST(BloomShellSettings, UnknownModelsAndOutOfRangeRowsFailClosed)
{
    BloomShellCapabilities capabilities{};
    EXPECT_NE(0, bloom_shell_capabilities_from_model(999, 0, &capabilities));
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(283, 0, &capabilities));
    EXPECT_EQ(nullptr, bloom_shell_settings_label(&capabilities, 7));
    EXPECT_EQ(nullptr, bloom_shell_quick_settings_label(&capabilities, 3));
}
