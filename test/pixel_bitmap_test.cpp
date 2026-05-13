#include "alia/gfx/bitmap/bitmap.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <span>
#include <stdexcept>

namespace {

    template <typename TPixel, std::size_t N>
    alia::bitmap_view<TPixel> make_view(std::array<TPixel, N> &pixels, int width, int height) {
        return alia::bitmap_view<TPixel>(
            width,
            height,
            width * static_cast<int>(sizeof(TPixel)),
            reinterpret_cast<std::byte *>(pixels.data())
        );
    }

} // namespace

static_assert(alia::pixel<alia::px_xy_u8>);
static_assert(alia::pixel<alia::px_xy_f32>);
static_assert(alia::pixel<alia::px_palette_u8>);
static_assert(alia::is_convert_pixel_allowed_v<alia::px_rgb888, alia::px_rgba8888>);
static_assert(alia::is_convert_pixel_allowed_v<alia::px_rgb565, alia::px_rgb888>);
static_assert(!alia::is_convert_pixel_allowed_v<alia::px_rgba8888, alia::px_rgb888>);
static_assert(!alia::is_convert_pixel_allowed_v<alia::px_rgb888, alia::px_rgb565>);
static_assert(!alia::is_convert_pixel_allowed_v<alia::px_rgb888, alia::px_gray_u8>);

TEST(PixelTest, NewPixelTagsAndFormatSizes) {
    EXPECT_EQ(alia::bytes_per_pixel_for_format(alia::pixel_format::xy_u8), 2);
    EXPECT_EQ(alia::bytes_per_pixel_for_format(alia::pixel_format::xy_f32), 8);
    EXPECT_EQ(alia::bytes_per_pixel_for_format(alia::pixel_format::palette_u8), 1);

    EXPECT_TRUE(alia::has_x_v<alia::px_xy_u8>);
    EXPECT_TRUE(alia::has_y_v<alia::px_xy_u8>);
    EXPECT_TRUE(alia::has_palette_index_v<alia::px_palette_u8>);
    EXPECT_EQ(alia::get_x(alia::px_xy_u8{3, 4}), 3);
    EXPECT_EQ(alia::get_y(alia::px_xy_u8{3, 4}), 4);
    EXPECT_EQ(alia::get_palette_index(alia::px_palette_u8{9}), 9);
}

TEST(PixelConversionTest, DefaultConversionRules) {
    EXPECT_TRUE(alia::can_convert_pixel(alia::pixel_format::rgb888, alia::pixel_format::rgba8888));
    EXPECT_TRUE(alia::can_convert_pixel(alia::pixel_format::rgb565, alia::pixel_format::rgb888));
    EXPECT_TRUE(alia::can_convert_pixel(alia::pixel_format::xy_u8, alia::pixel_format::xy_f32));
    EXPECT_FALSE(alia::can_convert_pixel(alia::pixel_format::rgba8888, alia::pixel_format::rgb888));
    EXPECT_FALSE(alia::can_convert_pixel(alia::pixel_format::rgb888, alia::pixel_format::gray_u8));
    EXPECT_FALSE(alia::can_convert_pixel(alia::pixel_format::xy_f32, alia::pixel_format::xy_u8));
}

TEST(PixelConversionTest, ConvertsColorGrayAndPackedPixels) {
    const auto rgba = alia::convert_pixel<alia::px_bgra8888>(alia::px_rgb888{10, 20, 30});
    EXPECT_EQ(rgba.r, 10);
    EXPECT_EQ(rgba.g, 20);
    EXPECT_EQ(rgba.b, 30);
    EXPECT_EQ(rgba.a, 255);

    const auto gray_rgba = alia::convert_pixel<alia::px_rgba8888>(alia::px_gray_u8{7});
    EXPECT_EQ(gray_rgba.r, 7);
    EXPECT_EQ(gray_rgba.g, 7);
    EXPECT_EQ(gray_rgba.b, 7);
    EXPECT_EQ(gray_rgba.a, 255);

    const auto f32 = alia::convert_pixel<alia::px_rgb_f32>(alia::px_rgb888{255, 128, 0});
    EXPECT_FLOAT_EQ(f32.r, 1.0f);
    EXPECT_NEAR(f32.g, 128.0f / 255.0f, 0.000001f);
    EXPECT_FLOAT_EQ(f32.b, 0.0f);

    const auto red565 = alia::convert_pixel<alia::px_rgb888>(alia::px_rgb565::of(0xf800));
    EXPECT_EQ(red565.r, 255);
    EXPECT_EQ(red565.g, 0);
    EXPECT_EQ(red565.b, 0);
}

