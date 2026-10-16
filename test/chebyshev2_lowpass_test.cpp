#include "alia/audio/chebyshev2_lowpass.hpp"
#include "alia/audio/hermite_resampler.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstring>
#include <limits>
#include <numbers>
#include <random>
#include <span>
#include <utility>
#include <vector>

namespace {
    using lowpass = alia::chebyshev2_lowpass;

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

    std::vector<float> sine(std::size_t frames, double frequency, int rate) {
        std::vector<float> points(frames);
        for (std::size_t i = 0; i < frames; ++i)
            points[i] = static_cast<float>(std::sin(2 * std::numbers::pi * frequency * static_cast<double>(i) / rate));
        return points;
    }

    // Level of one mono frequency relative to a unit sine, by correlation with
    // a complex exponential after skipping the filter's transient.
    double tone_db(std::span<const float> points, double frequency, int rate, std::size_t skip) {
        std::complex<double> sum;
        const auto step = -2 * std::numbers::pi * frequency / rate;
        for (std::size_t i = skip; i < points.size(); ++i)
            sum += static_cast<double>(points[i]) * std::polar(1.0, step * static_cast<double>(i));
        return 20 * std::log10(2 * std::abs(sum) / static_cast<double>(points.size() - skip));
    }

    std::vector<float> filter_mono(lowpass &effect, std::span<const float> input) {
        std::vector<float> output(input.size());
        EXPECT_EQ(effect.process(input, output), (alia::audio_effect_progress{input.size(), input.size()}));
        return output;
    }

    std::vector<float> downsample(std::span<const float> input, int input_rate, int output_rate, bool antialias) {
        std::vector<float> filtered(input.begin(), input.end());
        if (antialias) {
            auto filter = lowpass::for_downsampling(1, input_rate, output_rate);
            filtered = filter_mono(filter, input);
        }
        alia::hermite_resampler resampler(1, input_rate, output_rate);
        std::vector<float> output(resampler.output_frames_for_input(filtered.size()));
        EXPECT_EQ(resampler.process(filtered, output).produced, output.size());
        return output;
    }

    class Chebyshev2LowpassRatio : public ::testing::TestWithParam<std::pair<int, int>> {};
} // namespace

static_assert(!std::is_copy_constructible_v<alia::chebyshev2_lowpass>);
static_assert(std::is_nothrow_move_constructible_v<alia::chebyshev2_lowpass>);
static_assert(std::is_nothrow_move_assignable_v<alia::chebyshev2_lowpass>);

TEST(Chebyshev2Lowpass, RejectsInvalidParameters) {
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto inf = std::numeric_limits<double>::infinity();
    for (const int order : {-2, -1, 7, lowpass::max_order + 2})
        EXPECT_THROW((lowpass{2, 48000, {10000, 50, order}}), std::invalid_argument) << order;
    for (const double hz : {0.0, -1.0, 24000.0, 30000.0, nan, inf})
        EXPECT_THROW((lowpass{2, 48000, {hz}}), std::invalid_argument) << hz;
    for (const double db : {0.0, 0.5, -10.0, 201.0, nan, inf})
        EXPECT_THROW((lowpass{2, 48000, {10000, db}}), std::invalid_argument) << db;
    EXPECT_THROW((lowpass{0, 48000, {10000}}), std::invalid_argument);
    EXPECT_THROW((lowpass{2, 0, {10000}}), std::invalid_argument);
    EXPECT_THROW(lowpass::for_downsampling(2, 48000, 0), std::invalid_argument);
    EXPECT_THROW(lowpass::for_downsampling(2, 0, 8000), std::invalid_argument);
    EXPECT_THROW(lowpass::for_downsampling(0, 48000, 8000), std::invalid_argument);
    // Order 0 ignores the design parameters.
    EXPECT_EQ((lowpass{2, 48000, {0, 0, 0}}).order(), 0);
    EXPECT_EQ((lowpass{2, 48000, {23999, 200, lowpass::max_order}}).order(), lowpass::max_order);
}

