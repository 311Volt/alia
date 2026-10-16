#include "alia/audio/hermite_resampler.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>
#include <random>
#include <utility>
#include <vector>

namespace {
    constexpr alia::audio_stream_format stereo{{alia::sample_point_formats::f32, 2}, 48000};

    class gain_effect final : public alia::audio_effect {
    public:
        explicit gain_effect(alia::audio_stream_format input = stereo, alia::audio_stream_format output = stereo)
            : audio_effect(input, output) {
        }

        int process_calls = 0;
        int reset_calls = 0;

    private:
        alia::audio_effect_progress
        do_process(const float *in, std::size_t in_frames, float *out, std::size_t out_frames) override {
            ++process_calls;
            const auto count = (std::min)(in_frames, out_frames);
            const auto in_channels = static_cast<std::size_t>(input_format().frame.channels);
            const auto out_channels = static_cast<std::size_t>(output_format().frame.channels);
            for (std::size_t i = 0; i < count; ++i)
                for (std::size_t channel = 0; channel < out_channels; ++channel)
                    out[i * out_channels + channel] = 2 * in[i * in_channels + channel % in_channels];
            return {count, count};
        }

        void do_reset() noexcept override {
            ++reset_calls;
        }

        std::size_t do_input_frames_for_output(std::size_t n) const override {
            return n;
        }

        std::size_t do_output_frames_for_input(std::size_t m) const override {
            return m;
        }
    };

    void expect_identical(std::span<const float> actual, std::span<const float> expected) {
        ASSERT_EQ(actual.size(), expected.size());
        if (!actual.empty())
            EXPECT_EQ(std::memcmp(actual.data(), expected.data(), actual.size_bytes()), 0);
    }

    std::vector<float> random_points(std::size_t count, std::mt19937 &random) {
        std::vector<float> points(count);
        std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
        for (auto &point : points)
            point = distribution(random);
        return points;
    }

    // Independent reference counts use absolute stream positions, without any
    // knowledge of the resampler's window, compaction or skip state.
    std::size_t reference_input_count(
        std::size_t consumed, std::size_t produced, std::size_t n, int input_rate, int output_rate
    ) {
        if (!n)
            return 0;
        const auto end = (static_cast<std::uint64_t>(produced) + n - 1) * input_rate / output_rate + 4;
        return end > consumed ? static_cast<std::size_t>(end - consumed) : 0;
    }

    std::size_t reference_output_count(
        std::size_t consumed, std::size_t produced, std::size_t m, int input_rate, int output_rate
    ) {
        const auto available = static_cast<std::uint64_t>(consumed) + m;
        const auto total = available < 4 ? 0 : ((available - 3) * output_rate - 1) / input_rate + 1;
        return static_cast<std::size_t>(total - produced);
    }

    class HermiteResamplerRatio : public ::testing::TestWithParam<std::pair<int, int>> {};
} // namespace

static_assert(!std::is_copy_constructible_v<alia::hermite_resampler>);
static_assert(std::is_nothrow_move_constructible_v<alia::hermite_resampler>);
static_assert(std::is_nothrow_move_assignable_v<alia::hermite_resampler>);

TEST(AudioEffect, RejectsInvalidConstructionFormats) {
    for (const auto format : {
             alia::audio_stream_format{{alia::sample_point_formats::s16, 2}, 48000},
             alia::audio_stream_format{{alia::sample_point_formats::f64, 2}, 48000},
             alia::audio_stream_format{{alia::sample_point_formats::f32, 0}, 48000},
             alia::audio_stream_format{{alia::sample_point_formats::f32, -1}, 48000},
             alia::audio_stream_format{{alia::sample_point_formats::f32, 2}, 0},
             alia::audio_stream_format{{alia::sample_point_formats::f32, 2}, -1}
         }) {
        EXPECT_THROW((gain_effect{format, stereo}), std::invalid_argument);
        EXPECT_THROW((gain_effect{stereo, format}), std::invalid_argument);
    }
    EXPECT_THROW((alia::hermite_resampler{0, 44100, 48000}), std::invalid_argument);
    EXPECT_THROW((alia::hermite_resampler{2, 0, 48000}), std::invalid_argument);
    EXPECT_THROW((alia::hermite_resampler{2, 44100, 0}), std::invalid_argument);
}

