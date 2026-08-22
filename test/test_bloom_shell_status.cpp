#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>

#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "../src/bloomShell/bloom_shell_status.h"
}

TEST(BloomShellStatusTest, ParsesBoundedHealthAndUpdateModel)
{
    const char *json =
        "{\"schema\":1,\"healthy\":false,\"checks\":{"
        "\"system\":{\"healthy\":true,\"free_kb\":87031808},"
        "\"update_state\":{\"healthy\":true,\"phase\":\"testing\"},"
        "\"retroachievements\":{\"healthy\":true,\"enabled\":false,"
        "\"state\":\"signed_out\"}}}";
    BloomShellStatus status{};
    ASSERT_EQ(0, bloom_shell_status_parse(json, &status));
    EXPECT_EQ(1, status.ready);
    EXPECT_EQ(0, status.healthy);
    EXPECT_EQ(1, status.system_healthy);
    EXPECT_EQ(87031808U, status.storage_free_kb);
    EXPECT_EQ(1, status.update_healthy);
    EXPECT_EQ(1, status.ra_healthy);
    EXPECT_EQ(0, status.ra_enabled);
    EXPECT_STREQ("testing", status.update_phase);
    EXPECT_STREQ("signed_out", status.ra_state);
}

TEST(BloomShellStatusTest, RejectsMissingOrUnboundedStatusFields)
{
    BloomShellStatus status{};
    EXPECT_NE(0, bloom_shell_status_parse("{}", &status));
    EXPECT_NE(0, bloom_shell_status_parse(
                     "{\"schema\":1,\"healthy\":true,\"checks\":{"
                     "\"system\":{\"healthy\":true,\"free_kb\":87031808},"
                     "\"update_state\":{\"healthy\":true,\"phase\":\"testing-now\"},"
                     "\"retroachievements\":{\"healthy\":true,\"enabled\":true,"
                     "\"state\":\"ready\"}}}",
                     &status));
    EXPECT_EQ(0, status.ready);
}

TEST(BloomShellStatusTest, RefusesNonAbsoluteHealthExecutable)
{
    BloomShellStatus status{};
    EXPECT_NE(0, bloom_shell_status_load("bloomctl", &status));
}

TEST(BloomShellStatusTest, LoadsHealthWithoutUsingAShell)
{
    const auto script = std::filesystem::temp_directory_path() /
                        ("bloom-shell-health-" + std::to_string(getpid()));
    {
        std::ofstream output(script);
        output << "#!/bin/sh\n"
                  "printf '%s\\n' '{\"schema\":1,\"healthy\":true,\"checks\":{"
                  "\"system\":{\"healthy\":true,\"free_kb\":87031808},"
                  "\"update_state\":{\"healthy\":true,\"phase\":\"known_good\"},"
                  "\"retroachievements\":{\"healthy\":true,\"enabled\":true,"
                  "\"state\":\"ready\"}}}'\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    BloomShellStatus status{};
    ASSERT_EQ(0, bloom_shell_status_load(script.c_str(), &status));
    EXPECT_EQ(1, status.ready);
    EXPECT_EQ(1, status.healthy);
    EXPECT_STREQ("known_good", status.update_phase);
    std::filesystem::remove(script);
}

TEST(BloomShellStatusTest, FormatsPlainLanguageSettingsRows)
{
    BloomShellStatus status{};
    status.ready = 1;
    status.system_healthy = 1;
    status.update_healthy = 1;
    status.ra_healthy = 1;
    status.ra_enabled = 0;
    status.storage_free_kb = 87031808;
    snprintf(status.update_phase, sizeof(status.update_phase), "%s", "testing");
    char label[96] = {};
    ASSERT_EQ(0, bloom_shell_status_label(&status, 0, label, sizeof(label)));
    EXPECT_STREQ("System health: Good", label);
    ASSERT_EQ(0, bloom_shell_status_label(&status, 1, label, sizeof(label)));
    EXPECT_STREQ("Updates: Testing an update", label);
    ASSERT_EQ(0, bloom_shell_status_label(&status, 2, label, sizeof(label)));
    EXPECT_STREQ("RetroAchievements: Off", label);
    EXPECT_NE(0, bloom_shell_status_label(&status, 3, label, sizeof(label)));
    EXPECT_NE(0, bloom_shell_status_label(&status, 0, label, 4));
    ASSERT_EQ(0, bloom_shell_storage_label(&status, label, sizeof(label)));
    EXPECT_STREQ("Storage: 83.0 GB free", label);
}

TEST(BloomShellStatusTest, ExportsSupportWithFixedArgumentsWithoutAShell)
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("bloom-shell-export-" + std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    const auto script = directory / "bloomctl";
    const auto arguments = directory / "arguments";
    {
        std::ofstream output(script);
        output << "#!/bin/sh\n"
                  "printf '%s\\n' \"$*\" >\"$(dirname \"$0\")/arguments\"\n"
                  "[ \"$1\" = logs ] && [ \"$2\" = export ]\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    ASSERT_EQ(0, bloom_shell_support_export(script.c_str()));
    std::ifstream input(arguments);
    std::string value;
    std::getline(input, value);
    EXPECT_EQ("logs export", value);
    EXPECT_NE(0, bloom_shell_support_export("bloomctl"));
    std::filesystem::remove_all(directory);
}

TEST(BloomShellStatusTest, ConfirmsUpdateWithFixedArgumentsWithoutAShell)
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("bloom-shell-confirm-" + std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    const auto script = directory / "bloomctl";
    const auto arguments = directory / "arguments";
    {
        std::ofstream output(script);
        output << "#!/bin/sh\n"
                  "printf '%s\\n' \"$*\" >\"$(dirname \"$0\")/arguments\"\n"
                  "[ \"$1\" = update ] && [ \"$2\" = confirm ]\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    ASSERT_EQ(0, bloom_shell_update_confirm(script.c_str()));
    std::ifstream input(arguments);
    std::string value;
    std::getline(input, value);
    EXPECT_EQ("update confirm", value);
    EXPECT_NE(0, bloom_shell_update_confirm("bloomctl"));
    std::filesystem::remove_all(directory);
}

TEST(BloomShellStatusTest, RollsBackUpdateWithFixedArgumentsWithoutAShell)
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("bloom-shell-rollback-" + std::to_string(getpid()));
    std::filesystem::create_directory(directory);
    const auto script = directory / "bloomctl";
    const auto arguments = directory / "arguments";
    {
        std::ofstream output(script);
        output << "#!/bin/sh\n"
                  "printf '%s\\n' \"$*\" >\"$(dirname \"$0\")/arguments\"\n"
                  "[ \"$1\" = update ] && [ \"$2\" = rollback ]\n";
    }
    ASSERT_EQ(0, chmod(script.c_str(), 0700));
    ASSERT_EQ(0, bloom_shell_update_rollback(script.c_str()));
    std::ifstream input(arguments);
    std::string value;
    std::getline(input, value);
    EXPECT_EQ("update rollback", value);
    EXPECT_NE(0, bloom_shell_update_rollback("bloomctl"));
    std::filesystem::remove_all(directory);
}