TEST(PixelConversionTest, ExplicitAlphaDrop) {
    const auto rgb = alia::convert_pixel_no_preserve_alpha<alia::px_rgb888>(alia::px_rgba8888{1, 2, 3, 4});
    EXPECT_EQ(rgb.r, 1);
    EXPECT_EQ(rgb.g, 2);
    EXPECT_EQ(rgb.b, 3);
}

TEST(PixelConversionTest, GrayscaleConversions) {
    const auto weighted = alia::convert_to_grayscale<alia::px_gray_u8>(alia::px_rgb888{255, 0, 0});
    EXPECT_EQ(weighted.v, 76);

    const auto fast = alia::convert_to_grayscale_fast<alia::px_gray_u8>(alia::px_rgb888{10, 20, 30});
    EXPECT_EQ(fast.v, 20);

    const auto fast_f32 = alia::convert_to_grayscale_fast<alia::px_gray_f32>(alia::px_rgb_f32{1.0f, 0.5f, 0.0f});
    EXPECT_FLOAT_EQ(fast_f32.v, 0.5f);
}

TEST(PixelConversionTest, PaletteLookup) {
    std::array<alia::px_rgb888, 256> palette{};
    palette[3] = alia::px_rgb888{11, 22, 33};

    const auto px = alia::convert_from_palette<alia::px_rgb888>(
        alia::px_palette_u8{3},
        std::span<const alia::px_rgb888, 256>{palette}
    );

    EXPECT_EQ(px.r, 11);
    EXPECT_EQ(px.g, 22);
    EXPECT_EQ(px.b, 33);
}

TEST(BitmapBlitTest, FullBlitCopiesMatchingPixels) {
    std::array<alia::px_rgb888, 4> src{{
        {1, 2, 3}, {4, 5, 6},
        {7, 8, 9}, {10, 11, 12},
    }};
    std::array<alia::px_rgb888, 4> dst{};

    alia::blit(make_view(dst, 2, 2), make_view(src, 2, 2));

    EXPECT_EQ(dst[0].r, 1);
    EXPECT_EQ(dst[1].g, 5);
    EXPECT_EQ(dst[2].b, 9);
    EXPECT_EQ(dst[3].r, 10);
}

TEST(BitmapBlitTest, RectBlitClipsDestinationAndShiftsSource) {
    std::array<alia::px_rgb888, 6> src{{
        {1, 0, 0}, {2, 0, 0}, {3, 0, 0},
        {4, 0, 0}, {5, 0, 0}, {6, 0, 0},
    }};
    std::array<alia::px_rgb888, 6> dst{};

    alia::blit(
        make_view(dst, 3, 2),
        make_view(src, 3, 2),
        alia::rect_i::pos_size({0, 0}, {3, 2}),
        {-1, 0}
    );

    EXPECT_EQ(dst[0].r, 2);
    EXPECT_EQ(dst[1].r, 3);
    EXPECT_EQ(dst[2].r, 0);
    EXPECT_EQ(dst[3].r, 5);
    EXPECT_EQ(dst[4].r, 6);
    EXPECT_EQ(dst[5].r, 0);
}

TEST(BitmapBlitTest, RectBlitThrowsWhenResolvedSourceIsOutsideSourceBounds) {
    std::array<alia::px_rgb888, 6> src{};
    std::array<alia::px_rgb888, 6> dst{};

    EXPECT_THROW(
        alia::blit(
            make_view(dst, 3, 2),
            make_view(src, 3, 2),
            alia::rect_i::pos_size({2, 0}, {2, 1}),
            {0, 0}
        ),
        std::invalid_argument
    );
}

