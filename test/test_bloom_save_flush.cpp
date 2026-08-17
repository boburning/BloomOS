#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

extern "C" {
#include "../src/bloomSaveFlush/bloom_save_flush.h"
}

namespace {

class BloomSaveFlushTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root_ = std::filesystem::temp_directory_path() /
                ("bloom-save-flush-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root_ / "RetroArch/.retroarch/cores");
        std::ofstream(root_ / "RetroArch/.retroarch/cores/gambatte_libretro.info") << "corename = \"Gambatte\"\n";
    }

    void TearDown() override { std::filesystem::remove_all(root_); }

    int flush()
    {
        return bloom_save_flush(root_.c_str(), "gambatte_libretro.so", &result_, error_, sizeof(error_));
    }

    std::filesystem::path root_;
    bloom_save_flush_result result_ = {};
    char error_[256] = {};
};

TEST_F(BloomSaveFlushTest, FlushesOnlyCoreScopedSaveAndStateTrees)
{
    std::filesystem::create_directories(root_ / "Saves/CurrentProfile/saves/Gambatte/nested");
    std::filesystem::create_directories(root_ / "Saves/CurrentProfile/states/Gambatte");
    std::ofstream(root_ / "Saves/CurrentProfile/saves/Gambatte/game.sav") << "save";
    std::ofstream(root_ / "Saves/CurrentProfile/saves/Gambatte/nested/game.rtc") << "rtc";
    std::ofstream(root_ / "Saves/CurrentProfile/states/Gambatte/game.state") << "state";
    ASSERT_EQ(0, flush()) << error_;
    EXPECT_STREQ("Gambatte", result_.corename);
    EXPECT_EQ(3U, result_.files_flushed);
    EXPECT_EQ(5U, result_.directories_flushed);
}

TEST_F(BloomSaveFlushTest, MissingCoreTreesAreAllowed)
{
    std::filesystem::create_directories(root_ / "Saves/CurrentProfile/saves");
    std::filesystem::create_directories(root_ / "Saves/CurrentProfile/states");
    ASSERT_EQ(0, flush()) << error_;
    EXPECT_EQ(0U, result_.files_flushed);
    EXPECT_EQ(2U, result_.directories_flushed);
}

TEST_F(BloomSaveFlushTest, MissingInfoFailsClosed)
{
    std::filesystem::remove(root_ / "RetroArch/.retroarch/cores/gambatte_libretro.info");
    EXPECT_NE(0, flush());
}

TEST_F(BloomSaveFlushTest, SymlinkInCoreTreeIsRefused)
{
    std::filesystem::create_directories(root_ / "Saves/CurrentProfile/saves/Gambatte");
    std::filesystem::create_directories(root_ / "Saves/CurrentProfile/states");
    std::ofstream(root_ / "outside") << "outside";
    std::filesystem::create_symlink(root_ / "outside", root_ / "Saves/CurrentProfile/saves/Gambatte/link");
    EXPECT_NE(0, flush());
}

TEST_F(BloomSaveFlushTest, CoreTraversalIsRefused)
{
    EXPECT_NE(0, bloom_save_flush(root_.c_str(), "../gambatte_libretro.so", &result_, error_, sizeof(error_)));
}

} // namespace
