#include "alia/gfx/bitmap/bitmap.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>

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

    template <alia::pixel TPixel, std::size_t N>
    alia::any_bitmap_view make_any_view(std::array<TPixel, N> &pixels, int width, int height) {
        return alia::any_bitmap_view(
            TPixel::format_id, width, height, width * static_cast<int>(sizeof(TPixel)),
            reinterpret_cast<std::byte *>(pixels.data())
        );
    }

    template <alia::pixel TPixel>
    void expect_pixels_equal(TPixel actual, TPixel expected) {
        if constexpr (alia::has_color_v<TPixel>) {
            EXPECT_EQ(alia::get_red(actual), alia::get_red(expected));
            EXPECT_EQ(alia::get_green(actual), alia::get_green(expected));
            EXPECT_EQ(alia::get_blue(actual), alia::get_blue(expected));
        } else if constexpr (alia::has_gray_v<TPixel>) {
            EXPECT_EQ(alia::get_gray(actual), alia::get_gray(expected));
        } else if constexpr (alia::has_xy_v<TPixel>) {
            EXPECT_EQ(alia::get_x(actual), alia::get_x(expected));
            EXPECT_EQ(alia::get_y(actual), alia::get_y(expected));
        }
        if constexpr (alia::has_alpha_v<TPixel>)
            EXPECT_EQ(alia::get_alpha(actual), alia::get_alpha(expected));
    }

    constexpr std::array pixel_formats{
        alia::pixel_format::rgb888, alia::pixel_format::rgba8888, alia::pixel_format::rgb565,
        alia::pixel_format::bgr888, alia::pixel_format::bgra8888, alia::pixel_format::gray_u8,
        alia::pixel_format::gray_f32, alia::pixel_format::rgba_f32, alia::pixel_format::rgb_f32,
        alia::pixel_format::xy_u8, alia::pixel_format::xy_f32, alia::pixel_format::palette_u8,
    };

} // namespace

static_assert(alia::pixel<alia::px_xy_u8>);
static_assert(alia::pixel<alia::px_xy_f32>);
static_assert(alia::pixel<alia::px_palette_u8>);
static_assert(alia::is_convert_pixel_allowed_v<alia::px_rgb888, alia::px_rgba8888>);
static_assert(alia::is_convert_pixel_allowed_v<alia::px_rgb565, alia::px_rgb888>);
static_assert(!alia::is_convert_pixel_allowed_v<alia::px_rgba8888, alia::px_rgb888>);
static_assert(!alia::is_convert_pixel_allowed_v<alia::px_rgb888, alia::px_rgb565>);
static_assert(!alia::is_convert_pixel_allowed_v<alia::px_rgb888, alia::px_gray_u8>);
static_assert(!alia::is_convert_pixel_allowed_v<alia::px_rgba_f32, alia::px_rgb888>);
static_assert(alia::hub_convertible_pixel<alia::px_rgb888>);
static_assert(alia::hub_convertible_pixel<alia::px_rgba8888>);
static_assert(alia::hub_convertible_pixel<alia::px_rgb565>);
static_assert(alia::hub_convertible_pixel<alia::px_bgr888>);
static_assert(alia::hub_convertible_pixel<alia::px_bgra8888>);
static_assert(alia::hub_convertible_pixel<alia::px_gray_u8>);
static_assert(alia::hub_convertible_pixel<alia::px_gray_f32>);
static_assert(alia::hub_convertible_pixel<alia::px_rgba_f32>);
static_assert(alia::hub_convertible_pixel<alia::px_rgb_f32>);
static_assert(alia::hub_convertible_pixel<alia::px_xy_u8>);
static_assert(alia::hub_convertible_pixel<alia::px_xy_f32>);
static_assert(!alia::hub_convertible_pixel<alia::px_palette_u8>);
static_assert(!alia::hub_convertible_pixel<int>);
static_assert(alia::to_rgba8888(alia::px_gray_u8{42}).r == 42);
static_assert(alia::to_rgba_f32(alia::px_xy_f32{2.0f, -0.5f}).r == 2.0f);
static_assert(alia::from_rgba8888<alia::px_gray_u8>({42, 42, 42, 0}).v == 42);
static_assert(alia::from_rgba_f32<alia::px_rgb565>({0.5f, 0.5f, 0.5f, 1.0f}).r == 16);
static_assert(alia::convert_pixel_lossy<alia::px_rgb888>(alia::px_rgba_f32{0.5f, -0.5f, 1.5f, 0.0f}).r == 128);
static_assert(alia::convert_pixel_lossy<alia::px_gray_f32>(alia::px_gray_f32{2.0f}).v == 2.0f);

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

