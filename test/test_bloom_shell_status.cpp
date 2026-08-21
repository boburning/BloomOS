#include <gtest/gtest.h>

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
        "\"system\":{\"healthy\":true},"
        "\"update_state\":{\"healthy\":true,\"phase\":\"testing\"},"
        "\"retroachievements\":{\"healthy\":true,\"enabled\":false,"
        "\"state\":\"signed_out\"}}}";
    BloomShellStatus status{};
    ASSERT_EQ(0, bloom_shell_status_parse(json, &status));
    EXPECT_EQ(1, status.ready);
    EXPECT_EQ(0, status.healthy);
    EXPECT_EQ(1, status.system_healthy);
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
                     "\"system\":{\"healthy\":true},"
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
                  "\"system\":{\"healthy\":true},"
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