TEST(BitmapBlitTest, ConvertingBlitUsesDefaultOrCustomConverter) {
    std::array<alia::px_rgb888, 2> src{{{10, 20, 30}, {40, 50, 60}}};
    std::array<alia::px_rgba8888, 2> rgba_dst{};
    std::array<alia::px_gray_u8, 2> gray_dst{};

    alia::converting_blit(make_view(rgba_dst, 2, 1), make_view(src, 2, 1));
    EXPECT_EQ(rgba_dst[0].r, 10);
    EXPECT_EQ(rgba_dst[0].a, 255);
    EXPECT_EQ(rgba_dst[1].b, 60);

    alia::converting_blit(
        make_view(gray_dst, 2, 1),
        make_view(src, 2, 1),
        [](alia::px_rgb888 px) {
            return alia::convert_to_grayscale<alia::px_gray_u8>(px);
        }
    );
    EXPECT_EQ(gray_dst[0].v, 18);
    EXPECT_EQ(gray_dst[1].v, 48);
}

TEST(BitmapBlitTest, TypeErasedConvertingBlitDispatchesAllowedConversions) {
    std::array<alia::px_rgb888, 1> src{{{1, 2, 3}}};
    std::array<alia::px_rgba8888, 1> dst{};

    alia::any_bitmap_view src_any(
        alia::pixel_format::rgb888,
        1,
        1,
        static_cast<int>(sizeof(alia::px_rgb888)),
        reinterpret_cast<std::byte *>(src.data())
    );
    alia::any_bitmap_view dst_any(
        alia::pixel_format::rgba8888,
        1,
        1,
        static_cast<int>(sizeof(alia::px_rgba8888)),
        reinterpret_cast<std::byte *>(dst.data())
    );

    alia::converting_blit(dst_any, src_any);

    EXPECT_EQ(dst[0].r, 1);
    EXPECT_EQ(dst[0].g, 2);
    EXPECT_EQ(dst[0].b, 3);
    EXPECT_EQ(dst[0].a, 255);
}

TEST(BitmapBlitTest, TypeErasedConvertingBlitSupportsRectCopies) {
    std::array<alia::px_rgb888, 3> src{{{1, 0, 0}, {2, 0, 0}, {3, 0, 0}}};
    std::array<alia::px_rgba8888, 3> dst{};

    alia::any_bitmap_view src_any(
        alia::pixel_format::rgb888,
        3,
        1,
        3 * static_cast<int>(sizeof(alia::px_rgb888)),
        reinterpret_cast<std::byte *>(src.data())
    );
    alia::any_bitmap_view dst_any(
        alia::pixel_format::rgba8888,
        3,
        1,
        3 * static_cast<int>(sizeof(alia::px_rgba8888)),
        reinterpret_cast<std::byte *>(dst.data())
    );

    alia::converting_blit(dst_any, src_any, alia::rect_i::pos_size({0, 0}, {3, 1}), {-1, 0});

    EXPECT_EQ(dst[0].r, 2);
    EXPECT_EQ(dst[0].a, 255);
    EXPECT_EQ(dst[1].r, 3);
    EXPECT_EQ(dst[1].a, 255);
    EXPECT_EQ(dst[2].r, 0);
    EXPECT_EQ(dst[2].a, 0);
}

TEST(BitmapBlitTest, TypeErasedConvertingBlitThrowsForUnsupportedConversions) {
    std::array<alia::px_rgba8888, 1> src{{{1, 2, 3, 4}}};
    std::array<alia::px_rgb888, 1> dst{};

    alia::any_bitmap_view src_any(
        alia::pixel_format::rgba8888,
        1,
        1,
        static_cast<int>(sizeof(alia::px_rgba8888)),
        reinterpret_cast<std::byte *>(src.data())
    );
    alia::any_bitmap_view dst_any(
        alia::pixel_format::rgb888,
        1,
        1,
        static_cast<int>(sizeof(alia::px_rgb888)),
        reinterpret_cast<std::byte *>(dst.data())
    );

    EXPECT_THROW(alia::converting_blit(dst_any, src_any), std::invalid_argument);
}
