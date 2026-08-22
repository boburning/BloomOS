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

static std::vector<BloomShellSettingsRow> settings(const BloomShellCapabilities &capabilities)
{
    std::vector<BloomShellSettingsRow> result;
    for (size_t row = 0; row < bloom_shell_settings_count(&capabilities); ++row) {
        BloomShellSettingsRow settings_row{};
        if (bloom_shell_settings_row(&capabilities, row, &settings_row) == 0)
            result.push_back(settings_row);
    }
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
    auto rows = settings(capabilities);
    ASSERT_EQ(20U, rows.size());
    EXPECT_EQ(BLOOM_SHELL_SETTINGS_DISPLAY_SECTION, rows.front().id);
    EXPECT_EQ(BLOOM_SHELL_SETTINGS_ABOUT, rows.back().id);
    for (const auto &row : rows) {
        EXPECT_NE(BLOOM_SHELL_SETTINGS_NETWORK_SECTION, row.id);
        EXPECT_NE(BLOOM_SHELL_SETTINGS_DEVELOPER_SECTION, row.id);
    }
    EXPECT_EQ((std::vector<std::string>{"Brightness", "Volume / Mute", "Battery",
                                        "Open Settings"}),
              quick(capabilities));
}

TEST(BloomShellSettings, PlusShowsWifiWithoutFlipOrDeveloperControls)
{
    BloomShellCapabilities capabilities{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(354, 0, &capabilities));
    EXPECT_TRUE(capabilities.wifi);
    EXPECT_FALSE(capabilities.flip);
    EXPECT_EQ((std::vector<std::string>{"Brightness", "Volume / Mute", "Wi-Fi", "Battery",
                                        "Open Settings"}),
              quick(capabilities));
    auto rows = settings(capabilities);
    ASSERT_EQ(25U, rows.size());
    EXPECT_EQ(BLOOM_SHELL_SETTINGS_NETWORK_SECTION, rows[9].id);
    EXPECT_EQ(BLOOM_SHELL_SETTINGS_WIFI, rows[10].id);
    EXPECT_EQ(BLOOM_SHELL_SETTINGS_SSH, rows[11].id);
    EXPECT_EQ(BLOOM_SHELL_SETTINGS_SFTP, rows[12].id);
    EXPECT_EQ(BLOOM_SHELL_SETTINGS_SAMBA, rows[13].id);
}

TEST(BloomShellSettings, FlipAndDeveloperModeExposeOnlyTheirCapabilities)
{
    BloomShellCapabilities capabilities{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(285, 1, &capabilities));
    EXPECT_TRUE(capabilities.wifi);
    EXPECT_TRUE(capabilities.flip);
    EXPECT_TRUE(capabilities.developer_mode);
    auto rows = settings(capabilities);
    ASSERT_EQ(28U, rows.size());
    EXPECT_EQ(BLOOM_SHELL_SETTINGS_DEVELOPER_SECTION, rows[25].id);
    EXPECT_EQ(BLOOM_SHELL_SETTINGS_DIAGNOSTICS, rows.back().id);
}

TEST(BloomShellSettings, ParsesFormatsAndMutatesBoundedNetworkServices)
{
    BloomShellCapabilities capabilities{};
    BloomShellQuickValues values{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(354, 0, &capabilities));
    ASSERT_EQ(0, bloom_shell_network_services_parse(
                     R"({"schema":1,"service":"bloom-network-services","wifi_capable":true,"ssh":{"available":true,"enabled":true},"sftp":{"available":true,"enabled":false},"samba":{"available":true,"enabled":false}})",
                     &values));
    EXPECT_TRUE(values.network_services_ready);
    EXPECT_TRUE(values.ssh_enabled);
    char label[64];
    ASSERT_EQ(0, bloom_shell_settings_row_format(&capabilities, &values, 11, label,
                                                 sizeof(label)));
    EXPECT_STREQ("SSH                       On", label);
    ASSERT_EQ(0, bloom_shell_settings_row_format(&capabilities, &values, 12, label,
                                                 sizeof(label)));
    EXPECT_STREQ("SFTP                      Off", label);

    const auto directory = std::filesystem::temp_directory_path() /
                           ("bloom-network-services-" + std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    const auto script = directory / "adapter";
    const auto arguments = directory / "arguments";
    {
        std::ofstream output(script);
        output << "#!/bin/sh\nprintf '%s' \"$*\" >'" << arguments.string() << "'\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    ASSERT_EQ(0, bloom_shell_network_service_change(
                     &values, BLOOM_SHELL_SETTINGS_SFTP, 1, script.c_str()));
    EXPECT_TRUE(values.sftp_enabled);
    std::ifstream input(arguments);
    std::string recorded;
    std::getline(input, recorded);
    EXPECT_EQ("request sftp enable", recorded);
    std::filesystem::remove_all(directory);
}