TEST(AudioEffect, RejectsMismatchedViewsBeforeProcessing) {
    gain_effect effect;
    std::array<float, 8> input{}, output{};
    const auto *in = reinterpret_cast<const std::byte *>(input.data());
    auto *out = reinterpret_cast<std::byte *>(output.data());
    const alia::const_audio_clip_view good_input(stereo.frame, 48000, 2, in);
    const alia::any_audio_clip_view good_output(stereo.frame, 48000, 2, out);
    EXPECT_THROW(
        effect.process(alia::const_audio_clip_view({alia::sample_point_formats::f32, 1}, 48000, 2, in), good_output),
        std::invalid_argument
    );
    EXPECT_THROW(
        effect.process(good_input, alia::any_audio_clip_view({alia::sample_point_formats::s16, 2}, 48000, 2, out)),
        std::invalid_argument
    );
    EXPECT_THROW(
        effect.process(alia::const_audio_clip_view(stereo.frame, 44100, 2, in), good_output), std::invalid_argument
    );
    EXPECT_THROW(
        effect.process(good_input, alia::any_audio_clip_view(stereo.frame, 44100, 2, out)), std::invalid_argument
    );
    // Empty views still require the correct frame format.
    EXPECT_THROW(effect.process(alia::const_audio_clip_view{}, good_output), std::invalid_argument);
    EXPECT_THROW(effect.process(good_input, alia::any_audio_clip_view{}), std::invalid_argument);
    EXPECT_EQ(effect.process_calls, 0);
}

TEST(AudioEffect, RejectsIncompleteFramesOverlapAndMisalignment) {
    gain_effect effect;
    std::array<float, 12> input{}, output{};
    EXPECT_THROW(effect.process(std::span(input).first(3), output), std::invalid_argument);
    EXPECT_THROW(effect.process(input, std::span(output).first(3)), std::invalid_argument);
    EXPECT_THROW(effect.process(input, input), std::invalid_argument);
    EXPECT_THROW(effect.process(std::span(input).first(8), std::span(input).subspan(4, 8)), std::invalid_argument);
    EXPECT_THROW(effect.process(std::span(input).subspan(4, 8), std::span(input).first(8)), std::invalid_argument);
    EXPECT_THROW(
        effect.process(
            alia::const_audio_clip_view(stereo.frame, 48000, 2, reinterpret_cast<const std::byte *>(input.data())),
            alia::any_audio_clip_view(stereo.frame, 48000, 2, reinterpret_cast<std::byte *>(input.data()))
        ),
        std::invalid_argument
    );

    alignas(float) std::array<std::byte, 2 * sizeof(float) + alignof(float)> bytes{};
    const alia::const_audio_clip_view unaligned_input(stereo.frame, 48000, 1, bytes.data() + 1);
    const alia::any_audio_clip_view unaligned_output(stereo.frame, 48000, 1, bytes.data() + 1);
    const alia::const_audio_clip_view good_input(
        stereo.frame, 48000, 1, reinterpret_cast<const std::byte *>(input.data())
    );
    const alia::any_audio_clip_view good_output(stereo.frame, 48000, 1, reinterpret_cast<std::byte *>(output.data()));
    EXPECT_THROW(effect.process(unaligned_input, good_output), std::invalid_argument);
    EXPECT_THROW(effect.process(good_input, unaligned_output), std::invalid_argument);
    EXPECT_EQ(effect.process_calls, 0);

    // Adjacent ranges in one allocation do not overlap.
    EXPECT_EQ(
        effect.process(std::span(input).first(6), std::span(input).subspan(6)), (alia::audio_effect_progress{3, 3})
    );
}