TEST(PixelLossyConversionTest, AcceptsEveryKnownNonPaletteFormatPair) {
    for (const auto src : pixel_formats) {
        for (const auto dst : pixel_formats) {
            EXPECT_EQ(
                alia::can_convert_pixel_lossy(src, dst),
                src != alia::pixel_format::palette_u8 && dst != alia::pixel_format::palette_u8
            );
        }
    }
    const auto unknown = static_cast<alia::pixel_format>(-1);
    EXPECT_FALSE(alia::can_convert_pixel_lossy(unknown, alia::pixel_format::rgba8888));
    EXPECT_FALSE(alia::can_convert_pixel_lossy(alia::pixel_format::rgba8888, unknown));
}

TEST(PixelLossyConversionTest, FloatToIntegerClampsRoundsAndHandlesNaN) {
    const std::array values{
        -0.5f, 1.5f, std::numeric_limits<float>::quiet_NaN(), 0.5f,
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
    };
    const std::array expected_u8{0, 255, 0, 128, 255, 0};
    const std::array expected_5{0, 31, 0, 16, 31, 0};
    const std::array expected_6{0, 63, 0, 32, 63, 0};

    for (std::size_t i = 0; i < values.size(); ++i) {
        const alia::px_rgba_f32 src{values[i], values[i], values[i], values[i]};
        const auto rgb = alia::convert_pixel_lossy<alia::px_rgb888>(src);
        EXPECT_EQ(rgb.r, expected_u8[i]);
        EXPECT_EQ(rgb.g, expected_u8[i]);
        EXPECT_EQ(rgb.b, expected_u8[i]);
        EXPECT_EQ(alia::to_rgba8888(src).a, expected_u8[i]);
        const auto packed = alia::convert_pixel_lossy<alia::px_rgb565>(src);
        EXPECT_EQ(alia::get_red(packed), expected_5[i]);
        EXPECT_EQ(alia::get_green(packed), expected_6[i]);
        EXPECT_EQ(alia::get_blue(packed), expected_5[i]);
    }

    const auto color_pixel = alia::color_to_pixel<alia::px_rgba8888>({
        -0.5f, 1.5f, std::numeric_limits<float>::quiet_NaN(), 0.5f,
    });
    expect_pixels_equal(color_pixel, alia::px_rgba8888{0, 255, 0, 128});
}

TEST(PixelLossyConversionTest, DropsAlphaAndSuppliesOpaqueAlpha) {
    const auto rgb = alia::convert_pixel_lossy<alia::px_rgb888>(alia::px_rgba8888{10, 20, 30, 0});
    expect_pixels_equal(rgb, alia::px_rgb888{10, 20, 30});
    const auto rgba_f = alia::convert_pixel_lossy<alia::px_rgba_f32>(alia::px_rgb888{255, 128, 0});
    EXPECT_EQ(rgba_f.a, 1.0f);
    EXPECT_EQ(rgba_f.g, 128.0f / 255.0f);
    const auto gray_rgba = alia::to_rgba8888(alia::px_gray_u8{42});
    expect_pixels_equal(gray_rgba, alia::px_rgba8888{42, 42, 42, 255});
}

TEST(PixelLossyConversionTest, GrayRoundTripsExactlyThroughBothHubs) {
    for (int value = 0; value < 256; ++value) {
        const alia::px_gray_u8 gray{static_cast<std::uint8_t>(value)};
        EXPECT_EQ(alia::from_rgba8888<alia::px_gray_u8>(alia::to_rgba8888(gray)).v, value);
        EXPECT_EQ(alia::from_rgba_f32<alia::px_gray_u8>(alia::to_rgba_f32(gray)).v, value);
    }
    for (const float value : {-0.5f, 0.0f, 0.1f, 0.5f, 1.0f, 2.25f}) {
        const alia::px_gray_f32 gray{value};
        EXPECT_EQ(alia::from_rgba_f32<alia::px_gray_f32>(alia::to_rgba_f32(gray)).v, value);
        EXPECT_EQ(alia::convert_to_grayscale<alia::px_gray_f32>(alia::px_rgb_f32{value, value, value}).v, value);
    }
    EXPECT_EQ(alia::convert_pixel_lossy<alia::px_gray_f32>(alia::px_gray_u8{42}).v, 42.0f / 255.0f);
}