TEST(BloomShellSettings, NetworkServicesFailClosedOnMalformedOrUnavailableState)
{
    BloomShellQuickValues values{};
    BloomShellCapabilities capabilities{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(354, 0, &capabilities));
    EXPECT_NE(0, bloom_shell_network_services_parse("{}", &values));
    ASSERT_EQ(0, bloom_shell_network_services_parse(
                     R"({"schema":1,"service":"bloom-network-services","ssh":{"available":false,"enabled":false},"sftp":{"available":true,"enabled":false},"samba":{"available":true,"enabled":false}})",
                     &values));
    char label[64];
    ASSERT_EQ(0, bloom_shell_settings_row_format(&capabilities, &values, 12, label,
                                                 sizeof(label)));
    EXPECT_STREQ("SFTP              requires SSH", label);
    EXPECT_NE(0, bloom_shell_network_service_change(
                     &values, BLOOM_SHELL_SETTINGS_SSH, 1, "/does/not/exist"));
    EXPECT_NE(0, bloom_shell_network_service_change(
                     &values, BLOOM_SHELL_SETTINGS_SFTP, 1, "/does/not/exist"));
}

TEST(BloomShellSettings, UnknownModelsAndOutOfRangeRowsFailClosed)
{
    BloomShellCapabilities capabilities{};
    EXPECT_NE(0, bloom_shell_capabilities_from_model(999, 0, &capabilities));
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(283, 0, &capabilities));
    BloomShellSettingsRow row{};
    EXPECT_NE(0, bloom_shell_settings_row(&capabilities, 20, &row));
    EXPECT_EQ(nullptr, bloom_shell_quick_settings_label(&capabilities, 4));
}