TEST(AudioEffect, EmptyBuffersDoNotCallAStatelessEffect) {
    gain_effect effect;
    std::array<float, 4> input{}, output{};
    EXPECT_EQ(effect.process(std::span<const float>{}, output), alia::audio_effect_progress{});
    EXPECT_EQ(effect.process(input, std::span<float>{}), alia::audio_effect_progress{});
    EXPECT_EQ(effect.process(std::span<const float>{}, std::span<float>{}), alia::audio_effect_progress{});
    // An empty view can have rate zero or another rate; only its frame format matters.
    EXPECT_EQ(
        effect.process({stereo.frame, 0, 0, nullptr}, {stereo.frame, 44100, 0, nullptr}), alia::audio_effect_progress{}
    );
    EXPECT_EQ(effect.process_calls, 0);
    EXPECT_EQ(effect.input_frames_for_output(17), 17);
    EXPECT_EQ(effect.output_frames_for_input(11), 11);
    effect.reset();
    EXPECT_EQ(effect.reset_calls, 1);
}

TEST(AudioEffect, AcceptsDifferentInputAndOutputChannelsAndRates) {
    const alia::audio_stream_format mono{{alia::sample_point_formats::f32, 1}, 44100};
    gain_effect effect(mono, stereo);
    EXPECT_EQ(effect.input_format(), mono);
    EXPECT_EQ(effect.output_format(), stereo);
    std::array<float, 2> input{0.25f, -0.5f};
    std::array<alia::vec2f, 2> output{};
    const auto source = static_cast<alia::const_audio_clip_view>(alia::audio_clip_view(44100, std::span(input)));
    auto destination = static_cast<alia::any_audio_clip_view>(alia::audio_clip_view(48000, std::span(output)));
    EXPECT_EQ(effect.process(source, destination), (alia::audio_effect_progress{2, 2}));
    EXPECT_EQ(output[0], (alia::vec2f{0.5f, 0.5f}));
    EXPECT_EQ(output[1], (alia::vec2f{-1, -1}));
}

TEST(HermiteResampler, EqualRatesPreserveEveryBitWithThreeFrameLookahead) {
    constexpr std::size_t count = 4096;
    std::mt19937 random(818);
    auto input = random_points((count + 3) * 2, random);
    input[0] = -0.0f;
    input[1] = std::bit_cast<float>(std::uint32_t{0x7fc01234});
    input[2] = (std::numeric_limits<float>::infinity)();
    input[3] = -(std::numeric_limits<float>::infinity)();
    input[4] = (std::numeric_limits<float>::denorm_min)();
    alia::hermite_resampler effect(2, 48000, 48000);
    EXPECT_EQ(effect.input_frames_for_output(0), 0);
    EXPECT_EQ(effect.input_frames_for_output(count), count + 3);
    EXPECT_EQ(effect.output_frames_for_input(3), 0);
    EXPECT_EQ(effect.output_frames_for_input(count + 3), count);
    std::vector<float> output(count * 2);
    EXPECT_EQ(effect.process(input, output), (alia::audio_effect_progress{count + 3, count}));
    expect_identical(output, std::span(input).first(count * 2));
}

TEST(HermiteResampler, EmptyInputDrainsBufferedUpsampledOutput) {
    alia::hermite_resampler effect(1, 8000, 192000);
    const std::array input{1.0f, 2.0f, 3.0f, 4.0f};
    std::array<float, 1> first{};
    EXPECT_EQ(effect.process(input, first), (alia::audio_effect_progress{4, 1}));
    EXPECT_EQ(first[0], input[0]);
    EXPECT_EQ(effect.input_frames_for_output(23), 0);
    EXPECT_EQ(effect.output_frames_for_input(0), 23);
    EXPECT_EQ(effect.process(std::span<const float>{}, std::span<float>{}), alia::audio_effect_progress{});
    EXPECT_EQ(effect.output_frames_for_input(0), 23);
    std::array<float, 23> tail{};
    EXPECT_EQ(
        effect.process(std::span<const float>{}, std::span(tail).first(7)), (alia::audio_effect_progress{0, 7})
    );
    EXPECT_EQ(effect.output_frames_for_input(0), 16);
    const auto destination = static_cast<alia::any_audio_clip_view>(
        alia::audio_clip_view(192000, std::span(tail).subspan(7))
    );
    EXPECT_EQ(effect.process(alia::const_audio_clip_view{}, destination), (alia::audio_effect_progress{0, 16}));
    EXPECT_EQ(effect.output_frames_for_input(0), 0);
    EXPECT_EQ(effect.input_frames_for_output(1), 1);
    EXPECT_EQ(effect.process(std::span<const float>{}, first), alia::audio_effect_progress{});
}

