#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

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

TEST(BloomShellSettings, ParsesAndFormatsBoundedCanonicalValues)
{
    BloomShellCapabilities capabilities{};
    BloomShellQuickValues values{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(354, 0, &capabilities));
    ASSERT_EQ(0, bloom_shell_quick_values_parse(
                     R"({"schema":1,"service":"bloom-settings","generation":8,"authority":"bloom","device":{"brightness":7,"volume":12,"mute":true,"wifi_enabled":false}})",
                     &values));
    EXPECT_TRUE(values.ready);
    EXPECT_EQ(8, values.generation);
    char label[64];
    ASSERT_EQ(0, bloom_shell_quick_settings_format(&capabilities, &values, 0, label, sizeof(label)));
    EXPECT_STREQ("Brightness: 7", label);
    ASSERT_EQ(0, bloom_shell_quick_settings_format(&capabilities, &values, 1, label, sizeof(label)));
    EXPECT_STREQ("Volume: Muted / 12", label);
    ASSERT_EQ(0, bloom_shell_quick_settings_format(&capabilities, &values, 2, label, sizeof(label)));
    EXPECT_STREQ("Wi-Fi: Off", label);
}

TEST(BloomShellSettings, RejectsMalformedOrOutOfRangeValues)
{
    BloomShellQuickValues values{};
    EXPECT_NE(0, bloom_shell_quick_values_parse("{}", &values));
    EXPECT_NE(0, bloom_shell_quick_values_parse(
                     R"({"schema":1,"service":"bloom-settings","generation":1,"authority":"bloom","device":{"brightness":11,"volume":12,"mute":false,"wifi_enabled":true}})",
                     &values));
    EXPECT_FALSE(values.ready);
}

TEST(BloomShellSettings, UnavailableValuesAreExplicit)
{
    BloomShellCapabilities capabilities{};
    BloomShellQuickValues values{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(283, 0, &capabilities));
    char label[64];
    ASSERT_EQ(0, bloom_shell_quick_settings_format(&capabilities, &values, 0, label, sizeof(label)));
    EXPECT_STREQ("Brightness: unavailable", label);
    EXPECT_NE(0, bloom_shell_quick_settings_format(&capabilities, &values, 3, label, sizeof(label)));
}

TEST(BloomShellSettings, LoadsCanonicalValuesWithoutUsingAShell)
{
    const auto script = std::filesystem::temp_directory_path() /
                        ("bloom-shell-values-" + std::to_string(getpid()));
    {
        std::ofstream output(script);
        output << "#!/bin/sh\n"
                  "[ \"$1\" = values ] || exit 2\n"
                  "printf '%s\\n' '{\"schema\":1,\"service\":\"bloom-settings\","
                  "\"generation\":3,\"authority\":\"bloom\",\"device\":{"
                  "\"brightness\":4,\"volume\":9,\"mute\":false,"
                  "\"wifi_enabled\":true}}'\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    BloomShellQuickValues values{};
    ASSERT_EQ(0, bloom_shell_quick_values_load(script.c_str(), &values));
    EXPECT_EQ(4, values.brightness);
    EXPECT_EQ(9, values.volume);
    EXPECT_TRUE(values.wifi_enabled);
    std::filesystem::remove(script);
}

TEST(BloomShellSettings, SuccessfulFixedRequestUpdatesValueAndFailureDoesNot)
{
    const auto directory = std::filesystem::temp_directory_path();
    const auto script = directory / ("bloom-shell-control-" + std::to_string(getpid()));
    const auto arguments = directory / ("bloom-shell-control-args-" + std::to_string(getpid()));
    {
        std::ofstream output(script);
        output << "#!/bin/sh\nprintf '%s' \"$*\" > '" << arguments.string() << "'\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    BloomShellCapabilities capabilities{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(354, 0, &capabilities));
    BloomShellQuickValues values{1, 1, 4, 9, 1, 0};
    ASSERT_EQ(0, bloom_shell_quick_settings_adjust(&capabilities, &values, 1, 1,
                                                   script.c_str(), script.c_str()));
    EXPECT_EQ(10, values.volume);
    EXPECT_FALSE(values.mute);
    std::ifstream input(arguments);
    std::string recorded;
    std::getline(input, recorded);
    EXPECT_EQ("request volume 10", recorded);

    {
        std::ofstream output(script);
        output << "#!/bin/sh\nexit 1\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    EXPECT_NE(0, bloom_shell_quick_settings_adjust(&capabilities, &values, 0, 1,
                                                   script.c_str(), script.c_str()));
    EXPECT_EQ(4, values.brightness);
    std::filesystem::remove(script);
    std::filesystem::remove(arguments);
}

TEST(BloomShellSettings, ParsesAndFormatsBoundedBatteryState)
{
    BloomShellCapabilities capabilities{};
    BloomShellQuickValues values{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(354, 0, &capabilities));
    ASSERT_EQ(0, bloom_shell_quick_battery_parse(
                     R"({"schema":1,"service":"bloom-platform","battery":{"available":true,"source":"axp_live","capacity":81,"charging":true}})",
                     &values));
    char label[64];
    ASSERT_EQ(0, bloom_shell_quick_settings_format(&capabilities, &values, 3, label,
                                                   sizeof(label)));
    EXPECT_STREQ("Battery: 81% / Charging", label);

    EXPECT_NE(0, bloom_shell_quick_battery_parse(
                     R"({"schema":1,"service":"bloom-platform","battery":{"available":true,"source":"axp_live","capacity":101,"charging":false}})",
                     &values));
}

TEST(BloomShellSettings, EveryVisibleCategoryMapsToABoundedDetailPage)
{
    BloomShellCapabilities capabilities{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(354, 0, &capabilities));
    for (size_t row = 0; row < bloom_shell_settings_count(&capabilities); ++row) {
        BloomShellSettingsPage page = bloom_shell_settings_page(&capabilities, row);
        EXPECT_NE(BLOOM_SHELL_SETTINGS_TOP, page);
        EXPECT_GT(bloom_shell_settings_page_count(page), 0U);
    }
    EXPECT_EQ(BLOOM_SHELL_SETTINGS_TOP, bloom_shell_settings_page(&capabilities, 99));
}

TEST(BloomShellSettings, DetailPagesUseCanonicalValuesAndStableControlGrammar)
{
    BloomShellQuickValues values{};
    values.ready = 1;
    values.brightness = 6;
    values.volume = 11;
    values.mute = 1;
    values.wifi_enabled = 1;
    char label[64];
    ASSERT_EQ(0, bloom_shell_settings_page_format(BLOOM_SHELL_SETTINGS_DISPLAY, &values, 0,
                                                  label, sizeof(label)));
    EXPECT_STREQ("Brightness: 6", label);
    ASSERT_EQ(0, bloom_shell_settings_page_format(BLOOM_SHELL_SETTINGS_AUDIO, &values, 1, label,
                                                  sizeof(label)));
    EXPECT_STREQ("Mute: On", label);
    ASSERT_EQ(0, bloom_shell_settings_page_format(BLOOM_SHELL_SETTINGS_NETWORK, &values, 0, label,
                                                  sizeof(label)));
    EXPECT_STREQ("Wi-Fi: On", label);
    ASSERT_EQ(0, bloom_shell_settings_page_format(BLOOM_SHELL_SETTINGS_CONTROLS, &values, 5, label,
                                                  sizeof(label)));
    EXPECT_STREQ("MENU: GameSwitcher", label);
    EXPECT_NE(0, bloom_shell_settings_page_format(BLOOM_SHELL_SETTINGS_CONTROLS, &values, 6,
                                                  label, sizeof(label)));
}
