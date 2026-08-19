#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

extern "C" {
#include "../src/bloomLaunch/bloom_launch.h"
}

namespace {

class BloomLaunchTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root_ = std::filesystem::temp_directory_path() /
                ("bloom-launch-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root_);
        request_ = root_ / "request.json";
        command_ = root_ / "command.sh";
    }

    void TearDown() override { std::filesystem::remove_all(root_); }

    void write_request(const std::string &overrides = "")
    {
        std::ofstream file(request_);
        file << R"({
  "schema": 1,
  "game_id": "bloom-game-v1:2d514749ed2f60ba7a6583d7e36483b113005fd788bab176fc9941256551ad71",
  "system_id": "gb",
  "rom_path": "/mnt/SDCARD/Roms/GB/Bob's $pecial 游戏.zip",
  "launcher": "/mnt/SDCARD/Emu/GB/launch.sh",
  "emulator_type": "retroarch",
  "core": "gambatte_libretro.so",
  "auto_load_state": false,
  "append_configs": [],
  "requested_resolution": null,
  "environment": {})";
        file << overrides << "}\n";
    }

    std::filesystem::path root_;
    std::filesystem::path request_;
    std::filesystem::path command_;
    char error_[256] = {0};
};

TEST_F(BloomLaunchTest, ValidatesStructuredRequestAndQuotesLegacyBoundary)
{
    write_request();
    ASSERT_EQ(0, bloom_launch_validate_file(request_.c_str(), error_, sizeof(error_))) << error_;
    ASSERT_EQ(0, bloom_launch_write_legacy(request_.c_str(), command_.c_str(), error_, sizeof(error_))) << error_;

    std::ifstream file(command_);
    std::string command((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_EQ("LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so \"/mnt/SDCARD/Emu/GB/launch.sh\" "
              "\"/mnt/SDCARD/Roms/GB/Bob's $pecial 游戏.zip\"\n",
              command);
    EXPECT_EQ((std::filesystem::perms::owner_exec & std::filesystem::status(command_).permissions()),
              std::filesystem::perms::owner_exec);
}

TEST_F(BloomLaunchTest, CreatesCanonicalJsonWithoutTreatingPathsAsCode)
{
    ASSERT_EQ(0, bloom_launch_create_file(
                     request_.c_str(),
                     "bloom-game-v1:2d514749ed2f60ba7a6583d7e36483b113005fd788bab176fc9941256551ad71", "gb",
                     "/mnt/SDCARD/Roms/GB/Bob's $pecial 游戏.zip",
                     "/mnt/SDCARD/Emu/GB/launch.sh", "retroarch",
                     "gambatte_libretro.so", 0, error_, sizeof(error_)))
        << error_;
    EXPECT_EQ(0, bloom_launch_validate_file(request_.c_str(), error_, sizeof(error_))) << error_;
    std::ifstream file(request_);
    std::string request((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(request.find("Bob's $pecial"), std::string::npos);
    EXPECT_NE(request.find("\"environment\":{}"), std::string::npos);
    EXPECT_NE(request.find("\"achievements\":{\"enabled\":false"), std::string::npos);
    char core[128] = {};
    EXPECT_EQ(0, bloom_launch_get_string(request_.c_str(), "core", core, sizeof(core), error_, sizeof(error_)));
    EXPECT_STREQ("gambatte_libretro.so", core);
    EXPECT_NE(0, bloom_launch_get_string(request_.c_str(), "environment", core, sizeof(core), error_, sizeof(error_)));
}

TEST_F(BloomLaunchTest, AddsValidatedDirectAchievementPolicy)
{
    ASSERT_EQ(0, bloom_launch_create_file(
                     request_.c_str(),
                     "bloom-game-v1:2d514749ed2f60ba7a6583d7e36483b113005fd788bab176fc9941256551ad71", "gb",
                     "/mnt/SDCARD/Roms/GB/Bob's $pecial 游戏.zip", "/mnt/SDCARD/Emu/GB/launch.sh", "retroarch",
                     "gambatte_libretro.so", 0, error_, sizeof(error_)));
    ASSERT_EQ(0, bloom_launch_set_achievements(request_.c_str(), 1, "softcore", "direct", 1234, "untested",
                                               error_, sizeof(error_)))
        << error_;
    EXPECT_EQ(0, bloom_launch_validate_file(request_.c_str(), error_, sizeof(error_)));
    std::ifstream file(request_);
    std::string request((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(request.find("\"ra_game_id\":1234"), std::string::npos);
}

TEST_F(BloomLaunchTest, RejectsHardcoreProxyWithoutChangingExistingRequest)
{
    write_request();
    std::ifstream before_file(request_);
    std::string before((std::istreambuf_iterator<char>(before_file)), std::istreambuf_iterator<char>());
    EXPECT_NE(0, bloom_launch_set_achievements(request_.c_str(), 1, "hardcore", "proxy", 1234, "untested",
                                               error_, sizeof(error_)));
    std::ifstream after_file(request_);
    std::string after((std::istreambuf_iterator<char>(after_file)), std::istreambuf_iterator<char>());
    EXPECT_EQ(before, after);
}

TEST_F(BloomLaunchTest, HardcoreDisablesResumeAndProhibitedInputPaths)
{
    ASSERT_EQ(0, bloom_launch_create_file(
                     request_.c_str(),
                     "bloom-game-v1:2d514749ed2f60ba7a6583d7e36483b113005fd788bab176fc9941256551ad71", "gb",
                     "/mnt/SDCARD/Roms/GB/Bob's $pecial 游戏.zip", "/mnt/SDCARD/Emu/GB/launch.sh", "retroarch",
                     "gambatte_libretro.so", 0, error_, sizeof(error_)));
    ASSERT_EQ(0, bloom_launch_set_achievements(request_.c_str(), 1, "hardcore", "direct", 1234, "untested",
                                               error_, sizeof(error_)));
    std::filesystem::create_directories("/tmp/bloom-session");
    auto config_path =
        std::filesystem::path("/tmp/bloom-session") / ("hardcore-" + std::to_string(getpid()) + ".cfg");
    ASSERT_EQ(0, bloom_launch_write_ra_config(request_.c_str(), config_path.c_str(), "BloomUser", "token123", nullptr,
                                              error_, sizeof(error_)))
        << error_;
    std::ifstream config_file(config_path);
    std::string config((std::istreambuf_iterator<char>(config_file)), std::istreambuf_iterator<char>());
    EXPECT_NE(config.find("cheevos_hardcore_mode_enable = \"true\""), std::string::npos);
    EXPECT_NE(config.find("savestate_auto_load = \"false\""), std::string::npos);
    EXPECT_NE(config.find("rewind_enable = \"false\""), std::string::npos);
    EXPECT_NE(config.find("run_ahead_enabled = \"false\""), std::string::npos);
    EXPECT_NE(config.find("input_load_state = \"nul\""), std::string::npos);
    EXPECT_NE(config.find("input_frame_advance = \"nul\""), std::string::npos);
    EXPECT_NE(config.find("input_slowmotion = \"nul\""), std::string::npos);
    EXPECT_NE(config.find("input_cheat_toggle = \"nul\""), std::string::npos);
    EXPECT_EQ(config.find("cheevos_custom_host"), std::string::npos);
    std::filesystem::remove(config_path);
}

TEST_F(BloomLaunchTest, HardcoreRejectsAutomaticStateLoading)
{
    ASSERT_EQ(0, bloom_launch_create_file(
                     request_.c_str(),
                     "bloom-game-v1:2d514749ed2f60ba7a6583d7e36483b113005fd788bab176fc9941256551ad71", "gb",
                     "/mnt/SDCARD/Roms/GB/Bob's $pecial 游戏.zip", "/mnt/SDCARD/Emu/GB/launch.sh", "retroarch",
                     "gambatte_libretro.so", 1, error_, sizeof(error_)));
    EXPECT_NE(0, bloom_launch_set_achievements(request_.c_str(), 1, "hardcore", "direct", 1234, "untested",
                                               error_, sizeof(error_)));
    EXPECT_NE(std::string(error_).find("auto-load"), std::string::npos);
}

TEST_F(BloomLaunchTest, WritesSessionOnlyRaConfigAndLeavesPermanentConfigUntouched)
{
    ASSERT_EQ(0, bloom_launch_create_file(
                     request_.c_str(),
                     "bloom-game-v1:2d514749ed2f60ba7a6583d7e36483b113005fd788bab176fc9941256551ad71", "gb",
                     "/mnt/SDCARD/Roms/GB/Bob's $pecial 游戏.zip", "/mnt/SDCARD/Emu/GB/launch.sh", "retroarch",
                     "gambatte_libretro.so", 0, error_, sizeof(error_)));
    ASSERT_EQ(0, bloom_launch_set_achievements(request_.c_str(), 1, "softcore", "proxy", 1234, "untested",
                                               error_, sizeof(error_)));
    auto permanent = root_ / "retroarch.cfg";
    std::filesystem::create_directories("/tmp/bloom-session");
    auto temporary = std::filesystem::path("/tmp/bloom-session") / ("ra-test-" + std::to_string(getpid()) + ".cfg");
    std::ofstream(permanent) << "video_driver = \"sdl\"\n";
    ASSERT_EQ(0, bloom_launch_write_ra_config(request_.c_str(), temporary.c_str(), "BloomUser", "token123",
                                              "127.0.0.1:12345", error_, sizeof(error_)))
        << error_;
    std::ifstream permanent_file(permanent);
    std::string permanent_text((std::istreambuf_iterator<char>(permanent_file)), std::istreambuf_iterator<char>());
    EXPECT_EQ("video_driver = \"sdl\"\n", permanent_text);
    std::ifstream temporary_file(temporary);
    std::string config((std::istreambuf_iterator<char>(temporary_file)), std::istreambuf_iterator<char>());
    EXPECT_NE(config.find("cheevos_custom_host = \"127.0.0.1:12345\""), std::string::npos);
    EXPECT_NE(config.find("cheevos_token = \"token123\""), std::string::npos);
    auto permissions = std::filesystem::status(temporary).permissions();
    EXPECT_EQ(std::filesystem::perms::none,
              permissions & (std::filesystem::perms::group_all | std::filesystem::perms::others_all));
    std::ifstream request_file(request_);
    std::string request((std::istreambuf_iterator<char>(request_file)), std::istreambuf_iterator<char>());
    EXPECT_NE(request.find(temporary.string()), std::string::npos);
    std::filesystem::remove(temporary);
}

TEST_F(BloomLaunchTest, RejectsCorePathsInsteadOfCoreBasenames)
{
    write_request();
    std::ifstream input(request_);
    std::string request((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    size_t core = request.find("gambatte_libretro.so");
    ASSERT_NE(core, std::string::npos);
    request.replace(core, strlen("gambatte_libretro.so"), "../gambatte_libretro.so");
    std::ofstream(request_) << request;
    EXPECT_NE(0, bloom_launch_validate_file(request_.c_str(), error_, sizeof(error_)));
}

TEST_F(BloomLaunchTest, RejectsGameIdThatDoesNotMatchSystemAndRom)
{
    write_request();
    std::ifstream input(request_);
    std::string request((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    size_t game_id = request.find("2d514749");
    ASSERT_NE(game_id, std::string::npos);
    request.replace(game_id, strlen("2d514749"), "01234567");
    std::ofstream(request_) << request;
    EXPECT_NE(0, bloom_launch_validate_file(request_.c_str(), error_, sizeof(error_)));
    EXPECT_NE(std::string(error_).find("does not match"), std::string::npos);
}

TEST_F(BloomLaunchTest, RejectsUnknownAndDuplicateFields)
{
    write_request(",\"schema\":1");
    EXPECT_NE(0, bloom_launch_validate_file(request_.c_str(), error_, sizeof(error_)));
    EXPECT_NE(std::string(error_).find("duplicate"), std::string::npos);

    write_request(",\"surprise\":true");
    EXPECT_NE(0, bloom_launch_validate_file(request_.c_str(), error_, sizeof(error_)));
    EXPECT_NE(std::string(error_).find("unknown"), std::string::npos);
}

TEST_F(BloomLaunchTest, RejectsTraversalAndDoesNotReplaceExistingCommand)
{
    write_request();
    std::ifstream request_file(request_);
    std::string request((std::istreambuf_iterator<char>(request_file)), std::istreambuf_iterator<char>());
    size_t position = request.find("/Roms/GB/");
    ASSERT_NE(position, std::string::npos);
    request.replace(position, strlen("/Roms/GB/"), "/Roms/GB/../");
    std::ofstream(request_) << request;
    std::ofstream(command_) << "preserve me\n";

    EXPECT_NE(0, bloom_launch_write_legacy(request_.c_str(), command_.c_str(), error_, sizeof(error_)));
    std::ifstream file(command_);
    std::string command((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_EQ("preserve me\n", command);
}

TEST_F(BloomLaunchTest, KeepsUnrepresentableLegacyPathsOutOfShell)
{
    ASSERT_EQ(0, bloom_launch_create_file(
                     request_.c_str(),
                     "bloom-game-v1:8d875cf8a8944eea3bbd7c811e0c9cb56f87b14f1f66b2adb9c1d6d1f3b546f5", "gb",
                     "/mnt/SDCARD/Roms/GB/Unsafe`Name.gb",
                     "/mnt/SDCARD/Emu/GB/launch.sh", "retroarch",
                     "gambatte_libretro.so", 0, error_, sizeof(error_)));
    EXPECT_NE(0, bloom_launch_write_legacy(request_.c_str(), command_.c_str(), error_, sizeof(error_)));
    EXPECT_NE(std::string(error_).find("legacy MainUI boundary"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(command_));
}

} // namespace