TEST(PixelLossyConversionTest, ColorToGrayMatchesExplicitGrayscaleConversion) {
    for (const auto src : std::array<alia::px_rgb888, 4>{{
             {255, 0, 0}, {0, 255, 0}, {10, 20, 30}, {255, 255, 255},
         }}) {
        EXPECT_EQ(
            alia::convert_pixel_lossy<alia::px_gray_u8>(src).v,
            alia::convert_to_grayscale<alia::px_gray_u8>(src).v
        );
        EXPECT_EQ(
            alia::convert_pixel_lossy<alia::px_gray_f32>(src).v,
            alia::convert_to_grayscale<alia::px_gray_f32>(src).v
        );
    }
    const alia::px_rgba_f32 src{1.5f, 0.25f, -0.5f, 0.0f};
    EXPECT_EQ(
        alia::convert_pixel_lossy<alia::px_gray_f32>(src).v,
        alia::convert_to_grayscale<alia::px_gray_f32>(src).v
    );
}

TEST(PixelLossyConversionTest, XYMapsToRedGreenAndBack) {
    const alia::px_xy_u8 xy{17, 203};
    const auto rgba = alia::to_rgba8888(xy);
    expect_pixels_equal(rgba, alia::px_rgba8888{17, 203, 0, 255});
    expect_pixels_equal(alia::from_rgba8888<alia::px_xy_u8>(rgba), xy);
    const auto rgba_f = alia::to_rgba_f32(xy);
    EXPECT_EQ(rgba_f.r, 17.0f / 255.0f);
    EXPECT_EQ(rgba_f.g, 203.0f / 255.0f);
    EXPECT_EQ(rgba_f.b, 0.0f);
    EXPECT_EQ(rgba_f.a, 1.0f);
    expect_pixels_equal(alia::from_rgba_f32<alia::px_xy_u8>(rgba_f), xy);

    const alia::px_xy_f32 hdr{2.25f, -0.5f};
    expect_pixels_equal(alia::from_rgba_f32<alia::px_xy_f32>(alia::to_rgba_f32(hdr)), hdr);
    expect_pixels_equal(alia::to_rgba8888(hdr), alia::px_rgba8888{255, 0, 0, 255});
    expect_pixels_equal(
        alia::convert_pixel_lossy<alia::px_xy_u8>(alia::px_rgb888{3, 4, 255}), alia::px_xy_u8{3, 4}
    );
}

TEST(PixelLossyConversionTest, FloatHubPreservesHDRAndAvoidsQuantizedLuminance) {
    const alia::px_rgba_f32 hdr{2.25f, -0.5f, 1.5f, 0.25f};
    expect_pixels_equal(alia::to_rgba_f32(hdr), hdr);
    expect_pixels_equal(alia::from_rgba_f32<alia::px_rgba_f32>(hdr), hdr);
    expect_pixels_equal(alia::convert_pixel_lossy<alia::px_rgb_f32>(hdr), alia::px_rgb_f32{2.25f, -0.5f, 1.5f});

    const alia::px_rgba8888 red{255, 0, 0, 0};
    const auto gray = alia::convert_pixel_lossy<alia::px_gray_f32>(red);
    EXPECT_EQ(gray.v, 0.299f);
    EXPECT_NE(gray.v, 76.0f / 255.0f);
}

TEST(PixelLossyConversionTest, AllowedPairsMatchStrictAndTypeErasedMatchesTyped) {
    for (const auto src_fmt : pixel_formats) {
        for (const auto dst_fmt : pixel_formats) {
            SCOPED_TRACE(static_cast<int>(src_fmt));
            SCOPED_TRACE(static_cast<int>(dst_fmt));
            ASSERT_TRUE(alia::detail::visit_pixel_format(src_fmt, [&]<alia::pixel TSrc>() {
                return alia::detail::visit_pixel_format(dst_fmt, [&]<alia::pixel TDst>() {
                    if constexpr (alia::hub_convertible_pixel<TSrc> && alia::hub_convertible_pixel<TDst>) {
                        for (const auto sample : std::array<alia::px_rgba_f32, 2>{{
                                 {0.23f, 0.51f, 0.87f, 0.37f}, {-0.5f, 1.5f, 2.25f, 0.75f},
                             }}) {
                            std::array<TSrc, 1> src{alia::from_rgba_f32<TSrc>(sample)};
                            const auto expected = alia::convert_pixel_lossy<TDst>(src[0]);
                            if constexpr (alia::is_convert_pixel_allowed_v<TSrc, TDst>)
                                expect_pixels_equal(expected, alia::convert_pixel<TDst>(src[0]));
                            std::array<TDst, 1> dst{};
                            alia::converting_blit_lossy(make_any_view(dst, 1, 1), make_any_view(src, 1, 1));
                            expect_pixels_equal(dst[0], expected);
                        }
                    }
                    return true;
                });
            }));
        }
    }
}

