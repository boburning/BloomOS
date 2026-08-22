#include <gtest/gtest.h>

#include <SDL/SDL.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include <unistd.h>

extern "C" {
#include "../src/bloomUi/bloom_ui_renderer.h"
}

namespace {

uint64_t surface_hash(SDL_Surface *surface)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    if (SDL_LockSurface(surface) != 0) {
        return 0;
    }
    for (int y = 0; y < surface->h; ++y) {
        const auto *row = reinterpret_cast<const Uint32 *>(
            static_cast<const Uint8 *>(surface->pixels) + y * surface->pitch);
        for (int x = 0; x < surface->w; ++x) {
            Uint8 red = 0;
            Uint8 green = 0;
            Uint8 blue = 0;
            SDL_GetRGB(row[x], surface->format, &red, &green, &blue);
            for (Uint8 value : {red, green, blue}) {
                hash ^= value;
                hash *= UINT64_C(1099511628211);
            }
        }
    }
    SDL_UnlockSurface(surface);
    return hash;
}

SDL_Surface *make_surface(int width, int height)
{
    return SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, 0x00FF0000,
                                0x0000FF00, 0x000000FF, 0xFF000000);
}

BloomUiScene example_scene()
{
    BloomUiScene scene{};
    scene.destination = BLOOM_UI_DESTINATION_GAMES;
    scene.item_count = 12;
    scene.selected = 3;
    scene.window_start = 1;
    scene.progress_value = 3;
    scene.progress_maximum = 8;
    scene.healthy = 1;
    return scene;
}

} // namespace

TEST(BloomUiRenderer, PaletteMatchesCanonicalDesignTokens)
{
    const std::array<uint32_t, BLOOM_UI_COLOR_COUNT> expected = {
        0x211711,
        0x352319,
        0x493025,
        0xF3E2BD,
        0xCDAF7B,
        0xD86A2C,
        0xE2A93B,
        0xA84832,
        0x708A4A,
    };
    for (size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(expected[index], bloom_ui_color_rgb(static_cast<BloomUiColor>(index)));
    }
    EXPECT_EQ(0U, bloom_ui_color_rgb(static_cast<BloomUiColor>(999)));
}

TEST(BloomUiRenderer, MiniAndFlipScenesHaveStableGoldenHashes)
{
    const struct {
        int width;
        int height;
        uint64_t expected_hash;
    } fixtures[] = {
        {640, 480, UINT64_C(18082805113807211106)},
        {752, 560, UINT64_C(6591053936195333355)},
    };
    for (const auto &fixture : fixtures) {
        BloomUiLayout layout{};
        ASSERT_EQ(0, bloom_ui_layout_init(fixture.width, fixture.height, 0, &layout));
        SDL_Surface *surface = make_surface(fixture.width, fixture.height);
        ASSERT_NE(nullptr, surface);
        BloomUiScene scene = example_scene();
        ASSERT_EQ(0, bloom_ui_render_shell(surface, &layout, &scene));
        EXPECT_EQ(fixture.expected_hash, surface_hash(surface));
        SDL_FreeSurface(surface);
    }
}

TEST(BloomUiRenderer, FocusUsesBothRaisedFillAndAccentStripe)
{
    BloomUiLayout layout{};
    ASSERT_EQ(0, bloom_ui_layout_init(640, 480, 0, &layout));
    SDL_Surface *surface = make_surface(640, 480);
    ASSERT_NE(nullptr, surface);
    BloomUiScene scene = example_scene();
    ASSERT_EQ(0, bloom_ui_render_shell(surface, &layout, &scene));

    const int selected_row = static_cast<int>(scene.selected - scene.window_start);
    const int y = layout.content.y + selected_row * layout.row_height + 8;
    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    auto *pixels = static_cast<Uint32 *>(surface->pixels);
    SDL_GetRGB(pixels[y * surface->pitch / 4 + layout.content.x + 2], surface->format,
               &red, &green, &blue);
    EXPECT_EQ(0xD8, red);
    EXPECT_EQ(0x6A, green);
    EXPECT_EQ(0x2C, blue);
    SDL_GetRGB(pixels[y * surface->pitch / 4 + layout.content.x + 8], surface->format,
               &red, &green, &blue);
    EXPECT_EQ(0x49, red);
    EXPECT_EQ(0x30, green);
    EXPECT_EQ(0x25, blue);
    SDL_FreeSurface(surface);
}

