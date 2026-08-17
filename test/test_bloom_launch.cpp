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
  "game_id": "bloom-game-v1:test",
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
    ASSERT_EQ(0, bloom_launch_create_file(request_.c_str(), "bloom-game-v1:test", "gb",
                                          "/mnt/SDCARD/Roms/GB/Bob's $pecial 游戏.zip",
                                          "/mnt/SDCARD/Emu/GB/launch.sh", "retroarch",
                                          "gambatte_libretro.so", 0, error_, sizeof(error_)))
        << error_;
    EXPECT_EQ(0, bloom_launch_validate_file(request_.c_str(), error_, sizeof(error_))) << error_;
    std::ifstream file(request_);
    std::string request((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_NE(request.find("Bob's $pecial"), std::string::npos);
    EXPECT_NE(request.find("\"environment\":{}"), std::string::npos);
    char core[128] = {};
    EXPECT_EQ(0, bloom_launch_get_string(request_.c_str(), "core", core, sizeof(core), error_, sizeof(error_)));
    EXPECT_STREQ("gambatte_libretro.so", core);
    EXPECT_NE(0, bloom_launch_get_string(request_.c_str(), "environment", core, sizeof(core), error_, sizeof(error_)));
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
    ASSERT_EQ(0, bloom_launch_create_file(request_.c_str(), "bloom-game-v1:test", "gb",
                                          "/mnt/SDCARD/Roms/GB/Unsafe`Name.gb",
                                          "/mnt/SDCARD/Emu/GB/launch.sh", "retroarch",
                                          "gambatte_libretro.so", 0, error_, sizeof(error_)));
    EXPECT_NE(0, bloom_launch_write_legacy(request_.c_str(), command_.c_str(), error_, sizeof(error_)));
    EXPECT_NE(std::string(error_).find("legacy MainUI boundary"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(command_));
}

} // namespace