TEST(BitmapLossyBlitTest, TypedFullAndRectBlitsDropAlpha) {
    std::array<alia::px_rgba8888, 6> src{{
        {1, 10, 20, 0}, {2, 30, 40, 64}, {3, 50, 60, 128},
        {4, 70, 80, 255}, {5, 90, 100, 0}, {6, 110, 120, 255},
    }};
    std::array<alia::px_rgb888, 6> dst{};
    alia::converting_blit_lossy(make_view(dst, 3, 2), make_view(src, 3, 2));
    expect_pixels_equal(dst[0], alia::px_rgb888{1, 10, 20});
    expect_pixels_equal(dst[5], alia::px_rgb888{6, 110, 120});

    dst.fill({});
    alia::converting_blit_lossy(
        make_view(dst, 3, 2), make_view(src, 3, 2), alia::rect_i::pos_size({0, 0}, {3, 2}), {-1, 0}
    );
    expect_pixels_equal(dst[0], alia::px_rgb888{2, 30, 40});
    expect_pixels_equal(dst[1], alia::px_rgb888{3, 50, 60});
    expect_pixels_equal(dst[2], alia::px_rgb888{});
    expect_pixels_equal(dst[3], alia::px_rgb888{5, 90, 100});
    expect_pixels_equal(dst[4], alia::px_rgb888{6, 110, 120});
    expect_pixels_equal(dst[5], alia::px_rgb888{});
}

TEST(BitmapLossyBlitTest, TypeErasedFullAndRectBlitsDropAlpha) {
    std::array<alia::px_rgba8888, 6> src{{
        {1, 10, 20, 0}, {2, 30, 40, 64}, {3, 50, 60, 128},
        {4, 70, 80, 255}, {5, 90, 100, 0}, {6, 110, 120, 255},
    }};
    std::array<alia::px_rgb888, 6> dst{};
    const auto src_any = make_any_view(src, 3, 2);
    const auto dst_any = make_any_view(dst, 3, 2);
    EXPECT_THROW(alia::converting_blit(dst_any, src_any), std::invalid_argument);
    alia::converting_blit_lossy(dst_any, src_any);
    expect_pixels_equal(dst[0], alia::px_rgb888{1, 10, 20});
    expect_pixels_equal(dst[5], alia::px_rgb888{6, 110, 120});

    dst.fill({});
    alia::converting_blit_lossy(dst_any, src_any, alia::rect_i::pos_size({0, 0}, {3, 2}), {-1, 0});
    expect_pixels_equal(dst[0], alia::px_rgb888{2, 30, 40});
    expect_pixels_equal(dst[1], alia::px_rgb888{3, 50, 60});
    expect_pixels_equal(dst[2], alia::px_rgb888{});
    expect_pixels_equal(dst[3], alia::px_rgb888{5, 90, 100});
    expect_pixels_equal(dst[4], alia::px_rgb888{6, 110, 120});
    expect_pixels_equal(dst[5], alia::px_rgb888{});
}