TEST(BloomUiRenderer, GameListWidthLeavesANativePreviewPane)
{
    BloomUiLayout layout{};
    ASSERT_EQ(0, bloom_ui_layout_init(752, 560, 0, &layout));
    SDL_Surface *surface = make_surface(752, 560);
    ASSERT_NE(nullptr, surface);
    BloomUiScene scene = example_scene();
    scene.row_width_percent = 58;
    ASSERT_EQ(0, bloom_ui_render_shell(surface, &layout, &scene));

    const int selected_row = static_cast<int>(scene.selected - scene.window_start);
    const int y = layout.content.y + selected_row * layout.row_height + 8;
    const int list_end = layout.content.x + layout.content.width * 58 / 100;
    auto *pixels = static_cast<Uint32 *>(surface->pixels);
    Uint8 red = 0, green = 0, blue = 0;
    SDL_GetRGB(pixels[y * surface->pitch / 4 + list_end - 2], surface->format, &red, &green,
               &blue);
    EXPECT_EQ(0x49, red);
    SDL_GetRGB(pixels[y * surface->pitch / 4 + list_end + 2], surface->format, &red, &green,
               &blue);
    EXPECT_EQ(0x21, red);
    SDL_FreeSurface(surface);
}

TEST(BloomUiRenderer, RejectsInvalidStateAndSurfaceShape)
{
    BloomUiLayout layout{};
    ASSERT_EQ(0, bloom_ui_layout_init(640, 480, 0, &layout));
    SDL_Surface *surface = make_surface(752, 560);
    ASSERT_NE(nullptr, surface);
    BloomUiScene scene = example_scene();
    EXPECT_NE(0, bloom_ui_render_shell(surface, &layout, &scene));
    SDL_FreeSurface(surface);

    surface = make_surface(640, 480);
    ASSERT_NE(nullptr, surface);
    scene.selected = scene.item_count;
    EXPECT_NE(0, bloom_ui_render_shell(surface, &layout, &scene));
    scene.selected = 0;
    scene.progress_maximum = static_cast<size_t>(UINT32_MAX) + 1;
    EXPECT_NE(0, bloom_ui_render_shell(surface, &layout, &scene));
    scene.progress_maximum = 0;
    scene.row_width_percent = 101;
    EXPECT_NE(0, bloom_ui_render_shell(surface, &layout, &scene));
    EXPECT_NE(0, bloom_ui_render_shell(nullptr, &layout, &scene));
    SDL_FreeSurface(surface);
}

TEST(BloomUiRenderer, RotatesPresentationPixelsByOneHundredEightyDegrees)
{
    SDL_Surface *surface = make_surface(3, 2);
    ASSERT_NE(nullptr, surface);
    auto *pixels = static_cast<Uint32 *>(surface->pixels);
    for (Uint32 index = 0; index < 6; ++index) {
        pixels[index] = index + 1;
    }

    ASSERT_EQ(0, bloom_ui_rotate_180(surface));
    for (Uint32 index = 0; index < 6; ++index) {
        EXPECT_EQ(6U - index, pixels[index]);
    }
    ASSERT_EQ(0, bloom_ui_rotate_180(surface));
    for (Uint32 index = 0; index < 6; ++index) {
        EXPECT_EQ(index + 1, pixels[index]);
    }
    EXPECT_NE(0, bloom_ui_rotate_180(nullptr));
    SDL_FreeSurface(surface);
}

