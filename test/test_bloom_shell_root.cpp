#include <gtest/gtest.h>

extern "C" {
#include "../src/bloomShell/bloom_shell_root.h"
}

TEST(BloomShellRoot, ContinueOwnsColdBootFocusWhenAvailable)
{
    BloomShellRootState state{};
    bloom_shell_root_init(&state, 1);
    EXPECT_TRUE(state.continue_focused);
    EXPECT_EQ(BLOOM_SHELL_ROOT_GAMES, state.selected);
    EXPECT_EQ(1, bloom_shell_root_handle(&state, BLOOM_UI_ACTION_FOCUS_DOWN));
    EXPECT_FALSE(state.continue_focused);
    EXPECT_EQ(1, bloom_shell_root_handle(&state, BLOOM_UI_ACTION_FOCUS_UP));
    EXPECT_TRUE(state.continue_focused);
}

TEST(BloomShellRoot, RailStartsOnGamesAndReachesEveryDirectDestination)
{
    BloomShellRootState state{};
    bloom_shell_root_init(&state, 0);
    EXPECT_FALSE(state.continue_focused);
    for (int destination = BLOOM_SHELL_ROOT_GAMES; destination < BLOOM_SHELL_ROOT_COUNT;
         ++destination) {
        EXPECT_EQ(destination, state.selected);
        EXPECT_EQ(BLOOM_UI_DESTINATION_GAMES + destination, bloom_shell_root_open(&state));
        EXPECT_NE(nullptr, bloom_shell_root_label(state.selected));
        bloom_shell_root_handle(&state, BLOOM_UI_ACTION_FOCUS_RIGHT);
    }
    EXPECT_EQ(BLOOM_SHELL_ROOT_GAMES, state.selected);
    bloom_shell_root_handle(&state, BLOOM_UI_ACTION_FOCUS_LEFT);
    EXPECT_EQ(BLOOM_SHELL_ROOT_SETTINGS, state.selected);
}

TEST(BloomShellRoot, BackAndPageAcceleratorsCannotMutateRootState)
{
    BloomShellRootState state{};
    bloom_shell_root_init(&state, 0);
    for (BloomUiAction action : {BLOOM_UI_ACTION_BACK, BLOOM_UI_ACTION_NONE,
                                 BLOOM_UI_ACTION_GAME_SWITCHER,
                                 BLOOM_UI_ACTION_QUICK_SETTINGS, BLOOM_UI_ACTION_PAGE_UP,
                                 BLOOM_UI_ACTION_PAGE_DOWN}) {
        EXPECT_EQ(0, bloom_shell_root_handle(&state, action));
        EXPECT_EQ(BLOOM_SHELL_ROOT_GAMES, state.selected);
        EXPECT_FALSE(state.continue_focused);
    }
}