TEST(HermiteResampler, ReproducesACubicAwayFromTheZeroHistoryStart) {
    const auto cubic = [](double frame) {
        const auto x = frame / 512;
        return ((0.03125 * x - 0.125) * x + 0.25) * x - 0.5;
    };
    std::vector<float> input(515);
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<float>(cubic(static_cast<double>(i)));
    alia::hermite_resampler effect(1, 44100, 48000);
    std::vector<float> output(effect.output_frames_for_input(input.size()));
    ASSERT_EQ(effect.process(input, output).produced, output.size());
    for (std::size_t k = 3; k < output.size(); ++k)
        EXPECT_NEAR(output[k], cubic(static_cast<double>(k) * 441 / 480), 4 * std::numeric_limits<float>::epsilon())
            << "output frame " << k;
}

TEST(HermiteResampler, ConvertsOneSecondOfStereoSineWithoutDrift) {
    constexpr std::size_t input_frames = 44100 + alia::hermite_resampler::lookahead_frames;
    std::vector<float> input(input_frames * 2);
    for (std::size_t i = 0; i < input_frames; ++i) {
        const auto phase = 2 * std::numbers::pi * 1000 * static_cast<double>(i) / 44100;
        input[2 * i] = static_cast<float>(std::sin(phase));
        input[2 * i + 1] = static_cast<float>(std::cos(phase));
    }
    alia::hermite_resampler effect(2, 44100, 48000);
    EXPECT_EQ(effect.input_frames_for_output(48000), input_frames);
    EXPECT_EQ(effect.output_frames_for_input(input_frames), 48000);
    std::vector<float> output(48000 * 2);
    ASSERT_EQ(effect.process(input, output), (alia::audio_effect_progress{input_frames, 48000}));
    double maximum_error = 0;
    // The first two fractional positions touch the explicitly silent prehistory.
    for (std::size_t k = 3; k < 48000; ++k) {
        const auto phase = 2 * std::numbers::pi * 1000 * static_cast<double>(k) / 48000;
        maximum_error = (std::max)(maximum_error, std::abs(output[2 * k] - std::sin(phase)));
        maximum_error = (std::max)(maximum_error, std::abs(output[2 * k + 1] - std::cos(phase)));
    }
    EXPECT_LT(maximum_error, 1e-4);
}

TEST_P(HermiteResamplerRatio, ChunkSizesDoNotChangeAnyOutputBits) {
    const auto [input_rate, output_rate] = GetParam();
    constexpr std::size_t channels = 2;
    constexpr std::size_t input_frames = 4099;
    std::mt19937 random(740);
    const auto input = random_points(input_frames * channels, random);
    alia::hermite_resampler whole(channels, input_rate, output_rate);
    const auto output_frames = reference_output_count(0, 0, input_frames, input_rate, output_rate);
    ASSERT_EQ(whole.output_frames_for_input(input_frames), output_frames);
    std::vector<float> expected(output_frames * channels);
    ASSERT_EQ(whole.process(input, expected).produced, output_frames);

    alia::hermite_resampler chunks(channels, input_rate, output_rate);
    std::vector<float> actual(expected.size());
    std::size_t consumed = 0, produced = 0;
    while (produced < output_frames) {
        const auto input_count = (std::min)(input_frames - consumed, std::size_t{1} + random() % 113);
        const auto output_count = (std::min)(output_frames - produced, std::size_t{1} + random() % 97);
        const auto progress = chunks.process(
            std::span(input).subspan(consumed * channels, input_count * channels),
            std::span(actual).subspan(produced * channels, output_count * channels)
        );
        ASSERT_LE(progress.consumed, input_count);
        ASSERT_LE(progress.produced, output_count);
        ASSERT_TRUE(progress.consumed || progress.produced);
        consumed += progress.consumed;
        produced += progress.produced;
    }
    expect_identical(actual, expected);
}