TEST(BloomUiRenderer, DialogUsesSafeFocusAndDestructiveColorWithoutChangingSceneState)
{
    BloomUiLayout layout{};
    ASSERT_EQ(0, bloom_ui_layout_init(640, 480, 0, &layout));
    SDL_Surface *surface = make_surface(640, 480);
    ASSERT_NE(nullptr, surface);
    BloomUiScene scene = example_scene();
    ASSERT_EQ(0, bloom_ui_render_shell(surface, &layout, &scene));
    BloomUiDialogFocus dialog{};
    ASSERT_EQ(0, bloom_ui_dialog_init(&dialog, 2, 0, 1));
    ASSERT_EQ(0, bloom_ui_render_dialog(surface, &layout, &dialog));

    int width = layout.content.width * 4 / 5;
    int height = layout.viewport_height / 3;
    int x = (layout.viewport_width - width) / 2;
    int y = (layout.viewport_height - height) / 2;
    int button_width = (width - 48 - 12) / 2;
    int button_height = layout.row_height * 2 / 3;
    int button_y = y + height - button_height - 20;
    auto *pixels = static_cast<Uint32 *>(surface->pixels);
    Uint8 red = 0, green = 0, blue = 0;
    SDL_GetRGB(pixels[(button_y + 2) * surface->pitch / 4 + x + 26], surface->format,
               &red, &green, &blue);
    EXPECT_EQ(0xF3, red);
    EXPECT_EQ(0xE2, green);
    EXPECT_EQ(0xBD, blue);
    SDL_GetRGB(pixels[(button_y + 2) * surface->pitch / 4 + x + 24 + button_width + 14],
               surface->format, &red, &green, &blue);
    EXPECT_EQ(0xA8, red);
    EXPECT_EQ(0x48, green);
    EXPECT_EQ(0x32, blue);
    EXPECT_EQ(3U, scene.selected);
    SDL_FreeSurface(surface);
}

TEST(BloomUiRenderer, DialogRejectsMalformedStateAndSurfaceShape)
{
    BloomUiLayout layout{};
    ASSERT_EQ(0, bloom_ui_layout_init(640, 480, 0, &layout));
    SDL_Surface *surface = make_surface(640, 480);
    ASSERT_NE(nullptr, surface);
    BloomUiDialogFocus dialog{};
    EXPECT_NE(0, bloom_ui_render_dialog(surface, &layout, &dialog));
    dialog.button_count = 2;
    dialog.selected = 2;
    dialog.destructive = 1;
    EXPECT_NE(0, bloom_ui_render_dialog(surface, &layout, &dialog));
    dialog.selected = 0;
    dialog.destructive = 2;
    EXPECT_NE(0, bloom_ui_render_dialog(surface, &layout, &dialog));
    EXPECT_NE(0, bloom_ui_render_dialog(nullptr, &layout, &dialog));
    SDL_FreeSurface(surface);
}

TEST(BloomUiRenderer, KeyboardRendersEveryModeWithVisibleFocus)
{
    BloomUiLayout layout{};
    ASSERT_EQ(0, bloom_ui_layout_init(640, 480, 0, &layout));
    SDL_Surface *surface = make_surface(640, 480);
    ASSERT_NE(nullptr, surface);
    BloomUiScene scene = example_scene();
    BloomUiKeyboardFocus keyboard{};
    bloom_ui_keyboard_init(&keyboard);
    for (int mode = BLOOM_UI_KEYBOARD_LOWER; mode < BLOOM_UI_KEYBOARD_MODE_COUNT; ++mode) {
        ASSERT_EQ(0, bloom_ui_render_shell(surface, &layout, &scene));
        ASSERT_EQ(0, bloom_ui_render_keyboard(surface, &layout, &keyboard));
        EXPECT_NE(0U, surface_hash(surface));
        bloom_ui_keyboard_cycle_mode(&keyboard);
    }

    int height = layout.content.height * 3 / 4;
    int y = layout.content.y + layout.content.height - height;
    int gap = 4;
    int row_height = height / 4;
    int key_width = (layout.content.width - gap * 11) / 10;
    int row_width = key_width * 10 + gap * 9;
    int x = layout.content.x + (layout.content.width - row_width) / 2;
    auto *pixels = static_cast<Uint32 *>(surface->pixels);
    Uint8 red = 0, green = 0, blue = 0;
    SDL_GetRGB(pixels[(y + gap + row_height / 4) * surface->pitch / 4 + x + 2],
               surface->format, &red, &green, &blue);
    EXPECT_EQ(0xF3, red);
    EXPECT_EQ(0xE2, green);
    EXPECT_EQ(0xBD, blue);
    SDL_FreeSurface(surface);
}