TEST(BitmapLossyBlitTest, RejectsPaletteUnknownSizeAndBoundsBeforeWriting) {
    std::array<alia::px_palette_u8, 1> palette{{{7}}};
    std::array<alia::px_rgba8888, 1> color{{{10, 20, 30, 40}}};
    const auto palette_any = make_any_view(palette, 1, 1);
    const auto color_any = make_any_view(color, 1, 1);
    const auto rect = alia::rect_i::pos_size({0, 0}, {1, 1});
    EXPECT_THROW(alia::converting_blit_lossy(color_any, palette_any), std::invalid_argument);
    EXPECT_THROW(alia::converting_blit_lossy(palette_any, color_any), std::invalid_argument);
    EXPECT_THROW(alia::converting_blit_lossy(palette_any, palette_any), std::invalid_argument);
    EXPECT_THROW(alia::converting_blit_lossy(color_any, palette_any, rect, {}), std::invalid_argument);
    EXPECT_THROW(alia::converting_blit_lossy(palette_any, color_any, rect, {}), std::invalid_argument);

    alia::any_bitmap_view unknown(static_cast<alia::pixel_format>(-1), 1, 1, 1, nullptr);
    EXPECT_THROW(alia::converting_blit_lossy(color_any, unknown), std::invalid_argument);
    EXPECT_THROW(alia::converting_blit_lossy(unknown, color_any), std::invalid_argument);
    EXPECT_THROW(alia::converting_blit_lossy(color_any, unknown, rect, {}), std::invalid_argument);
    EXPECT_THROW(alia::converting_blit_lossy(unknown, color_any, rect, {}), std::invalid_argument);

    alia::any_bitmap_view different_size(alia::pixel_format::rgb888, 2, 1, 6, nullptr);
    EXPECT_THROW(alia::converting_blit_lossy(color_any, different_size), std::invalid_argument);
    EXPECT_THROW(
        alia::converting_blit_lossy(color_any, color_any, alia::rect_i::pos_size({1, 0}, {1, 1}), {}),
        std::invalid_argument
    );
    expect_pixels_equal(color[0], alia::px_rgba8888{10, 20, 30, 40});
    EXPECT_EQ(palette[0].palette_index, 7);
}

TEST(BitmapLossyBlitTest, EmptyViewsAndClippedRectsDoNotAccessPixels) {
    alia::any_bitmap_view src(alia::pixel_format::rgba_f32, 0, 1, 0, nullptr);
    alia::any_bitmap_view dst(alia::pixel_format::rgb888, 0, 1, 0, nullptr);
    EXPECT_NO_THROW(alia::converting_blit_lossy(dst, src));
    EXPECT_NO_THROW(alia::converting_blit_lossy(dst, src, {}, {}));
    alia::any_bitmap_view palette(alia::pixel_format::palette_u8, 0, 1, 0, nullptr);
    EXPECT_THROW(alia::converting_blit_lossy(dst, palette), std::invalid_argument);
    EXPECT_THROW(alia::converting_blit_lossy(dst, palette, {}, {}), std::invalid_argument);

    alia::any_bitmap_view full_src(alia::pixel_format::rgba_f32, 1, 1, 16, nullptr);
    alia::any_bitmap_view full_dst(alia::pixel_format::rgb888, 1, 1, 3, nullptr);
    EXPECT_NO_THROW(alia::converting_blit_lossy(full_dst, full_src, alia::rect_i::pos_size({0, 0}, {1, 1}), {2, 0}));
}

TEST(BitmapLossyBlitTest, FloatToBGRAMatchesCursorChannelMath) {
    std::array<alia::px_rgba_f32, 3> src{{
        {-0.5f, 0.5f, 1.5f, 0.25f}, {0.1f, 0.3f, 0.7f, 1.0f}, {1.0f, 0.0f, 0.125f, -0.5f},
    }};
    std::array<alia::px_bgra8888, 3> dst{};
    alia::converting_blit_lossy(make_any_view(dst, 3, 1), make_any_view(src, 3, 1));
    const auto old_channel = [](float value) {
        return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    for (std::size_t i = 0; i < src.size(); ++i) {
        expect_pixels_equal(dst[i], alia::px_bgra8888{
            old_channel(src[i].b), old_channel(src[i].g), old_channel(src[i].r), old_channel(src[i].a),
        });
    }
}

TEST(BitmapLossyBlitTest, ConvertsPastChunkBoundaryAndKeepsRowPadding) {
    constexpr int width = 513;
    constexpr int height = 2;
    constexpr int src_stride = width * sizeof(alia::px_rgba8888) + 3;
    constexpr int dst_stride = width * sizeof(alia::px_rgb565) + 3;
    std::array<std::byte, src_stride * height + 1> src{};
    std::array<std::byte, dst_stride * height + 1> dst;
    dst.fill(std::byte{0x5a});
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const alia::px_rgba8888 px{
                static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(x / 2),
                static_cast<std::uint8_t>(y * 127), 0,
            };
            std::memcpy(src.data() + 1 + y * src_stride + x * sizeof(px), &px, sizeof(px));
        }
    }
    alia::converting_blit_lossy(
        alia::any_bitmap_view(alia::pixel_format::rgb565, width, height, dst_stride, dst.data() + 1),
        alia::any_bitmap_view(alia::pixel_format::rgba8888, width, height, src_stride, src.data() + 1)
    );
    EXPECT_EQ(dst[0], std::byte{0x5a});
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            alia::px_rgb565 actual;
            std::memcpy(&actual, dst.data() + 1 + y * dst_stride + x * sizeof(actual), sizeof(actual));
            EXPECT_EQ(alia::get_red(actual), (static_cast<unsigned>(x % 256) * 31u + 127u) / 255u);
            EXPECT_EQ(alia::get_green(actual), (static_cast<unsigned>((x / 2) % 256) * 63u + 127u) / 255u);
            EXPECT_EQ(alia::get_blue(actual), (static_cast<unsigned>(y * 127) * 31u + 127u) / 255u);
        }
        for (int i = width * sizeof(alia::px_rgb565); i < dst_stride; ++i)
            EXPECT_EQ(dst[1 + y * dst_stride + i], std::byte{0x5a});
    }
}

