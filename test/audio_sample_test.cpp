#include "alia/audio/sample_conversions.hpp"
#include <gtest/gtest.h>
#include <limits>

struct padded_audio_frame { float point; char padding; };
namespace std {
    template <> struct tuple_size<padded_audio_frame> : integral_constant<size_t, 1> {};
    template <> struct tuple_element<0, padded_audio_frame> { using type = float; };
}
template <> inline constexpr bool alia::enable_contiguous_sample_frame<padded_audio_frame> = true;

static_assert(alia::sample_frame<float>);
static_assert(alia::sample_frame<std::int16_t>);
static_assert(alia::sample_frame<std::uint8_t>);
static_assert(alia::sample_frame<alia::vec2f>);
static_assert(alia::sample_frame<std::array<std::int16_t, 6>>);
static_assert(!alia::sample_frame<bool>);
static_assert(!alia::sample_frame<std::tuple<float, float>>);
static_assert(!alia::sample_frame<std::pair<float, int>>);
static_assert(!alia::sample_frame<padded_audio_frame>);
static_assert(!alia::sample_frame<std::array<float, 0>>);
static_assert(std::same_as<alia::sample_point_t<const alia::vec2f>, float>);
static_assert(alia::channel_count_v<std::array<float, 6>> == 6);
static_assert(alia::sample_frame_format_of_v<alia::vec2f> == alia::sample_frame_format{alia::sample_point_formats::f32, 2});
static_assert(alia::sample_frame_format_of_v<std::uint8_t> == alia::sample_frame_format{alia::sample_point_formats::u8, 1});

TEST(AudioSample, SignedAndUnsignedFullScale) {
    using alia::convert_sample_point;
    EXPECT_FLOAT_EQ(convert_sample_point<float>(std::int16_t{-32768}), -1);
    EXPECT_FLOAT_EQ(convert_sample_point<float>(std::int16_t{32767}), 32767.0f / 32768);
    EXPECT_FLOAT_EQ(convert_sample_point<float>(std::uint8_t{0}), -1);
    EXPECT_FLOAT_EQ(convert_sample_point<float>(std::uint8_t{128}), 0);
    EXPECT_FLOAT_EQ(convert_sample_point<float>(std::uint8_t{255}), 127.0f / 128);
    EXPECT_EQ(convert_sample_point<std::int16_t>(-1.0f), -32768);
    EXPECT_EQ(convert_sample_point<std::int16_t>(1.0f), 32767);
    EXPECT_EQ(convert_sample_point<std::int16_t>(2.0f), 32767);
    EXPECT_EQ(convert_sample_point<std::int16_t>(-2.0f), -32768);
    EXPECT_EQ(convert_sample_point<std::uint8_t>(0.0f), 128);
    EXPECT_EQ(convert_sample_point<std::uint8_t>(1.0f), 255);
    EXPECT_EQ(convert_sample_point<std::uint8_t>(-1.0f), 0);
    EXPECT_EQ(convert_sample_point<std::uint16_t>((std::numeric_limits<float>::quiet_NaN)()), 32768);
    EXPECT_EQ(convert_sample_point<std::int16_t>(0.5f / 32768), 1);
    EXPECT_EQ(convert_sample_point<std::int16_t>(-0.5f / 32768), -1);
}
TEST(AudioSample, IntegerShiftsAndSignedness) {
    using alia::convert_sample_point;
    EXPECT_EQ(convert_sample_point<std::int16_t>(std::uint8_t{0}), -32768);
    EXPECT_EQ(convert_sample_point<std::int16_t>(std::uint8_t{128}), 0);
    EXPECT_EQ(convert_sample_point<std::uint8_t>(std::int16_t{32767}), 255);
    EXPECT_EQ(convert_sample_point<std::int32_t>(std::int16_t{-1}), -65536);
    EXPECT_EQ(convert_sample_point<std::int16_t>(std::int32_t{-1}), -1);
    EXPECT_EQ(convert_sample_point<std::uint64_t>(std::int64_t{0}), std::uint64_t{1} << 63);
    EXPECT_DOUBLE_EQ(convert_sample_point<double>((std::uint64_t{1} << 63) + 1), std::ldexp(1.0, -63));
    EXPECT_EQ(convert_sample_point<std::uint64_t>(std::ldexp(1.0, -63)), (std::uint64_t{1} << 63) + 1);
    EXPECT_EQ(convert_sample_point<std::uint64_t>(-std::ldexp(1.0, -63)), (std::uint64_t{1} << 63) - 1);
}
TEST(AudioSample, AllSigned16BitValuesRoundTripThroughFloat) {
    for (int value = -32768; value <= 32767; ++value) {
        const auto point = static_cast<std::int16_t>(value);
        ASSERT_EQ(alia::convert_sample_point<std::int16_t>(alia::convert_sample_point<float>(point)), point);
    }
}
TEST(AudioSample, SilenceUsesUnsignedMidpoint) {
    std::array<std::array<std::uint16_t, 2>, 3> frames{};
    alia::fill_silence(alia::audio_clip_view(48000, std::span(frames)));
    for (const auto &frame : frames) for (auto point : frame) EXPECT_EQ(point, 32768);
    alia::any_audio_clip_view view = alia::audio_clip_view(48000, std::span(frames));
    alia::fill_silence(view);
    EXPECT_EQ(frames[1][1], 32768);
}
