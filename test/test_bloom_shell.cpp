#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "../src/bloomGameId/bloom_game_id.h"
#include "../src/bloomShell/bloom_shell_launch.h"
}

class BloomShellLaunchTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root_ = std::filesystem::path("/tmp/bloom-session") /
                ("bloom-shell-" + std::to_string(getpid()) + "-" +
                 std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::create_directories(root_);
        request_ = root_ / "request.json";
        session_request_ = root_ / "session.json";
        command_ = root_ / "command.sh";
        sd_root_ = root_ / "Bob's SD";
        launcher_ = sd_root_ / "App/Activity Tracker/launch.sh";
        std::filesystem::create_directories(launcher_.parent_path());
        std::ofstream(launcher_) << "#!/bin/sh\nexit 0\n";
        chmod(launcher_.c_str(), 0755);
        session_ = root_ / "bloom-session";
        std::ofstream(session_) << "#!/bin/sh\n"
                                   "case \"$1\" in\n"
                                   "start) cp \"$2\" \"$BLOOM_TEST_SESSION_REQUEST\" ;;\n"
                                   "transition) [ \"$2\" = PREPARING:STARTING ] ;;\n"
                                   "fail) exit 0 ;;\n"
                                   "*) exit 2 ;;\n"
                                   "esac\n";
        chmod(session_.c_str(), 0755);
        setenv("BLOOM_TEST_SESSION_REQUEST", session_request_.c_str(), 1);
        snprintf(game_.system_id, sizeof(game_.system_id), "gb");
        snprintf(game_.normalized_rom_path, sizeof(game_.normalized_rom_path), "GB/Bob's Game.gb");
        snprintf(game_.launch_path, sizeof(game_.launch_path), "Emu/GB/launch.sh");
        snprintf(app_.app_id, sizeof(app_.app_id), "activity-tracker");
        snprintf(app_.label, sizeof(app_.label), "Activity Tracker");
        snprintf(app_.launch_path, sizeof(app_.launch_path), "App/Activity Tracker/launch.sh");
        snprintf(app_.compatibility, sizeof(app_.compatibility), "bloom-native");
        char normalized[512] = {};
        char error[128] = {};
        ASSERT_EQ(0, bloom_game_id_create("gb", "/mnt/SDCARD/Roms/GB/Bob's Game.gb",
                                          game_.bloom_game_id, sizeof(game_.bloom_game_id), normalized,
                                          sizeof(normalized), error, sizeof(error)));
    }

    void TearDown() override
    {
        unsetenv("BLOOM_TEST_SESSION_REQUEST");
        std::filesystem::remove_all(root_);
    }

    std::filesystem::path root_;
    std::filesystem::path request_;
    std::filesystem::path session_request_;
    std::filesystem::path command_;
    std::filesystem::path sd_root_;
    std::filesystem::path launcher_;
    std::filesystem::path session_;
    BloomLibraryGame game_{};
    BloomLibraryApp app_{};
};