TEST_P(HermiteResamplerRatio, QueriesAreExactAcrossRandomStates) {
    const auto [input_rate, output_rate] = GetParam();
    constexpr std::size_t channels = 2;
    alia::hermite_resampler effect(channels, input_rate, output_rate);
    std::mt19937 random(921);
    std::size_t consumed = 0, produced = 0;
    for (int iteration = 0; iteration < 400; ++iteration) {
        SCOPED_TRACE(iteration);
        if (iteration % 2 == 0) {
            const auto n = static_cast<std::size_t>(random() % 74);
            const auto need = reference_input_count(consumed, produced, n, input_rate, output_rate);
            ASSERT_EQ(effect.input_frames_for_output(n), need);
            // Sometimes offer surplus input to verify that output capacity limits
            // consumption to exactly the queried amount.
            const auto extra = iteration % 4 == 0 ? 17 : 0;
            const auto input = random_points((need + extra) * channels, random);
            std::vector<float> output(n * channels);
            const auto progress = effect.process(input, output);
            ASSERT_EQ(progress, (alia::audio_effect_progress{need, n}));
            consumed += progress.consumed;
            produced += progress.produced;
        } else {
            const auto m = static_cast<std::size_t>(random() % 128);
            const auto expected = reference_output_count(consumed, produced, m, input_rate, output_rate);
            ASSERT_EQ(effect.output_frames_for_input(m), expected);
            const auto input = random_points(m * channels, random);
            // One spare slot ensures the call is limited by input, including
            // calls which only accumulate lookahead or discard skipped frames.
            std::vector<float> output((expected + 1) * channels, 123.0f);
            const auto progress = effect.process(input, output);
            ASSERT_EQ(progress, (alia::audio_effect_progress{m, expected}));
            EXPECT_EQ(output[expected * channels], 123.0f);
            EXPECT_EQ(output[expected * channels + 1], 123.0f);
            consumed += progress.consumed;
            produced += progress.produced;
        }
        EXPECT_EQ(
            effect.output_frames_for_input(0), reference_output_count(consumed, produced, 0, input_rate, output_rate)
        );
    }
}

TEST_P(HermiteResamplerRatio, ChannelsRemainIsolatedAndResetRestoresFreshState) {
    const auto [input_rate, output_rate] = GetParam();
    std::mt19937 random(331);
    auto input = random_points(1027 * 2, random);
    for (std::size_t i = 1; i < input.size(); i += 2)
        input[i] = 0;
    alia::hermite_resampler fresh(2, input_rate, output_rate);
    alia::hermite_resampler reset(2, input_rate, output_rate);
    std::array<float, 14> prefix_output{};
    reset.process(std::span(input).first(64), prefix_output);
    reset.reset();
    const auto count = fresh.output_frames_for_input(input.size() / 2);
    EXPECT_EQ(reset.output_frames_for_input(input.size() / 2), count);
    EXPECT_EQ(reset.input_frames_for_output(count), fresh.input_frames_for_output(count));
    std::vector<float> expected(count * 2), actual(count * 2);
    EXPECT_EQ(reset.process(input, actual), fresh.process(input, expected));
    expect_identical(actual, expected);
    for (std::size_t i = 1; i < actual.size(); i += 2)
        EXPECT_EQ(actual[i], 0.0f);
}

TEST_P(HermiteResamplerRatio, ThreeSilentLookaheadFramesFlushTheSourceDuration) {
    const auto [input_rate, output_rate] = GetParam();
    constexpr std::size_t source_frames = 1013;
    std::vector<float> input(source_frames + alia::hermite_resampler::lookahead_frames, 0.0f);
    std::fill_n(input.begin(), source_frames, 0.5f);
    const auto count = static_cast<std::size_t>(
        (static_cast<std::uint64_t>(source_frames) * output_rate + input_rate - 1) / input_rate
    );
    alia::hermite_resampler effect(1, input_rate, output_rate);
    std::vector<float> output(count + 1, 123.0f);
    const auto prefix = effect.process(std::span(input).first(source_frames), output);
    ASSERT_EQ(prefix.consumed, source_frames);
    const auto tail = effect.process(std::span(input).subspan(source_frames), std::span(output).subspan(prefix.produced));
    EXPECT_EQ(tail.consumed, alia::hermite_resampler::lookahead_frames);
    EXPECT_EQ(prefix.produced + tail.produced, count);
    EXPECT_EQ(effect.output_frames_for_input(0), 0);
    EXPECT_EQ(output[count], 123.0f);
}

