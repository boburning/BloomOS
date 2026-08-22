#include "gtest/gtest.h"

#include <cstdlib>

extern "C" {
#include "../src/common/utils/log.h"
#include "../src/common/theme/load.h"

TTF_Font *TTF_OpenFont(const char *, int)
{
    return nullptr;
}
}

class ThemeLoadTest : public testing::Test {
  protected:
    void SetUp() override { unsetenv("BLOOM_FIXED_THEME"); }
    void TearDown() override { unsetenv("BLOOM_FIXED_THEME"); }
};

TEST_F(ThemeLoadTest, FixedBloomModeRequiresTheExactRuntimeValue)
{
    EXPECT_FALSE(theme_usesFixedBloomResources());
    ASSERT_EQ(setenv("BLOOM_FIXED_THEME", "true", 1), 0);
    EXPECT_FALSE(theme_usesFixedBloomResources());
    ASSERT_EQ(setenv("BLOOM_FIXED_THEME", "1", 1), 0);
    EXPECT_TRUE(theme_usesFixedBloomResources());
}

TEST_F(ThemeLoadTest, FixedBloomModeBypassesThemePathsAndOverrides)
{
    ASSERT_EQ(setenv("BLOOM_FIXED_THEME", "1", 1), 0);
    char theme_path[STR_MAX] = "unsafe";
    char image_path[STR_MAX * 2] = "unsafe";

    EXPECT_STREQ(FALLBACK_THEME_PATH, theme_getPath(theme_path));
    EXPECT_EQ(0, theme_getImagePath("/untrusted/theme/", "extra/bootScreen", image_path));
    EXPECT_STREQ(SYSTEM_RESOURCES "bootScreen.png", image_path);
    EXPECT_EQ(0, theme_getImagePath("/untrusted/theme/", "power-full-icon", image_path));
    EXPECT_STREQ(FALLBACK_PATH "skin/power-full-icon.png", image_path);
}