TEST(BitmapLossyBlitTest, FloatHubSupportsUnalignedRowsAndMultipleChunks) {
    constexpr int width = 257;
    constexpr int height = 2;
    constexpr int stride = width * sizeof(alia::px_rgba_f32) + 1;
    std::array<std::byte, stride * height + 1> src{};
    std::array<alia::px_bgra8888, width * height> dst{};
    const alia::px_rgba_f32 sample{0.5f, 1.5f, -0.5f, 0.25f};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x)
            std::memcpy(src.data() + 1 + y * stride + x * sizeof(sample), &sample, sizeof(sample));
    }
    alia::converting_blit_lossy(
        make_any_view(dst, width, height),
        alia::any_bitmap_view(alia::pixel_format::rgba_f32, width, height, stride, src.data() + 1)
    );
    for (const auto px : dst)
        expect_pixels_equal(px, alia::px_bgra8888{0, 255, 128, 64});
}

TEST(BitmapLossyBlitTest, SameFormatCopiesFloatRepresentationUnchanged) {
    std::array<alia::px_rgba_f32, 2> src{{
        {2.25f, -0.5f, -0.0f, std::numeric_limits<float>::quiet_NaN()},
        {0.5f, std::numeric_limits<float>::infinity(), 1.0f, 0.25f},
    }};
    std::array<alia::px_rgba_f32, 2> dst{};
    alia::converting_blit_lossy(make_any_view(dst, 2, 1), make_any_view(src, 2, 1));
    EXPECT_EQ(std::memcmp(src.data(), dst.data(), sizeof(src)), 0);
}

TEST(PixelRowsVisitorTest, RGBWritebackCrossesChunkBoundariesAndPreservesPadding) {
    constexpr int width = 513;
    constexpr int height = 2;
    constexpr int stride = width * sizeof(alia::px_rgb888) + 3;
    std::array<std::byte, stride * height + 1> data;
    data.fill(std::byte{0x5a});
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const alia::px_rgb888 px{
                static_cast<std::uint8_t>(x), 7, static_cast<std::uint8_t>(y * 100),
            };
            std::memcpy(data.data() + 1 + y * stride + x * sizeof(px), &px, sizeof(px));
        }
    }
    int rows = 0;
    EXPECT_TRUE((alia::detail::visit_pixel_rows<alia::px_rgba8888, true, true>(
        alia::pixel_format::rgb888, data.data() + 1, stride, {width, height},
        [&](std::span<alia::px_rgba8888> row, int y) {
            EXPECT_EQ(y, rows++);
            EXPECT_EQ(row.size(), width);
            for (int x = 0; x < width; ++x) {
                EXPECT_EQ(row[x].r, static_cast<std::uint8_t>(x));
                EXPECT_EQ(row[x].g, 7);
                EXPECT_EQ(row[x].b, y * 100);
                EXPECT_EQ(row[x].a, 255);
                row[x].g = 255;
            }
        })));
    EXPECT_EQ(rows, height);
    EXPECT_EQ(data[0], std::byte{0x5a});
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            alia::px_rgb888 px;
            std::memcpy(&px, data.data() + 1 + y * stride + x * sizeof(px), sizeof(px));
            expect_pixels_equal(px, alia::px_rgb888{
                static_cast<std::uint8_t>(x), 255, static_cast<std::uint8_t>(y * 100),
            });
        }
        for (int i = width * sizeof(alia::px_rgb888); i < stride; ++i)
            EXPECT_EQ(data[1 + y * stride + i], std::byte{0x5a});
    }
}