TEST_P(HermiteResamplerRatio, MovingPreservesBufferedInputAndPhase) {
    const auto [input_rate, output_rate] = GetParam();
    std::mt19937 random(443);
    const auto input = random_points(1027 * 2, random);
    alia::hermite_resampler reference(2, input_rate, output_rate);
    alia::hermite_resampler source(2, input_rate, output_rate);
    std::array<float, 14> a{}, b{};
    const auto prefix = reference.process(std::span(input).first(64), a);
    ASSERT_EQ(source.process(std::span(input).first(64), b), prefix);
    auto moved = std::move(source);
    alia::hermite_resampler assigned(1, 48000, 48000);
    assigned = std::move(moved);
    EXPECT_EQ(assigned.input_format(), reference.input_format());
    EXPECT_EQ(assigned.output_format(), reference.output_format());
    const auto rest = std::span(input).subspan(prefix.consumed * 2);
    const auto count = reference.output_frames_for_input(rest.size() / 2);
    EXPECT_EQ(assigned.output_frames_for_input(rest.size() / 2), count);
    std::vector<float> actual(count * 2), expected(count * 2);
    EXPECT_EQ(assigned.process(rest, actual), reference.process(rest, expected));
    expect_identical(actual, expected);
}

INSTANTIATE_TEST_SUITE_P(
    Rates,
    HermiteResamplerRatio,
    ::testing::Values(
        std::pair{44100, 48000},
        std::pair{48000, 44100},
        std::pair{8000, 192000},
        std::pair{192000, 8000},
        std::pair{44100, 47999}
    )
);

TEST(HermiteResampler, HandlesExtremeIntegerRatesWithoutGrowingTheWindow) {
    constexpr auto maximum_rate = (std::numeric_limits<int>::max)();
    alia::hermite_resampler down(1, maximum_rate, 1);
    const std::array input{1.0f, 2.0f, 3.0f, 4.0f};
    std::array<float, 1> output{};
    EXPECT_EQ(down.process(input, output), (alia::audio_effect_progress{4, 1}));
    EXPECT_EQ(down.input_frames_for_output(1), static_cast<std::size_t>(maximum_rate));
    EXPECT_EQ(down.output_frames_for_input(16), 0);
    const std::array<float, 16> skipped{};
    EXPECT_EQ(down.process(skipped, output), (alia::audio_effect_progress{16, 0}));
    EXPECT_EQ(down.input_frames_for_output(1), static_cast<std::size_t>(maximum_rate) - 16);
    down.reset();
    EXPECT_EQ(down.input_frames_for_output(1), 4);

    alia::hermite_resampler up(1, 1, maximum_rate);
    EXPECT_EQ(up.process(input, output), (alia::audio_effect_progress{4, 1}));
    EXPECT_EQ(up.output_frames_for_input(0), static_cast<std::size_t>(maximum_rate) - 1);
    EXPECT_EQ(up.input_frames_for_output(static_cast<std::size_t>(maximum_rate) - 1), 0);
}

TEST(HermiteResampler, RejectsOverflowingQueriesWithoutChangingState) {
    const auto maximum = (std::numeric_limits<std::size_t>::max)();
    alia::hermite_resampler identity(1, 48000, 48000);
    EXPECT_THROW(identity.input_frames_for_output(maximum), std::length_error);
    EXPECT_THROW(identity.output_frames_for_input(maximum), std::length_error);
    EXPECT_EQ(identity.input_frames_for_output(1), 4);
    EXPECT_EQ(identity.output_frames_for_input(4), 1);
    alia::hermite_resampler down(1, 192000, 8000);
    alia::hermite_resampler up(1, 8000, 192000);
    EXPECT_THROW(down.input_frames_for_output(maximum), std::length_error);
    EXPECT_THROW(up.output_frames_for_input(maximum), std::length_error);
    EXPECT_EQ(down.input_frames_for_output(0), 0);
    EXPECT_EQ(up.input_frames_for_output(0), 0);
}