TEST(BloomShellSettings, FlatRowsSkipSectionHeadersAndPreservePosition)
{
    BloomShellCapabilities capabilities{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(354, 0, &capabilities));
    EXPECT_EQ(1U, bloom_shell_settings_first_selectable(&capabilities));
    EXPECT_FALSE(bloom_shell_settings_row_selectable(&capabilities, 3));
    EXPECT_EQ(4U, bloom_shell_settings_next_selectable(&capabilities, 2, 1));
    EXPECT_EQ(2U, bloom_shell_settings_next_selectable(&capabilities, 4, -1));
    EXPECT_EQ(1U, bloom_shell_settings_next_selectable(&capabilities, 1, -1));
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
    EXPECT_NE(0, bloom_shell_quick_settings_format(&capabilities, &values, 4, label, sizeof(label)));
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

TEST(BloomShellSettings, MuteToggleUsesFixedAdapterAndUpdatesOnlyAfterSuccess)
{
    const auto script = std::filesystem::temp_directory_path() /
                        ("bloom-shell-mute-" + std::to_string(getpid()));
    {
        std::ofstream output(script);
        output << "#!/bin/sh\n[ \"$*\" = 'request mute true' ]\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    BloomShellQuickValues values{1, 1, 4, 9, 0, 0};
    ASSERT_EQ(0, bloom_shell_mute_toggle(&values, script.c_str()));
    EXPECT_TRUE(values.mute);
    {
        std::ofstream output(script);
        output << "#!/bin/sh\n[ \"$*\" = 'request volume 9' ]\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    ASSERT_EQ(0, bloom_shell_mute_toggle(&values, script.c_str()));
    EXPECT_FALSE(values.mute);
    std::filesystem::remove(script);
}

TEST(BloomShellSettings, QuickSettingsAActivatesTogglesAndOpensFullSettings)
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("bloom-quick-activate-" + std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    const auto script = directory / "adapter";
    const auto arguments = directory / "arguments";
    {
        std::ofstream output(script);
        output << "#!/bin/sh\nprintf '%s\\n' \"$*\" >>'" << arguments.string() << "'\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    BloomShellCapabilities capabilities{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(354, 0, &capabilities));
    BloomShellQuickValues values{1, 1, 7, 9, 0, 0};
    int open_settings = 0;

    ASSERT_EQ(0, bloom_shell_quick_settings_activate(&capabilities, &values, 1,
                                                     script.c_str(), script.c_str(),
                                                     &open_settings));
    EXPECT_TRUE(values.mute);
    EXPECT_FALSE(open_settings);
    ASSERT_EQ(0, bloom_shell_quick_settings_activate(&capabilities, &values, 2,
                                                     script.c_str(), script.c_str(),
                                                     &open_settings));
    EXPECT_TRUE(values.wifi_enabled);
    EXPECT_FALSE(open_settings);
    ASSERT_EQ(0, bloom_shell_quick_settings_activate(&capabilities, &values, 4,
                                                     script.c_str(), script.c_str(),
                                                     &open_settings));
    EXPECT_TRUE(open_settings);

    std::ifstream input(arguments);
    std::string line;
    ASSERT_TRUE(std::getline(input, line));
    EXPECT_EQ("request mute true", line);
    ASSERT_TRUE(std::getline(input, line));
    EXPECT_EQ("request enable", line);
    std::filesystem::remove_all(directory);
}

TEST(BloomShellSettings, FlatSchemaUsesInlineControlsAndCapabilityRows)
{
    BloomShellCapabilities capabilities{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(354, 0, &capabilities));
    bool brightness = false;
    bool mute = false;
    bool account = false;
    bool achievements = false;
    bool mode = false;
    bool offline = false;
    for (size_t row = 0; row < bloom_shell_settings_count(&capabilities); ++row) {
        BloomShellSettingsRow settings_row{};
        ASSERT_EQ(0, bloom_shell_settings_row(&capabilities, row, &settings_row));
        if (settings_row.id == BLOOM_SHELL_SETTINGS_BRIGHTNESS) {
            brightness = true;
            EXPECT_EQ(BLOOM_SHELL_SETTINGS_ROW_SLIDER, settings_row.kind);
        }
        if (settings_row.id == BLOOM_SHELL_SETTINGS_MUTE) {
            mute = true;
            EXPECT_EQ(BLOOM_SHELL_SETTINGS_ROW_TOGGLE, settings_row.kind);
        }
        if (settings_row.id == BLOOM_SHELL_SETTINGS_RA_ACCOUNT) {
            account = true;
            EXPECT_EQ(BLOOM_SHELL_SETTINGS_ROW_DETAIL, settings_row.kind);
        }
        if (settings_row.id == BLOOM_SHELL_SETTINGS_RA_ENABLED) {
            achievements = true;
            EXPECT_EQ(BLOOM_SHELL_SETTINGS_ROW_TOGGLE, settings_row.kind);
        }
        if (settings_row.id == BLOOM_SHELL_SETTINGS_RA_MODE) {
            mode = true;
            EXPECT_EQ(BLOOM_SHELL_SETTINGS_ROW_ENUM, settings_row.kind);
        }
        if (settings_row.id == BLOOM_SHELL_SETTINGS_RA_OFFLINE) {
            offline = true;
            EXPECT_EQ(BLOOM_SHELL_SETTINGS_ROW_ENUM, settings_row.kind);
        }
    }
    EXPECT_TRUE(brightness);
    EXPECT_TRUE(mute);
    EXPECT_TRUE(account);
    EXPECT_TRUE(achievements);
    EXPECT_TRUE(mode);
    EXPECT_TRUE(offline);
}

TEST(BloomShellSettings, ParsesAndFormatsRetroAchievementsValues)
{
    BloomShellRaValues values{};
    ASSERT_EQ(0, bloom_shell_ra_values_parse(
                     R"({"schema":1,"configured":true,"enabled":true,"authenticated":true,"mode":"softcore","offline_casual":false})",
                     &values));
    EXPECT_TRUE(values.ready);
    EXPECT_TRUE(values.configured);
    EXPECT_TRUE(values.enabled);
    EXPECT_TRUE(values.authenticated);
    EXPECT_FALSE(values.hardcore);
    EXPECT_FALSE(values.offline_casual);

    char label[64];
    ASSERT_EQ(0, bloom_shell_ra_settings_format(
                     &values, BLOOM_SHELL_SETTINGS_RA_ENABLED, label, sizeof(label)));
    EXPECT_NE(std::string::npos, std::string(label).find("On"));
    ASSERT_EQ(0, bloom_shell_ra_settings_format(
                     &values, BLOOM_SHELL_SETTINGS_RA_MODE, label, sizeof(label)));
    EXPECT_NE(std::string::npos, std::string(label).find("Softcore"));
    ASSERT_EQ(0, bloom_shell_ra_settings_format(
                     &values, BLOOM_SHELL_SETTINGS_RA_OFFLINE, label, sizeof(label)));
    EXPECT_NE(std::string::npos, std::string(label).find("Off"));

    EXPECT_NE(0, bloom_shell_ra_values_parse("{}", &values));
    EXPECT_FALSE(values.ready);
    EXPECT_NE(0, bloom_shell_ra_values_parse(
                     R"({"schema":1,"configured":true,"enabled":true,"authenticated":true,"mode":"unknown","offline_casual":false})",
                     &values));
    EXPECT_FALSE(values.ready);
}

TEST(BloomShellSettings, RetroAchievementsChangesUseFixedArgumentsAndCommitAfterSuccess)
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("bloom-ra-settings-" + std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    const auto script = directory / "bloom-ra";
    const auto arguments = directory / "arguments";
    {
        std::ofstream output(script);
        output << "#!/bin/sh\nprintf '%s\\n' \"$*\" >>\"$(dirname \"$0\")/arguments\"\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    BloomShellRaValues values{1, 1, 0, 1, 0, 0};

    ASSERT_EQ(0, bloom_shell_ra_settings_change(
                     &values, BLOOM_SHELL_SETTINGS_RA_ENABLED, 1, script.c_str()));
    EXPECT_TRUE(values.enabled);
    ASSERT_EQ(0, bloom_shell_ra_settings_change(
                     &values, BLOOM_SHELL_SETTINGS_RA_OFFLINE, 1, script.c_str()));
    EXPECT_TRUE(values.offline_casual);
    ASSERT_EQ(0, bloom_shell_ra_settings_change(
                     &values, BLOOM_SHELL_SETTINGS_RA_MODE, 1, script.c_str()));
    EXPECT_TRUE(values.hardcore);

    std::ifstream input(arguments);
    std::string line;
    ASSERT_TRUE(std::getline(input, line));
    EXPECT_EQ("account set enabled true", line);
    ASSERT_TRUE(std::getline(input, line));
    EXPECT_EQ("account set offline-casual automatic", line);
    ASSERT_TRUE(std::getline(input, line));
    EXPECT_EQ("account set mode hardcore", line);

    {
        std::ofstream output(script);
        output << "#!/bin/sh\nexit 1\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    EXPECT_NE(0, bloom_shell_ra_settings_change(
                     &values, BLOOM_SHELL_SETTINGS_RA_ENABLED, -1, script.c_str()));
    EXPECT_TRUE(values.enabled);
    std::filesystem::remove_all(directory);
}

TEST(BloomShellSettings, FlatRowsFormatCanonicalValuesAndStableControlGrammar)
{
    BloomShellCapabilities capabilities{};
    ASSERT_EQ(0, bloom_shell_capabilities_from_model(354, 0, &capabilities));
    BloomShellQuickValues values{};
    values.ready = 1;
    values.brightness = 6;
    values.volume = 11;
    values.mute = 1;
    values.wifi_enabled = 1;
    char label[64];
    ASSERT_EQ(0, bloom_shell_settings_row_format(&capabilities, &values, 1, label, sizeof(label)));
    EXPECT_NE(std::string::npos, std::string(label).find("Brightness"));
    EXPECT_NE(std::string::npos, std::string(label).find("6"));
    ASSERT_EQ(0, bloom_shell_settings_row_format(&capabilities, &values, 5, label, sizeof(label)));
    EXPECT_NE(std::string::npos, std::string(label).find("On"));
    ASSERT_EQ(0, bloom_shell_settings_row_format(&capabilities, &values, 10, label, sizeof(label)));
    EXPECT_NE(std::string::npos, std::string(label).find("Wi-Fi"));
    EXPECT_NE(std::string::npos, std::string(label).find("On"));
    ASSERT_EQ(0, bloom_shell_settings_row_format(&capabilities, &values, 7, label, sizeof(label)));
    EXPECT_NE(std::string::npos, std::string(label).find("A Confirm / B Back"));
    EXPECT_NE(0, bloom_shell_settings_row_format(&capabilities, &values, 99, label, sizeof(label)));
}

TEST(BloomShellSettings, FirstRunStatusIsBoundedAndExplicit)
{
    BloomShellFirstRun first_run{};
    ASSERT_EQ(0, bloom_shell_first_run_parse(
                     R"({"schema":1,"service":"bloom-settings","first_run_complete":false})",
                     &first_run));
    EXPECT_EQ(1, first_run.ready);
    EXPECT_EQ(0, first_run.complete);
    ASSERT_EQ(0, bloom_shell_first_run_parse(
                     R"({"schema":1,"service":"bloom-settings","first_run_complete":true})",
                     &first_run));
    EXPECT_EQ(1, first_run.complete);
    EXPECT_NE(0, bloom_shell_first_run_parse(
                     R"({"schema":1,"service":"other","first_run_complete":false})",
                     &first_run));
    EXPECT_EQ(0, first_run.ready);
    EXPECT_NE(0, bloom_shell_first_run_parse(
                     R"({"schema":1,"service":"bloom-settings","first_run_complete":"no"})",
                     &first_run));
}

TEST(BloomShellSettings, FirstRunFinishUsesFixedOrderedSettingsOperations)
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("bloom-first-run-" + std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    const auto script = directory / "bloom-settings";
    const auto arguments = directory / "arguments";
    {
        std::ofstream output(script);
        output << "#!/bin/sh\n"
                  "printf '%s\\n' \"$*\" >>\"$(dirname \"$0\")/arguments\"\n"
                  "case \"$1\" in\n"
                  "  activate-bloom|complete-first-run) exit 0 ;;\n"
                  "  values) printf '%s\\n' '{\"schema\":1,\"service\":\"bloom-settings\","
                  "\"generation\":2,\"authority\":\"bloom\",\"device\":{"
                  "\"brightness\":7,\"volume\":20,\"mute\":false,"
                  "\"wifi_enabled\":false}}' ;;\n"
                  "  *) exit 2 ;;\n"
                  "esac\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    BloomShellFirstRun first_run{1, 0};
    BloomShellQuickValues values{};
    ASSERT_EQ(0, bloom_shell_first_run_finish(script.c_str(), &first_run, &values));
    EXPECT_EQ(1, first_run.complete);
    EXPECT_EQ(1, values.ready);
    std::ifstream input(arguments);
    std::string line;
    ASSERT_TRUE(std::getline(input, line));
    EXPECT_EQ("activate-bloom", line);
    ASSERT_TRUE(std::getline(input, line));
    EXPECT_EQ("complete-first-run", line);
    ASSERT_TRUE(std::getline(input, line));
    EXPECT_EQ("values", line);
    EXPECT_NE(0, bloom_shell_first_run_finish(script.c_str(), &first_run, &values));
    std::filesystem::remove_all(directory);
}