TEST(Chebyshev2Lowpass, DownsamplingFactoryFiltersOnlyRatiosAboveOnePointTwo) {
    EXPECT_EQ(lowpass::for_downsampling(2, 44100, 48000).order(), 0);
    EXPECT_EQ(lowpass::for_downsampling(2, 48000, 48000).order(), 0);
    EXPECT_EQ(lowpass::for_downsampling(2, 48000, 44100).order(), 0);
    EXPECT_EQ(lowpass::for_downsampling(2, 48000, 40001).order(), 0);
    EXPECT_EQ(lowpass::for_downsampling(2, 48000, 39999).order(), 8);
    const auto effect = lowpass::for_downsampling(2, 96000, 44100);
    EXPECT_EQ(effect.order(), 8);
    const alia::audio_stream_format format{{alia::sample_point_formats::f32, 2}, 96000};
    EXPECT_EQ(effect.input_format(), format);
    EXPECT_EQ(effect.output_format(), format);
    EXPECT_EQ(effect.input_frames_for_output(17), 17);
    EXPECT_EQ(effect.output_frames_for_input(11), 11);
}

TEST(Chebyshev2Lowpass, OrderZeroCopiesEveryBit) {
    constexpr std::size_t frames = 1000;
    std::mt19937 random(512);
    auto input = random_points(frames * 2, random);
    input[0] = -0.0f;
    input[1] = std::bit_cast<float>(std::uint32_t{0x7fc01234});
    input[2] = (std::numeric_limits<float>::infinity)();
    input[3] = -(std::numeric_limits<float>::infinity)();
    input[4] = (std::numeric_limits<float>::denorm_min)();
    auto effect = lowpass::for_downsampling(2, 48000, 44100);
    ASSERT_EQ(effect.order(), 0);
    std::vector<float> output(input.size());
    EXPECT_EQ(effect.process(input, std::span(output).first(7 * 2)), (alia::audio_effect_progress{7, 7}));
    EXPECT_EQ(
        effect.process(std::span(input).subspan(7 * 2), std::span(output).subspan(7 * 2)),
        (alia::audio_effect_progress{frames - 7, frames - 7})
    );
    expect_identical(output, input);
}

TEST_P(Chebyshev2LowpassRatio, PassesDcAndPassbandAndRejectsTheStopband) {
    const auto [input_rate, output_rate] = GetParam();
    constexpr std::size_t frames = 24576, skip = 8192;
    {
        auto effect = lowpass::for_downsampling(1, input_rate, output_rate);
        ASSERT_EQ(effect.order(), 8);
        const auto output = filter_mono(effect, std::vector<float>(frames, 1.0f));
        EXPECT_NEAR(output.back(), 1.0f, 1e-3);
    }
    const auto response_db = [&](double frequency) {
        auto effect = lowpass::for_downsampling(1, input_rate, output_rate);
        return tone_db(filter_mono(effect, sine(frames, frequency, input_rate)), frequency, input_rate, skip);
    };
    EXPECT_NEAR(response_db(0.3 * output_rate), 0, 0.1);
    // The stopband is equiripple at -50 dB from the edge up to Nyquist.
    for (double fraction = 0.6; fraction * output_rate < 0.49 * input_rate; fraction *= 1.1)
        EXPECT_LT(response_db(fraction * output_rate), -49) << "at " << fraction << " * output rate";
    EXPECT_LT(response_db(0.49 * input_rate), -49);
}

TEST_P(Chebyshev2LowpassRatio, ChunkSizesDoNotChangeAnyOutputBits) {
    const auto [input_rate, output_rate] = GetParam();
    constexpr std::size_t channels = 2, frames = 4099;
    std::mt19937 random(613);
    // Decay into silence so state flushes happen inside the compared range.
    auto input = random_points(frames * channels, random);
    std::fill(input.begin() + 1024 * channels, input.end(), 0.0f);
    auto whole = lowpass::for_downsampling(channels, input_rate, output_rate);
    std::vector<float> expected(input.size());
    ASSERT_EQ(whole.process(input, expected), (alia::audio_effect_progress{frames, frames}));

    auto chunks = lowpass::for_downsampling(channels, input_rate, output_rate);
    std::vector<float> actual(input.size());
    for (std::size_t done = 0; done < frames;) {
        const auto input_count = (std::min)(frames - done, std::size_t{1} + random() % 300);
        const auto output_count = (std::min)(frames - done, std::size_t{1} + random() % 300);
        const auto progress = chunks.process(
            std::span(input).subspan(done * channels, input_count * channels),
            std::span(actual).subspan(done * channels, output_count * channels)
        );
        const auto count = (std::min)(input_count, output_count);
        ASSERT_EQ(progress, (alia::audio_effect_progress{count, count}));
        done += count;
    }
    expect_identical(actual, expected);
}

