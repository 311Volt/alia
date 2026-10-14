#include "alia/audio/sample_conversions.hpp"
#include <gtest/gtest.h>

static_assert(std::same_as<decltype(std::declval<const alia::audio_clip &>().view_as<float>().frames()), std::span<const float>>);
static_assert(std::same_as<decltype(std::declval<const alia::audio_clip_view<float> &>().subview(0, 1)), alia::audio_clip_view<const float>>);
static_assert(!std::is_convertible_v<alia::audio_clip_view<const float>, alia::any_audio_clip_view>);

TEST(AudioClip, OwnsCopiesAndClonesAndMoves) {
    std::array<alia::vec2f, 2> input{{{0.25f, -0.5f}, {0.75f, 1}}};
    alia::audio_clip clip(48000, std::span(input));
    input[0].x = 0;
    EXPECT_FLOAT_EQ(clip.view_as<alia::vec2f>()[0].x, 0.25f);
    auto clone = clip.clone();
    clone.view_as<alia::vec2f>()[0].x = 1;
    EXPECT_FLOAT_EQ(clip.view_as<alia::vec2f>()[0].x, 0.25f);
    EXPECT_EQ(clip.frame_count(), 2);
    EXPECT_DOUBLE_EQ(clip.duration(), 2.0 / 48000);
    EXPECT_EQ(clip.bytes().size(), 2 * sizeof(alia::vec2f));
    EXPECT_THROW(clip.view_as<float>(), std::invalid_argument);
    auto moved = std::move(clip);
    EXPECT_EQ(clip.frame_count(), 0);
    EXPECT_EQ(moved.frame_count(), 2);
    EXPECT_EQ(alia::audio_clip{}.clone().frame_count(), 0);
}
TEST(AudioClip, ClampsSubviewsAndChecksIndices) {
    alia::audio_clip clip(4, 48000, 0.5f);
    auto subview = clip.view_as<float>().subview(2, 99);
    EXPECT_EQ(subview.frame_count(), 2);
    subview[0] = 1;
    EXPECT_EQ(clip.view_as<float>()[2], 1);
    EXPECT_EQ(clip.subview(99, 2).frame_count(), 0);
    EXPECT_THROW(subview.at(2), std::out_of_range);
    EXPECT_EQ(clip.view().subview(3, 9).frame_count(), 1);
    EXPECT_EQ(clip.view().as<float>().subview(4, 9).frame_count(), 0);
}
TEST(AudioClip, ValidatesRawDataAndInitializesUnsignedSilence) {
    const std::array<std::byte, 3> bytes{};
    EXPECT_THROW((alia::audio_clip({alia::sample_point_formats::s16, 1}, 48000, bytes)), std::invalid_argument);
    EXPECT_THROW((alia::audio_clip(1, 0, 0.0f)), std::invalid_argument);
    alia::audio_clip clip(3, 48000, alia::detail::silent_frame<std::uint8_t>());
    EXPECT_EQ(clip.view_as<std::uint8_t>()[1], 128);
}
TEST(AudioClip, TypedAndErasedMonoStereoConversion) {
    std::array<std::int16_t, 3> mono{-32768, 0, 16384};
    std::array<alia::vec2f, 3> stereo{};
    auto src = alia::audio_clip_view(48000, std::span(mono));
    auto dst = alia::audio_clip_view(48000, std::span(stereo));
    alia::converting_copy(dst, src);
    EXPECT_EQ(stereo[0], (alia::vec2f{-1, -1}));
    EXPECT_EQ(stereo[2], (alia::vec2f{0.5f, 0.5f}));
    stereo[1] = {1, -0.5f};
    alia::converting_copy(static_cast<alia::any_audio_clip_view>(src), static_cast<alia::const_audio_clip_view>(dst));
    EXPECT_EQ(mono[1], 8192);
    mono = {0, 16384, -16384};
    alia::converting_copy(static_cast<alia::any_audio_clip_view>(dst), static_cast<alia::const_audio_clip_view>(src));
    EXPECT_EQ(stereo[2], (alia::vec2f{-0.5f, -0.5f}));
    alia::converting_copy(src, src, [](std::int16_t value) { return static_cast<std::int16_t>(value / 2); });
    EXPECT_EQ(mono[1], 8192);
}
TEST(AudioClip, RejectsIncompatibleSizesChannelsAndErasedEncoding) {
    alia::audio_clip a(2, 48000, alia::vec2f{}), b(2, 48000, alia::vec3f{}), c(3, 48000, 0.0f);
    EXPECT_THROW(alia::converting_copy(a.view_as<alia::vec2f>(), b.view_as<alia::vec3f>()), std::invalid_argument);
    EXPECT_THROW(alia::converting_copy(a.view(), b.view()), std::invalid_argument);
    EXPECT_THROW(alia::converting_copy(a.view(), c.view()), std::invalid_argument);
    EXPECT_THROW(alia::converting_copy(a.view_as<alia::vec2f>(), c.view_as<float>()), std::invalid_argument);
    alia::audio_clip wide(2, 48000, std::int64_t{0});
    EXPECT_THROW(alia::converting_copy(a.view(), wide.view()), std::invalid_argument);
}