TEST(BloomUiRenderer, KeyboardRejectsMalformedFocusAndSurfaceShape)
{
    BloomUiLayout layout{};
    ASSERT_EQ(0, bloom_ui_layout_init(640, 480, 0, &layout));
    SDL_Surface *surface = make_surface(640, 480);
    ASSERT_NE(nullptr, surface);
    BloomUiKeyboardFocus keyboard{};
    bloom_ui_keyboard_init(&keyboard);
    keyboard.row = 4;
    EXPECT_NE(0, bloom_ui_render_keyboard(surface, &layout, &keyboard));
    keyboard.row = 0;
    keyboard.column = 10;
    EXPECT_NE(0, bloom_ui_render_keyboard(surface, &layout, &keyboard));
    keyboard.column = 0;
    keyboard.mode = BLOOM_UI_KEYBOARD_MODE_COUNT;
    EXPECT_NE(0, bloom_ui_render_keyboard(surface, &layout, &keyboard));
    EXPECT_NE(0, bloom_ui_render_keyboard(nullptr, &layout, &keyboard));
    SDL_FreeSurface(surface);
}

TEST(BloomUiRenderer, PublishesEveryFramebufferPage)
{
    SDL_Surface *surface = make_surface(3, 2);
    ASSERT_NE(nullptr, surface);
    auto *pixels = static_cast<Uint8 *>(surface->pixels);
    for (int y = 0; y < surface->h; ++y)
        for (int x = 0; x < surface->w * 4; ++x)
            pixels[y * surface->pitch + x] = static_cast<Uint8>(y * 32 + x);

    const auto path = std::filesystem::temp_directory_path() /
                      ("bloom-framebuffer-pages-" + std::to_string(getpid()));
    {
        std::ofstream target(path, std::ios::binary);
        std::vector<char> empty(3 * 2 * 4 * 3, '\0');
        target.write(empty.data(), static_cast<std::streamsize>(empty.size()));
    }
    ASSERT_EQ(0, bloom_ui_publish_framebuffer_pages(surface, path.c_str(), 3));
    std::ifstream source(path, std::ios::binary);
    std::vector<Uint8> actual((std::istreambuf_iterator<char>(source)),
                              std::istreambuf_iterator<char>());
    std::vector<Uint8> expected;
    for (int page = 0; page < 3; ++page)
        for (int y = 0; y < surface->h; ++y)
            expected.insert(expected.end(), pixels + y * surface->pitch,
                            pixels + y * surface->pitch + surface->w * 4);
    EXPECT_EQ(expected, actual);
    EXPECT_NE(0, bloom_ui_publish_framebuffer_pages(surface, "", 3));
    EXPECT_NE(0, bloom_ui_publish_framebuffer_pages(surface, path.c_str(), 0));
    std::filesystem::remove(path);
    SDL_FreeSurface(surface);
}

TEST(BloomUiRenderer, FiveHundredOpenBackActionCyclesRemainDeterministic)
{
    BloomUiLayout layout{};
    ASSERT_EQ(0, bloom_ui_layout_init(640, 480, 0, &layout));
    SDL_Surface *surface = make_surface(640, 480);
    ASSERT_NE(nullptr, surface);

    BloomUiScene root{};
    root.destination = BLOOM_UI_DESTINATION_ROOT;
    root.item_count = 5;
    root.healthy = 1;
    BloomUiScene games = example_scene();
    BloomUiDialogFocus actions{};

    for (size_t cycle = 0; cycle < 500; ++cycle) {
        root.selected = cycle % root.item_count;
        ASSERT_EQ(0, bloom_ui_render_shell(surface, &layout, &root));

        games.selected = cycle % games.item_count;
        games.window_start = games.selected < layout.visible_rows
                                 ? 0
                                 : games.selected - layout.visible_rows + 1;
        ASSERT_EQ(0, bloom_ui_render_shell(surface, &layout, &games));
        ASSERT_EQ(0, bloom_ui_dialog_init(&actions, 2, 0, 1));
        if ((cycle & 1U) != 0) {
            ASSERT_EQ(1, bloom_ui_dialog_step(&actions, 1));
        }
        ASSERT_EQ(0, bloom_ui_render_dialog(surface, &layout, &actions));

        ASSERT_EQ(0, bloom_ui_render_shell(surface, &layout, &root));
    }

    const uint64_t final_hash = surface_hash(surface);
    ASSERT_NE(0U, final_hash);
    ASSERT_EQ(0, bloom_ui_render_shell(surface, &layout, &root));
    EXPECT_EQ(final_hash, surface_hash(surface));
    SDL_FreeSurface(surface);
}
