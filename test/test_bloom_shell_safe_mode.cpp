#include <gtest/gtest.h>

extern "C" {
#include "../src/bloomShell/bloom_shell_safe_mode.h"
}

TEST(BloomShellSafeMode, RequiresTheStrictRuntimeFlag)
{
    EXPECT_EQ(1, bloom_shell_safe_mode_enabled("true"));
    EXPECT_EQ(0, bloom_shell_safe_mode_enabled(nullptr));
    EXPECT_EQ(0, bloom_shell_safe_mode_enabled("false"));
    EXPECT_EQ(0, bloom_shell_safe_mode_enabled("TRUE"));
    EXPECT_EQ(0, bloom_shell_safe_mode_enabled("1"));
}

TEST(BloomShellSafeMode, ExposesOneFlatRecoveryList)
{
    EXPECT_STREQ("Browse Games", bloom_shell_safe_mode_label(BLOOM_SHELL_SAFE_MODE_GAMES));
    EXPECT_STREQ("System Health", bloom_shell_safe_mode_label(BLOOM_SHELL_SAFE_MODE_HEALTH));
    EXPECT_STREQ("Export Support File",
                 bloom_shell_safe_mode_label(BLOOM_SHELL_SAFE_MODE_EXPORT_SUPPORT));
    EXPECT_STREQ("Restore Previous Version",
                 bloom_shell_safe_mode_label(BLOOM_SHELL_SAFE_MODE_ROLLBACK));
    EXPECT_STREQ("Restart Normally",
                 bloom_shell_safe_mode_label(BLOOM_SHELL_SAFE_MODE_RESTART_NORMAL));
    EXPECT_EQ(nullptr,
              bloom_shell_safe_mode_label((BloomShellSafeModeRow)BLOOM_SHELL_SAFE_MODE_ROW_COUNT));
}