TEST_P(Chebyshev2LowpassRatio, ChannelsRemainIsolatedAndResetRestoresFreshState) {
    const auto [input_rate, output_rate] = GetParam();
    std::mt19937 random(331);
    auto input = random_points(1027 * 2, random);
    for (std::size_t i = 1; i < input.size(); i += 2)
        input[i] = 0;
    auto fresh = lowpass::for_downsampling(2, input_rate, output_rate);
    auto reset = lowpass::for_downsampling(2, input_rate, output_rate);
    // Cross a flush position so the flush phase is also reset.
    std::vector<float> prefix_output(300 * 2);
    reset.process(std::span(input).first(300 * 2), prefix_output);
    reset.reset();
    std::vector<float> expected(input.size()), actual(input.size());
    EXPECT_EQ(reset.process(input, actual), fresh.process(input, expected));
    expect_identical(actual, expected);
    for (std::size_t i = 1; i < actual.size(); i += 2)
        EXPECT_EQ(actual[i], 0.0f);
}

TEST_P(Chebyshev2LowpassRatio, MovingPreservesStateAndFlushPhase) {
    const auto [input_rate, output_rate] = GetParam();
    std::mt19937 random(443);
    const auto input = random_points(1027 * 2, random);
    auto reference = lowpass::for_downsampling(2, input_rate, output_rate);
    auto source = lowpass::for_downsampling(2, input_rate, output_rate);
    std::array<float, 100 * 2> a{}, b{};
    const auto prefix = reference.process(std::span(input).first(a.size()), a);
    ASSERT_EQ(source.process(std::span(input).first(b.size()), b), prefix);
    auto moved = std::move(source);
    lowpass assigned(1, 48000, {10000});
    assigned = std::move(moved);
    EXPECT_EQ(assigned.input_format(), reference.input_format());
    EXPECT_EQ(assigned.order(), reference.order());
    const auto rest = std::span(input).subspan(a.size());
    std::vector<float> actual(rest.size()), expected(rest.size());
    EXPECT_EQ(assigned.process(rest, actual), reference.process(rest, expected));
    expect_identical(actual, expected);
}

INSTANTIATE_TEST_SUITE_P(
    Rates,
    Chebyshev2LowpassRatio,
    ::testing::Values(
        std::pair{96000, 44100},
        std::pair{192000, 8000},
        std::pair{48000, 22050},
        std::pair{44100, 32000},
        std::pair{48000, 8000}
    )
);

TEST(Chebyshev2Lowpass, SilenceFlushesToExactZeroWithoutSubnormals) {
    // Slow poles (stopband edge at 0.025 * fs) would otherwise spend thousands of
    // frames decaying through the subnormal range.
    constexpr std::size_t loud = 1024, silent = 16384;
    std::mt19937 random(97);
    auto input = random_points(loud + silent, random);
    std::fill(input.begin() + loud, input.end(), 0.0f);
    auto effect = lowpass::for_downsampling(1, 192000, 8000);
    const auto output = filter_mono(effect, input);
    for (std::size_t i = 0; i < output.size(); ++i)
        ASSERT_NE(std::fpclassify(output[i]), FP_SUBNORMAL) << "frame " << i;
    for (std::size_t i = output.size() - 4096; i < output.size(); ++i)
        ASSERT_EQ(output[i], 0.0f) << "frame " << i;
}

TEST(Chebyshev2Lowpass, PreventsHermiteDownsamplingAliases) {
    constexpr int input_rate = 96000, output_rate = 44100;
    constexpr std::size_t frames = 65536, skip = 4096;
    // A tone at 0.8 * output_rate folds to 0.2 * output_rate.
    const auto tone = sine(frames, 0.8 * output_rate, input_rate);
    const auto alias = 0.2 * output_rate;
    EXPECT_GT(tone_db(downsample(tone, input_rate, output_rate, false), alias, output_rate, skip), -20);
    EXPECT_LT(tone_db(downsample(tone, input_rate, output_rate, true), alias, output_rate, skip), -45);
    const auto passband = 0.3 * output_rate;
    const auto kept = downsample(sine(frames, passband, input_rate), input_rate, output_rate, true);
    EXPECT_NEAR(tone_db(kept, passband, output_rate, skip), 0, 0.5);
}