TEST(PixelRowsVisitorTest, MatchingFormatUsesLockedRowsDirectly) {
    std::array<alia::px_rgba8888, 8> pixels{};
    constexpr int width = 3;
    constexpr int stride = 4 * sizeof(alia::px_rgba8888);
    auto *data = reinterpret_cast<std::byte *>(pixels.data());
    int rows = 0;
    EXPECT_TRUE((alia::detail::visit_pixel_rows<alia::px_rgba8888, true, true>(
        alia::pixel_format::rgba8888, data, stride, {width, 2},
        [&](std::span<alia::px_rgba8888> row, int y) {
            ++rows;
            EXPECT_EQ(row.data(), pixels.data() + y * 4);
            EXPECT_EQ(row.size(), width);
            for (auto &px : row)
                px.g = 255;
        })));
    EXPECT_EQ(rows, 2);
    EXPECT_EQ(pixels[0].g, 255);
    EXPECT_EQ(pixels[4].g, 255);
    EXPECT_EQ(pixels[3].g, 0);
    EXPECT_EQ(pixels[7].g, 0);

    EXPECT_TRUE((alia::detail::visit_pixel_rows<alia::px_rgba8888, true, false>(
        alia::pixel_format::rgba8888, data, stride, {width, 2},
        [&](auto row, int y) {
            static_assert(std::is_const_v<typename decltype(row)::element_type>);
            EXPECT_EQ(row.data(), pixels.data() + y * 4);
            EXPECT_EQ(row[0].g, 255);
        })));
}

TEST(PixelRowsVisitorTest, U8HubRejectsReadWriteFloatButAllowsReadOnly) {
    std::array<alia::px_rgba_f32, 1> pixels{{{2.5f, -0.5f, 0.5f, 0.25f}}};
    const auto original = pixels;
    auto *data = reinterpret_cast<std::byte *>(pixels.data());
    int calls = 0;
    EXPECT_FALSE((alia::detail::visit_pixel_rows<alia::px_rgba8888, true, true>(
        alia::pixel_format::rgba_f32, data, sizeof(alia::px_rgba_f32), {1, 1},
        [&](std::span<alia::px_rgba8888>, int) { ++calls; })));
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE((alia::detail::visit_pixel_rows<alia::px_rgba8888, true, false>(
        alia::pixel_format::rgba_f32, data, sizeof(alia::px_rgba_f32), {1, 1},
        [&](std::span<const alia::px_rgba8888> row, int y) {
            ++calls;
            EXPECT_EQ(y, 0);
            expect_pixels_equal(row[0], alia::px_rgba8888{255, 0, 128, 64});
        })));
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(std::memcmp(pixels.data(), original.data(), sizeof(pixels)), 0);
}

TEST(PixelRowsVisitorTest, FloatHubPreservesUntouchedHDRChannels) {
    std::array<alia::px_rgb_f32, 2> pixels{{{2.5f, -0.5f, 0.125f}, {7.0f, 1.5f, -2.0f}}};
    const auto original = pixels;
    EXPECT_TRUE((alia::detail::visit_pixel_rows<alia::px_rgba_f32, true, true>(
        alia::pixel_format::rgb_f32, reinterpret_cast<std::byte *>(pixels.data()),
        sizeof(pixels), {2, 1}, [](std::span<alia::px_rgba_f32> row, int) {
            EXPECT_FLOAT_EQ(row[0].a, 1.0f);
            row[0].g = 3.5f;
        })));
    EXPECT_FLOAT_EQ(pixels[0].g, 3.5f);
    EXPECT_FLOAT_EQ(pixels[0].r, original[0].r);
    EXPECT_FLOAT_EQ(pixels[0].b, original[0].b);
    EXPECT_EQ(std::memcmp(&pixels[1], &original[1], sizeof(pixels[1])), 0);
}

TEST(PixelRowsVisitorTest, WriteOnlyConvertsHubRowsBackToNativeFormat) {
    std::array<alia::px_rgb888, 4> pixels{};
    int calls = 0;
    EXPECT_TRUE((alia::detail::visit_pixel_rows<alia::px_rgba8888, false, true>(
        alia::pixel_format::rgb888, reinterpret_cast<std::byte *>(pixels.data()),
        2 * sizeof(alia::px_rgb888), {2, 2}, [&](std::span<alia::px_rgba8888> row, int y) {
            if (y == 0) {
                for (const auto px : row)
                    expect_pixels_equal(px, alia::px_rgba8888{});
            }
            ++calls;
            for (auto &px : row)
                px = {10, static_cast<std::uint8_t>(20 + y), 30, 40};
        })));
    EXPECT_EQ(calls, 2);
    expect_pixels_equal(pixels[0], alia::px_rgb888{10, 20, 30});
    expect_pixels_equal(pixels[1], alia::px_rgb888{10, 20, 30});
    expect_pixels_equal(pixels[2], alia::px_rgb888{10, 21, 30});
    expect_pixels_equal(pixels[3], alia::px_rgb888{10, 21, 30});
}

