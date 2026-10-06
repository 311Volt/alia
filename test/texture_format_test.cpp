#include "alia/gfx/texture.hpp"

#include <gtest/gtest.h>
#include <initializer_list>

namespace {
    using alia::pixel_format;
    using alia::texture_role;
}

TEST(TextureFormatTest, ExactFormatWinsWithoutProbingFallbacks) {
    int queries = 0;
    const auto result = alia::detail::closest_texture_format(
        pixel_format::rgb888, texture_role::color, [&](pixel_format fmt) {
            ++queries;
            EXPECT_EQ(fmt, pixel_format::rgb888);
            return true;
        });
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, pixel_format::rgb888);
    EXPECT_EQ(queries, 1);
}

TEST(TextureFormatTest, RGBFallsBackToBGRAFirst) {
    const auto result = alia::detail::closest_texture_format(
        pixel_format::rgb888, texture_role::color, [](pixel_format fmt) {
            return fmt == pixel_format::bgra8888 || fmt == pixel_format::rgba8888;
        });
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, pixel_format::bgra8888);
}

TEST(TextureFormatTest, RGBFallsBackToRGBAWhenBGRAIsUnsupported) {
    const auto result = alia::detail::closest_texture_format(
        pixel_format::rgb888, texture_role::color,
        [](pixel_format fmt) { return fmt == pixel_format::rgba8888; });
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, pixel_format::rgba8888);
}

TEST(TextureFormatTest, OtherIntegerColorFormatsUseTheSameFallbackOrder) {
    for (const auto source : {pixel_format::bgr888, pixel_format::rgb565,
                              pixel_format::xy_u8, pixel_format::gray_u8}) {
        const auto result = alia::detail::closest_texture_format(
            source, texture_role::color,
            [](pixel_format fmt) { return fmt == pixel_format::bgra8888; });
        ASSERT_TRUE(result);
        EXPECT_EQ(*result, pixel_format::bgra8888);
    }
}

TEST(TextureFormatTest, RGBAAndBGRACanFallBackToEachOther) {
    const auto bgra = alia::detail::closest_texture_format(
        pixel_format::rgba8888, texture_role::color,
        [](pixel_format fmt) { return fmt == pixel_format::bgra8888; });
    const auto rgba = alia::detail::closest_texture_format(
        pixel_format::bgra8888, texture_role::color,
        [](pixel_format fmt) { return fmt == pixel_format::rgba8888; });
    ASSERT_TRUE(bgra);
    ASSERT_TRUE(rgba);
    EXPECT_EQ(*bgra, pixel_format::bgra8888);
    EXPECT_EQ(*rgba, pixel_format::rgba8888);
}

TEST(TextureFormatTest, FloatFormatsFallBackToRGBAFloat) {
    for (const auto source : {pixel_format::rgb_f32, pixel_format::gray_f32, pixel_format::xy_f32}) {
        const auto result = alia::detail::closest_texture_format(
            source, texture_role::color,
            [](pixel_format fmt) { return fmt == pixel_format::rgba_f32; });
        ASSERT_TRUE(result);
        EXPECT_EQ(*result, pixel_format::rgba_f32);
    }
}

TEST(TextureFormatTest, AlphaMaskHasNoColorFallback) {
    EXPECT_FALSE(alia::detail::closest_texture_format(
        pixel_format::gray_u8, texture_role::alpha_mask,
        [](pixel_format fmt) { return fmt == pixel_format::bgra8888 || fmt == pixel_format::rgba8888; }));
}

TEST(TextureFormatTest, PaletteAndRGBAFloatHaveNoFallback) {
    for (const auto source : {pixel_format::palette_u8, pixel_format::rgba_f32}) {
        int queries = 0;
        EXPECT_FALSE(alia::detail::closest_texture_format(
            source, texture_role::color, [&](pixel_format fmt) {
                ++queries;
                return fmt == pixel_format::bgra8888;
            }));
        EXPECT_EQ(queries, 1);
    }
}

TEST(TextureFormatTest, ExhaustedFallbacksReturnNullopt) {
    int queries = 0;
    EXPECT_FALSE(alia::detail::closest_texture_format(
        pixel_format::rgb888, texture_role::color, [&](pixel_format) {
            ++queries;
            return false;
        }));
    EXPECT_EQ(queries, 3);
}
