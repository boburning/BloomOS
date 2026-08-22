#include <gtest/gtest.h>

extern "C" {
#include "../src/bloomUi/bloom_ui_input.h"
#include "../src/common/system/keymap_sw.h"
}

TEST(BloomUiInput, MapsMiyooKeysToSemanticInput)
{
    EXPECT_EQ(BLOOM_UI_INPUT_UP, bloom_ui_input_from_sdl_key(SW_BTN_UP));
    EXPECT_EQ(BLOOM_UI_INPUT_DOWN, bloom_ui_input_from_sdl_key(SW_BTN_DOWN));
    EXPECT_EQ(BLOOM_UI_INPUT_LEFT, bloom_ui_input_from_sdl_key(SW_BTN_LEFT));
    EXPECT_EQ(BLOOM_UI_INPUT_RIGHT, bloom_ui_input_from_sdl_key(SW_BTN_RIGHT));
    EXPECT_EQ(BLOOM_UI_INPUT_CONFIRM, bloom_ui_input_from_sdl_key(SW_BTN_A));
    EXPECT_EQ(BLOOM_UI_INPUT_BACK, bloom_ui_input_from_sdl_key(SW_BTN_B));
    EXPECT_EQ(BLOOM_UI_INPUT_CONTEXT, bloom_ui_input_from_sdl_key(SW_BTN_X));
    EXPECT_EQ(BLOOM_UI_INPUT_FAVORITE, bloom_ui_input_from_sdl_key(SW_BTN_Y));
    EXPECT_EQ(BLOOM_UI_INPUT_SEARCH, bloom_ui_input_from_sdl_key(SW_BTN_SELECT));
    EXPECT_EQ(BLOOM_UI_INPUT_QUICK_SETTINGS, bloom_ui_input_from_sdl_key(SW_BTN_START));
    EXPECT_EQ(BLOOM_UI_INPUT_GAME_SWITCHER, bloom_ui_input_from_sdl_key(SW_BTN_MENU));
    EXPECT_EQ(BLOOM_UI_INPUT_PAGE_UP, bloom_ui_input_from_sdl_key(SW_BTN_L1));
    EXPECT_EQ(BLOOM_UI_INPUT_PAGE_DOWN, bloom_ui_input_from_sdl_key(SW_BTN_R1));
}

TEST(BloomUiInput, LeavesUnassignedAndUnknownKeysUnbound)
{
    EXPECT_EQ(BLOOM_UI_INPUT_NONE, bloom_ui_input_from_sdl_key(SW_BTN_L2));
    EXPECT_EQ(BLOOM_UI_INPUT_NONE, bloom_ui_input_from_sdl_key(SW_BTN_R2));
    EXPECT_EQ(BLOOM_UI_INPUT_NONE, bloom_ui_input_from_sdl_key(SDLK_UNKNOWN));
    EXPECT_EQ(BLOOM_UI_INPUT_NONE, bloom_ui_input_from_sdl_key(SDLK_a));
}

TEST(BloomUiInput, RearShouldersRemainUnassigned)
{
    for (SDLKey key : {SW_BTN_L2, SW_BTN_R2}) {
        BloomUiInput input = bloom_ui_input_from_sdl_key(key);
        EXPECT_EQ(BLOOM_UI_INPUT_NONE, input);
        EXPECT_EQ(BLOOM_UI_ACTION_NONE, bloom_ui_normalize_input(input));
    }
}