TEST_F(BloomShellLaunchTest, StagesStructuredSessionAndQuotedLegacyBoundary)
{
    char error[256] = {};
    ASSERT_EQ(0, bloom_shell_stage_game(&game_, "gambatte_libretro.so", request_.c_str(),
                                        command_.c_str(), session_request_.c_str(), session_.c_str(),
                                        error, sizeof(error)))
        << error;
    EXPECT_FALSE(std::filesystem::exists(request_));
    EXPECT_TRUE(std::filesystem::exists(session_request_));
    std::ifstream stream(command_);
    std::string command((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ("LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so "
              "\"/mnt/SDCARD/.tmp_update/bin/bloom-launch-run\" \"" +
                  session_request_.string() + "\"\n",
              command);
}

TEST_F(BloomShellLaunchTest, StagesCompatibleAppWithShellSafeAtomicCommand)
{
    char error[256] = {};
    ASSERT_EQ(0, bloom_shell_stage_app(&app_, sd_root_.c_str(), command_.c_str(), error,
                                       sizeof(error)))
        << error;
    std::ifstream stream(command_);
    std::string command((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ("#!/bin/sh\nexec '" + root_.string() + "/Bob'\\''s SD/App/Activity Tracker/launch.sh'\n",
              command);
    struct stat metadata{};
    ASSERT_EQ(0, stat(command_.c_str(), &metadata));
    EXPECT_NE(0, metadata.st_mode & S_IXUSR);
}

TEST_F(BloomShellLaunchTest, AllowsOnionCompatibleApps)
{
    snprintf(app_.compatibility, sizeof(app_.compatibility), "onion-compatible");
    char error[256] = {};
    EXPECT_EQ(0, bloom_shell_stage_app(&app_, sd_root_.c_str(), command_.c_str(), error,
                                       sizeof(error)))
        << error;
}

TEST_F(BloomShellLaunchTest, RejectsUnsafeOrUnsupportedApps)
{
    char error[256] = {};
    for (const char *compatibility : {"mainui-dependent", "development-only"}) {
        snprintf(app_.compatibility, sizeof(app_.compatibility), "%s", compatibility);
        EXPECT_NE(0, bloom_shell_stage_app(&app_, sd_root_.c_str(), command_.c_str(), error,
                                           sizeof(error)));
        EXPECT_FALSE(std::filesystem::exists(command_));
    }

    snprintf(app_.compatibility, sizeof(app_.compatibility), "bloom-native");
    snprintf(app_.launch_path, sizeof(app_.launch_path), "App/../secret.sh");
    EXPECT_NE(0, bloom_shell_stage_app(&app_, sd_root_.c_str(), command_.c_str(), error,
                                       sizeof(error)));
    snprintf(app_.launch_path, sizeof(app_.launch_path), "Emu/GB/launch.sh");
    EXPECT_NE(0, bloom_shell_stage_app(&app_, sd_root_.c_str(), command_.c_str(), error,
                                       sizeof(error)));
}

TEST_F(BloomShellLaunchTest, RejectsMissingNonExecutableSymlinkAndOccupiedBoundaries)
{
    char error[256] = {};
    chmod(launcher_.c_str(), 0644);
    EXPECT_NE(0, bloom_shell_stage_app(&app_, sd_root_.c_str(), command_.c_str(), error,
                                       sizeof(error)));
    chmod(launcher_.c_str(), 0755);
    std::filesystem::remove(launcher_);
    std::filesystem::create_symlink("/bin/true", launcher_);
    EXPECT_NE(0, bloom_shell_stage_app(&app_, sd_root_.c_str(), command_.c_str(), error,
                                       sizeof(error)));
    std::filesystem::remove(launcher_);
    std::ofstream(launcher_) << "#!/bin/sh\nexit 0\n";
    chmod(launcher_.c_str(), 0755);
    std::ofstream(command_) << "occupied";
    EXPECT_NE(0, bloom_shell_stage_app(&app_, sd_root_.c_str(), command_.c_str(), error,
                                       sizeof(error)));
}

TEST_F(BloomShellLaunchTest, RejectsTraversalAndExistingCommandsBeforeStartingSession)
{
    char error[256] = {};
    snprintf(game_.normalized_rom_path, sizeof(game_.normalized_rom_path), "../secret.gb");
    EXPECT_NE(0, bloom_shell_stage_game(&game_, "gambatte_libretro.so", request_.c_str(),
                                        command_.c_str(), session_request_.c_str(), session_.c_str(),
                                        error, sizeof(error)));
    EXPECT_FALSE(std::filesystem::exists(session_request_));

    snprintf(game_.normalized_rom_path, sizeof(game_.normalized_rom_path), "GB/game.gb");
    std::ofstream(command_) << "occupied";
    EXPECT_NE(0, bloom_shell_stage_game(&game_, "gambatte_libretro.so", request_.c_str(),
                                        command_.c_str(), session_request_.c_str(), session_.c_str(),
                                        error, sizeof(error)));
    EXPECT_FALSE(std::filesystem::exists(session_request_));
}