TEST(PixelRowsVisitorTest, WriteOnlyU8HubAllowsFloatStorage) {
    std::array<alia::px_rgba_f32, 1> pixels{{{9.0f, 9.0f, 9.0f, 9.0f}}};
    EXPECT_TRUE((alia::detail::visit_pixel_rows<alia::px_rgba8888, false, true>(
        alia::pixel_format::rgba_f32, reinterpret_cast<std::byte *>(pixels.data()),
        sizeof(pixels), {1, 1}, [](std::span<alia::px_rgba8888> row, int) {
            row[0] = {255, 0, 255, 255};
        })));
    expect_pixels_equal(pixels[0], alia::px_rgba_f32{1.0f, 0.0f, 1.0f, 1.0f});
}

TEST(PixelRowsVisitorTest, RejectsPaletteAndUnknownFormatsWithoutCallingVisitor) {
    int calls = 0;
    auto visitor = [&](std::span<alia::px_rgba8888>, int) { ++calls; };
    for (const auto fmt : {alia::pixel_format::palette_u8, static_cast<alia::pixel_format>(-1)}) {
        EXPECT_FALSE((alia::detail::visit_pixel_rows<alia::px_rgba8888, true, true>(
            fmt, nullptr, 1, {1, 1}, visitor)));
    }
    EXPECT_EQ(calls, 0);
}

TEST(PixelViewVisitorTest, DispatchesMatchingNativeTypeWithoutCopying) {
    std::array<alia::px_rgb888, 2> pixels{{{1, 2, 3}, {4, 5, 6}}};
    auto *data = reinterpret_cast<std::byte *>(pixels.data());
    int calls = 0;
    EXPECT_TRUE(alia::detail::visit_pixel_view<false>(
        alia::pixel_format::rgb888, data, sizeof(pixels), {2, 1},
        [&]<alia::pixel P>(alia::bitmap_view<P> &view) {
            ++calls;
            EXPECT_EQ(P::format_id, alia::pixel_format::rgb888);
            EXPECT_EQ(view.line(0).data(), reinterpret_cast<P *>(pixels.data()));
            EXPECT_EQ(view.width(), 2);
            EXPECT_EQ(view.height(), 1);
            if constexpr (std::same_as<P, alia::px_rgb888>)
                view[0, 0].g = 255;
        }));
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(pixels[0].g, 255);
}

TEST(PixelViewVisitorTest, ConstrainedVisitorRejectsPaletteAndUnknownFormats) {
    int calls = 0;
    auto visitor = [&]<alia::hub_convertible_pixel P>(alia::bitmap_view<P> &) { ++calls; };
    EXPECT_FALSE(alia::detail::visit_pixel_view<false>(
        alia::pixel_format::palette_u8, nullptr, 1, {1, 1}, visitor));
    EXPECT_FALSE(alia::detail::visit_pixel_view<false>(
        static_cast<alia::pixel_format>(-1), nullptr, 1, {1, 1}, visitor));
    EXPECT_EQ(calls, 0);
}

TEST(PixelViewVisitorTest, ReadOnlyPassesAConstViewAndRejectsMutableCallbacks) {
    std::array<alia::px_rgba8888, 1> pixels{{{1, 2, 3, 4}}};
    auto *data = reinterpret_cast<std::byte *>(pixels.data());
    int calls = 0;
    EXPECT_TRUE(alia::detail::visit_pixel_view<true>(
        alia::pixel_format::rgba8888, data, sizeof(pixels), {1, 1},
        [&](auto &view) {
            static_assert(std::is_const_v<std::remove_reference_t<decltype(view)>>);
            static_assert(std::is_const_v<std::remove_reference_t<decltype(view[0, 0])>>);
            ++calls;
            EXPECT_EQ(static_cast<const void *>(view.line(0).data()), static_cast<const void *>(data));
        }));
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(alia::detail::visit_pixel_view<true>(
        alia::pixel_format::rgba8888, data, sizeof(pixels), {1, 1},
        [&]<alia::pixel P>(alia::bitmap_view<P> &) { ++calls; }));
    EXPECT_EQ(calls, 1);
}
