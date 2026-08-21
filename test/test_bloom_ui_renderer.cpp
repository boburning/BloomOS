#include <gtest/gtest.h>

#include <SDL/SDL.h>

#include <array>
#include <cstdint>

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
    scene.destination = BLOOM_UI_DESTINATION_LIBRARY;
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
        {640, 480, UINT64_C(2753868655391038583)},
        {752, 560, UINT64_C(7668221142765054867)},
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
    EXPECT_NE(0, bloom_ui_render_shell(nullptr, &layout, &scene));
    SDL_FreeSurface(surface);
}
