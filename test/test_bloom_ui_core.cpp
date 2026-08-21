#include <gtest/gtest.h>

extern "C" {
#include "../src/bloomUi/bloom_ui_core.h"
}

TEST(BloomUiLayout, FitsSupportedMiniAndFlipViewports)
{
    BloomUiLayout mini = {};
    ASSERT_EQ(0, bloom_ui_layout_init(640, 480, 0, &mini));
    EXPECT_EQ(24, mini.margin);
    EXPECT_EQ(56, mini.header.height);
    EXPECT_EQ(40, mini.footer.height);
    EXPECT_EQ(6UL, mini.visible_rows);
    EXPECT_EQ(mini.header.height, mini.content.y);
    EXPECT_EQ(mini.footer.y, mini.content.y + mini.content.height);

    BloomUiLayout flip = {};
    ASSERT_EQ(0, bloom_ui_layout_init(752, 560, 0, &flip));
    EXPECT_EQ(28, flip.margin);
    EXPECT_EQ(64, flip.header.height);
    EXPECT_EQ(48, flip.footer.height);
    EXPECT_EQ(7UL, flip.visible_rows);
    EXPECT_LE(flip.content.x + flip.content.width, flip.viewport_width);
    EXPECT_EQ(flip.footer.y, flip.content.y + flip.content.height);
}

TEST(BloomUiLayout, LargeTextReducesRowsWithoutClippingRegions)
{
    BloomUiLayout normal = {};
    BloomUiLayout large = {};
    ASSERT_EQ(0, bloom_ui_layout_init(640, 480, 0, &normal));
    ASSERT_EQ(0, bloom_ui_layout_init(640, 480, 1, &large));
    EXPECT_LT(large.visible_rows, normal.visible_rows);
    EXPECT_GT(large.row_height, normal.row_height);
    EXPECT_EQ(large.footer.y, large.content.y + large.content.height);
}

TEST(BloomUiLayout, RejectsUnsupportedOrMalformedGeometry)
{
    BloomUiLayout layout = {};
    EXPECT_NE(0, bloom_ui_layout_init(639, 480, 0, &layout));
    EXPECT_NE(0, bloom_ui_layout_init(640, 479, 0, &layout));
    EXPECT_NE(0, bloom_ui_layout_init(752, 560, 2, &layout));
    EXPECT_NE(0, bloom_ui_layout_init(752, 560, 0, nullptr));
}

TEST(BloomUiInput, NormalizesTheStableControlGrammar)
{
    EXPECT_EQ(BLOOM_UI_ACTION_FOCUS_UP, bloom_ui_normalize_input(BLOOM_UI_INPUT_UP));
    EXPECT_EQ(BLOOM_UI_ACTION_CONFIRM, bloom_ui_normalize_input(BLOOM_UI_INPUT_CONFIRM));
    EXPECT_EQ(BLOOM_UI_ACTION_BACK, bloom_ui_normalize_input(BLOOM_UI_INPUT_BACK));
    EXPECT_EQ(BLOOM_UI_ACTION_PREVIOUS_DESTINATION,
              bloom_ui_normalize_input(BLOOM_UI_INPUT_PREVIOUS_DESTINATION));
    EXPECT_EQ(BLOOM_UI_ACTION_NEXT_DESTINATION,
              bloom_ui_normalize_input(BLOOM_UI_INPUT_NEXT_DESTINATION));
    EXPECT_EQ(BLOOM_UI_ACTION_TOGGLE_FAVORITE, bloom_ui_normalize_input(BLOOM_UI_INPUT_FAVORITE));
    EXPECT_EQ(BLOOM_UI_ACTION_SEARCH, bloom_ui_normalize_input(BLOOM_UI_INPUT_SEARCH));
    EXPECT_EQ(BLOOM_UI_ACTION_QUICK_SETTINGS, bloom_ui_normalize_input(BLOOM_UI_INPUT_QUICK_SETTINGS));
    EXPECT_EQ(BLOOM_UI_ACTION_GAME_SWITCHER, bloom_ui_normalize_input(BLOOM_UI_INPUT_GAME_SWITCHER));
    EXPECT_EQ(BLOOM_UI_ACTION_NONE, bloom_ui_normalize_input((BloomUiInput)999));
}

TEST(BloomUiNavigation, TopLevelDestinationsWrapInBothDirections)
{
    EXPECT_EQ(BLOOM_UI_DESTINATION_LIBRARY, bloom_ui_destination_step(BLOOM_UI_DESTINATION_HOME, 1));
    EXPECT_EQ(BLOOM_UI_DESTINATION_HOME, bloom_ui_destination_step(BLOOM_UI_DESTINATION_SETTINGS, 1));
    EXPECT_EQ(BLOOM_UI_DESTINATION_SETTINGS, bloom_ui_destination_step(BLOOM_UI_DESTINATION_HOME, -1));
    EXPECT_EQ(BLOOM_UI_DESTINATION_HOME, bloom_ui_destination_step((BloomUiDestination)99, 1));
}

TEST(BloomUiFocus, ClampsAndScrollsAListWithoutWrapping)
{
    BloomUiFocus focus = {};
    bloom_ui_focus_init(&focus, 10);
    for (int index = 0; index < 7; index++) {
        EXPECT_EQ(1, bloom_ui_focus_step(&focus, 1, 4));
    }
    EXPECT_EQ(7UL, focus.selected);
    EXPECT_EQ(4UL, focus.window_start);

    for (int index = 0; index < 20; index++) {
        bloom_ui_focus_step(&focus, 1, 4);
    }
    EXPECT_EQ(9UL, focus.selected);
    EXPECT_EQ(6UL, focus.window_start);
    EXPECT_EQ(0, bloom_ui_focus_step(&focus, 1, 4));

    bloom_ui_focus_set_count(&focus, 3, 4);
    EXPECT_EQ(2UL, focus.selected);
    EXPECT_EQ(0UL, focus.window_start);
}

TEST(BloomUiFocus, EmptyListsAndInvalidStepsRemainStable)
{
    BloomUiFocus focus = {};
    bloom_ui_focus_init(&focus, 0);
    EXPECT_EQ(0, bloom_ui_focus_step(&focus, 1, 4));
    EXPECT_EQ(0, bloom_ui_focus_step(&focus, 0, 4));
    EXPECT_EQ(0UL, focus.selected);
    EXPECT_EQ(0UL, focus.window_start);
}

TEST(BloomUiDialog, UsesAnExplicitSafeDefaultAndClampedButtons)
{
    BloomUiDialogFocus dialog = {};
    ASSERT_EQ(0, bloom_ui_dialog_init(&dialog, 3, 0, 2));
    EXPECT_EQ(0UL, dialog.selected);
    EXPECT_EQ(2UL, dialog.destructive);
    EXPECT_EQ(0, bloom_ui_dialog_step(&dialog, -1));
    EXPECT_EQ(1, bloom_ui_dialog_step(&dialog, 1));
    EXPECT_EQ(1UL, dialog.selected);
    EXPECT_EQ(1, bloom_ui_dialog_step(&dialog, 1));
    EXPECT_EQ(0, bloom_ui_dialog_step(&dialog, 1));
    EXPECT_NE(0, bloom_ui_dialog_init(&dialog, 4, 0, SIZE_MAX));
    EXPECT_NE(0, bloom_ui_dialog_init(&dialog, 2, 2, SIZE_MAX));
}

TEST(BloomUiKeyboard, ProvidesLowerUpperAndPrintableAsciiSymbols)
{
    BloomUiKeyboardFocus keyboard = {};
    bloom_ui_keyboard_init(&keyboard);
    EXPECT_EQ('1', bloom_ui_keyboard_character(&keyboard));
    ASSERT_EQ(1, bloom_ui_keyboard_move(&keyboard, 0, 1));
    EXPECT_EQ('q', bloom_ui_keyboard_character(&keyboard));
    bloom_ui_keyboard_cycle_mode(&keyboard);
    EXPECT_EQ('Q', bloom_ui_keyboard_character(&keyboard));
    bloom_ui_keyboard_cycle_mode(&keyboard);
    EXPECT_EQ('-', bloom_ui_keyboard_character(&keyboard));
    bloom_ui_keyboard_cycle_mode(&keyboard);
    EXPECT_EQ('q', bloom_ui_keyboard_character(&keyboard));
}

TEST(BloomUiKeyboard, ClampsMovementToTheCurrentRow)
{
    BloomUiKeyboardFocus keyboard = {};
    bloom_ui_keyboard_init(&keyboard);
    ASSERT_EQ(1, bloom_ui_keyboard_move(&keyboard, 0, 1));
    for (int index = 0; index < 20; index++) {
        bloom_ui_keyboard_move(&keyboard, 1, 0);
    }
    EXPECT_EQ('p', bloom_ui_keyboard_character(&keyboard));
    ASSERT_EQ(1, bloom_ui_keyboard_move(&keyboard, 0, 1));
    EXPECT_EQ('l', bloom_ui_keyboard_character(&keyboard));
    EXPECT_EQ(0, bloom_ui_keyboard_move(&keyboard, 1, 1));
}

TEST(BloomUiKeyboard, TextEditingIsBoundedAndAsciiOnly)
{
    char text[5] = {};
    EXPECT_EQ(0, bloom_ui_text_append(text, sizeof(text), 'A'));
    EXPECT_EQ(0, bloom_ui_text_append(text, sizeof(text), '@'));
    EXPECT_EQ(0, bloom_ui_text_append(text, sizeof(text), '9'));
    EXPECT_EQ(0, bloom_ui_text_append(text, sizeof(text), '!'));
    EXPECT_STREQ("A@9!", text);
    EXPECT_NE(0, bloom_ui_text_append(text, sizeof(text), 'x'));
    EXPECT_NE(0, bloom_ui_text_append(text, sizeof(text), '\n'));
    EXPECT_EQ(0, bloom_ui_text_backspace(text, sizeof(text)));
    EXPECT_STREQ("A@9", text);
}
