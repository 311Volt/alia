#include "alia/audio/audio.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <numbers>
#include <thread>
#include <vector>

namespace {
    constexpr alia::audio_stream_format native_format{{alia::sample_point_formats::f32, 2}, 48000};
    struct fake_context;
    struct fake_stream : alia::audio_stream_handle {
        fake_context *context = nullptr;
        alia::audio_stream_properties properties;
        alia::audio_render_fn render = nullptr;
        void *user = nullptr;
        std::atomic<alia::audio_stream_state> state{alia::audio_stream_state::stopped};
        std::int64_t position = 0;
        std::mutex callback_mutex; // Implements the backend's synchronous stop contract.
    };
    struct fake_context : alia::audio_context_handle {
        fake_stream *last = nullptr;
        int max_callback_frames = 1024;
        int required_channels = 0; // Zero accepts any float bus without fallback.
        bool fail_start = false;
    };
    alia::created_audio_context create_fake(const alia::audio_context_config &) {
        auto context = std::make_unique<fake_context>();
        alia::audio_backend_interface ops;
        ops.id = alia::audio_backend::null;
        ops.caps = {true, false, 1, 1024, "mixer manually pumped fake"};
        ops.destroy_context.operation = [](alia::audio_context_handle *p) { delete static_cast<fake_context *>(p); };
        ops.enumerate_devices.operation = [](alia::audio_context_handle *, alia::audio_direction direction) {
            return std::vector<alia::audio_device_info>{{"fake", "fake output", direction, true, native_format}};
        };
        ops.stream_format_supported.operation = [](alia::audio_context_handle *p, alia::audio_direction,
            std::string_view, const alia::audio_stream_format &format) {
            auto &context = *static_cast<fake_context *>(p);
            return format.sample_rate == 48000 && format.frame.point == alia::sample_point_formats::f32 &&
                   (!context.required_channels || format.frame.channels == context.required_channels);
        };
        ops.create_output_stream.operation = [](alia::audio_context_handle *p, const alia::audio_stream_desc &desc,
            alia::audio_render_fn render) -> alia::audio_stream_handle * {
            auto &context = *static_cast<fake_context *>(p);
            auto stream = std::make_unique<fake_stream>();
            stream->context = &context;
            stream->properties = {desc.format, 64, context.max_callback_frames, 64.0 / 48000};
            stream->render = render;
            stream->user = desc.user;
            context.last = stream.get();
            return stream.release();
        };
        ops.destroy_stream.operation = [](alia::audio_stream_handle *p) {
            auto *stream = static_cast<fake_stream *>(p);
            if (stream->context->last == stream)
                stream->context->last = nullptr;
            delete stream;
        };
        ops.stream_properties.operation = [](const alia::audio_stream_handle *p) {
            return static_cast<const fake_stream *>(p)->properties;
        };
        ops.stream_start.operation = [](alia::audio_stream_handle *p) {
            auto &stream = *static_cast<fake_stream *>(p);
            std::lock_guard lock(stream.callback_mutex);
            if (stream.state.load() == alia::audio_stream_state::failed || stream.context->fail_start)
                return false;
            stream.state.store(alia::audio_stream_state::running);
            return true;
        };
        ops.stream_stop.operation = [](alia::audio_stream_handle *p) {
            auto &stream = *static_cast<fake_stream *>(p);
            std::lock_guard lock(stream.callback_mutex);
            if (stream.state.load() != alia::audio_stream_state::failed)
                stream.state.store(alia::audio_stream_state::stopped);
        };
        ops.stream_state.operation = [](const alia::audio_stream_handle *p) {
            return static_cast<const fake_stream *>(p)->state.load();
        };
        ops.stream_position.operation = [](const alia::audio_stream_handle *p) {
            return static_cast<const fake_stream *>(p)->position;
        };
        return {context.release(), std::move(ops)};
    }

    template <class Predicate>
    bool wait_until(Predicate predicate) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!predicate()) {
            if (std::chrono::steady_clock::now() >= deadline)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }
    struct decoder_probe {
        std::atomic<bool> fail{false}, hold{false}, waiting{false}, destroyed{false};
    };
    using pcm_frame = std::array<std::int16_t, 2>;
    class memory_decoder final : public alia::audio_decoder {
    public:
        memory_decoder(std::vector<pcm_frame> frames, int rate, std::shared_ptr<decoder_probe> probe, bool known)
            : frames_(std::move(frames)), rate_(rate), probe_(std::move(probe)), known_(known) {}
        ~memory_decoder() override { probe_->destroyed.store(true); }
        alia::audio_stream_format format() const override {
            return {alia::sample_frame_format_of_v<pcm_frame>, rate_};
        }
        std::optional<std::size_t> frame_count() const override {
            return known_ ? std::optional<std::size_t>(frames_.size()) : std::nullopt;
        }
        std::size_t read(std::span<std::byte> output) override {
            if (probe_->hold.load()) {
                probe_->waiting.store(true);
                while (probe_->hold.load())
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (probe_->fail.load())
                throw std::runtime_error("mixer test decoder failure");
            const auto count = (std::min)(output.size() / sizeof(pcm_frame), frames_.size() - position_);
            if (count)
                std::memcpy(output.data(), frames_.data() + position_, count * sizeof(pcm_frame));
            position_ += count;
            return count;
        }
        void seek(std::size_t frame) override {
            if (frame > frames_.size())
                throw std::out_of_range("mixer test seek");
            position_ = frame;
        }
    private:
        std::vector<pcm_frame> frames_;
        int rate_;
        std::shared_ptr<decoder_probe> probe_;
        bool known_;
        std::size_t position_ = 0;
    };

    alia::audio_clip clip_from(std::span<const float> points, int channels = 2, int rate = 48000) {
        return alia::audio_clip({alia::sample_point_formats::f32, channels}, rate, std::as_bytes(points));
    }
    std::vector<float> signal(std::size_t frames, int channels = 2) {
        std::vector<float> points(frames * channels);
        for (std::size_t i = 0; i < points.size(); ++i)
            points[i] = static_cast<float>(static_cast<int>((i * 73 + 19) % 199) - 99) / 128;
        return points;
    }
    // Independent, whole-clip effect chain. EOF padding is filtered, too.
    std::vector<float> offline(std::span<const float> points, int channels, int input_rate, int output_rate = 48000) {
        if (input_rate == output_rate)
            return {points.begin(), points.end()};
        const auto length = points.size() / channels;
        std::vector<float> input(points.begin(), points.end());
        input.resize((length + alia::hermite_resampler::lookahead_frames) * channels, 0);
        auto lowpass = alia::chebyshev2_lowpass::for_downsampling(channels, input_rate, output_rate);
        std::vector<float> filtered(input.size());
        lowpass.process(std::span<const float>(input), std::span<float>(filtered));
        alia::hermite_resampler resampler(channels, input_rate, output_rate);
        const auto count = (length * output_rate + input_rate - 1) / input_rate;
        std::vector<float> output(count * channels);
        const auto progress = resampler.process(std::span<const float>(filtered), std::span<float>(output));
        EXPECT_EQ(progress.produced, count);
        return output;
    }
    void expect_identical(std::span<const float> actual, std::span<const float> expected) {
        ASSERT_EQ(actual.size(), expected.size());
        if (!actual.empty())
            EXPECT_EQ(std::memcmp(actual.data(), expected.data(), actual.size_bytes()), 0);
    }
    void expect_silent(std::span<const float> points) {
        for (const auto point : points)
            EXPECT_EQ(point, 0.0f);
    }

    class AudioMixer : public ::testing::Test {
    protected:
        alia::audio_context context;
        fake_context &fake() { return *static_cast<fake_context *>(context.handle()); }
        void SetUp() override {
            alia::register_audio_backend({alia::audio_backend::null, create_fake});
            context = alia::audio_context::create(alia::audio_backend::null);
        }
        // A stopped backend invokes no callback, and consequently advances no source.
        std::vector<float> pump(std::size_t frames) {
            auto &stream = *fake().last;
            std::lock_guard lock(stream.callback_mutex);
            EXPECT_LE(frames, stream.properties.max_callback_frames);
            const auto channels = stream.properties.format.frame.channels;
            std::vector<float> points(frames * channels, 0);
            if (stream.state.load() == alia::audio_stream_state::running) {
                stream.render(stream.user, reinterpret_cast<std::byte *>(points.data()), static_cast<int>(frames),
                              {stream.position, alia::audio_callback_flags::none});
                stream.position += frames;
            }
            return points;
        }
        std::vector<float> capture(std::size_t frames, std::size_t block) {
            std::vector<float> points;
            while (frames) {
                const auto count = (std::min)(frames, block);
                auto part = pump(count);
                points.insert(points.end(), part.begin(), part.end());
                frames -= count;
            }
            return points;
        }
        alia::audio_reader reader(std::vector<pcm_frame> frames, int rate,
                                  const std::shared_ptr<decoder_probe> &probe, bool known = true) {
            return alia::audio_reader(std::make_unique<memory_decoder>(std::move(frames), rate, probe, known));
        }
    };

    TEST_F(AudioMixer, DefaultAndConfigurationValidation) {
        alia::audio_mixer empty;
        EXPECT_EQ(empty.state(), alia::audio_stream_state::stopped);
        EXPECT_EQ(empty.sample_rate(), 0);
        EXPECT_EQ(empty.channels(), 0);
        EXPECT_EQ(empty.gain(), 1);
        EXPECT_THROW(empty.start(), std::logic_error);
        EXPECT_THROW(empty.stop(), std::logic_error);
        EXPECT_THROW(empty.set_gain(1), std::logic_error);
        EXPECT_THROW(empty.play_one_shot({}), std::logic_error);
        EXPECT_THROW(empty.stop_one_shots(), std::logic_error);
        EXPECT_THROW((alia::audio_mixer(context, {.sample_rate = -1})), std::invalid_argument);
        EXPECT_THROW((alia::audio_mixer(context, {.channels = 0})), std::invalid_argument);
        EXPECT_THROW((alia::audio_mixer(context, {.period_frames = -1})), std::invalid_argument);
        EXPECT_THROW((alia::audio_mixer(context, {.gain = std::numeric_limits<float>::infinity()})), std::invalid_argument);
        EXPECT_THROW((alia::audio_mixer(context, {.max_players = (std::numeric_limits<std::size_t>::max)(),
                                                .one_shot_voices = 1})), std::length_error);
    }
    TEST_F(AudioMixer, StartsStoppedAndStopFreezesPlayers) {
        const auto clip = clip_from(signal(1024));
        alia::audio_mixer mixer(context);
        alia::audio_player player(mixer, clip.view());
        EXPECT_EQ(mixer.state(), alia::audio_stream_state::stopped);
        EXPECT_EQ(mixer.sample_rate(), 48000);
        EXPECT_EQ(mixer.channels(), 2);
        player.play();
        expect_silent(pump(16));
        EXPECT_EQ(player.position_frames(), 0);
        mixer.start();
        pump(16);
        EXPECT_EQ(player.position_frames(), 16);
        mixer.stop();
        expect_silent(pump(16));
        EXPECT_EQ(player.position_frames(), 16);
        mixer.start();
        pump(16);
        EXPECT_EQ(player.position_frames(), 32);
        EXPECT_THROW(mixer.set_gain(std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
    }
    TEST_F(AudioMixer, IdentityMatchesSourceAndStandaloneBitsIncludingSignedZero) {
        auto points = signal(1200);
        points[0] = -0.0f;
        const auto clip = clip_from(points);
        std::vector<float> standalone;
        {
            alia::audio_player player(context, clip.view());
            player.play();
            standalone = capture(1200, 300);
        }
        alia::audio_mixer mixer(context);
        alia::audio_player player(mixer, clip.view());
        mixer.start();
        player.play();
        const auto output = capture(1200, 300);
        expect_identical(output, points);
        expect_identical(output, standalone);
        expect_silent(pump(1024));
    }
    TEST_F(AudioMixer, MappingAndUnsupportedLayouts) {
        struct mapping_case { int source, bus; std::vector<float> input, expected; };
        constexpr float k = std::numbers::sqrt2_v<float> / 2;
        for (const auto &c : std::vector<mapping_case>{
                 {1, 2, {0.25f}, {0.25f, 0.25f}},
                 {2, 1, {0.5f, -0.25f}, {0.125f}},
                 {6, 2, {1, 2, 3, 100, 4, 5}, {1 + k * 3 + k * 4, 2 + k * 3 + k * 5}},
                 {7, 2, {1, 2, 3, 100, 4, 5, 6}, {1 + k * 3 + k * 4 + 3, 2 + k * 3 + k * 5 + 3}},
                 {8, 2, {1, 2, 3, 100, 4, 5, 6, 7}, {1 + k * 3 + k * 4 + k * 6, 2 + k * 3 + k * 5 + k * 7}},
                 {2, 4, {0.5f, -0.25f}, {0.5f, -0.25f, 0, 0}},
                 {1, 6, {0.5f}, {0.5f, 0.5f, 0, 0, 0, 0}}
             }) {
            SCOPED_TRACE(::testing::Message() << c.source << " -> " << c.bus);
            const auto clip = clip_from(c.input, c.source);
            alia::audio_mixer mixer(context, {.channels = c.bus});
            alia::audio_player player(mixer, clip.view());
            mixer.start();
            player.play();
            EXPECT_EQ(pump(1), c.expected);
            ASSERT_TRUE(mixer.play_one_shot(clip.view()));
            EXPECT_EQ(pump(1), c.expected);
        }
        const auto four = clip_from(signal(2, 4), 4);
        const auto nine = clip_from(signal(2, 9), 9);
        alia::audio_mixer mixer(context, {.channels = 6});
        EXPECT_THROW((alia::audio_player(mixer, four.view())), std::invalid_argument);
        EXPECT_THROW(mixer.play_one_shot(four.view()), std::invalid_argument);
        EXPECT_THROW((alia::audio_player(mixer, nine.view())), std::invalid_argument);
        EXPECT_THROW(mixer.play_one_shot(nine.view()), std::invalid_argument);
    }
    TEST_F(AudioMixer, BusRemainsRequestedFormatWhenStreamFallbackConverts) {
        fake().required_channels = 1;
        const auto clip = clip_from(std::array{1.0f, -0.5f});
        alia::audio_mixer mixer(context);
        alia::audio_player player(mixer, clip.view());
        EXPECT_EQ(mixer.channels(), 2);
        mixer.start();
        player.play();
        EXPECT_EQ(pump(1), (std::vector<float>{0.25f}));
    }
    TEST_F(AudioMixer, ResamplingMatchesOfflineForEveryCallbackSizeAndFlushLength) {
        for (const int rate : {44100, 96000})
            for (const std::size_t length : {0u, 1u, 2u, 511u, 512u, 1301u})
                for (const std::size_t block : {1u, 3u, 256u, 300u, 1024u}) {
                    SCOPED_TRACE(::testing::Message() << rate << ", length " << length << ", pump " << block);
                    const auto points = signal(length);
                    const auto expected = offline(points, 2, rate);
                    const auto clip = clip_from(points, 2, rate);
                    alia::audio_mixer mixer(context);
                    alia::audio_player player(mixer, clip.view());
                    mixer.start();
                    player.play();
                    const auto output = capture(expected.size() / 2 + 16, block);
                    expect_identical(std::span(output).first(expected.size()), expected);
                    expect_silent(std::span(output).subspan(expected.size()));
                    EXPECT_EQ(player.state(), alia::audio_player_state::stopped);
                    // Same processor and flushing contract for pool voices.
                    ASSERT_TRUE(mixer.play_one_shot(clip.view()));
                    const auto shot = capture(expected.size() / 2 + 16, block);
                    expect_identical(std::span(shot).first(expected.size()), expected);
                    expect_silent(std::span(shot).subspan(expected.size()));
                }
    }
    TEST_F(AudioMixer, ExactPanEndpointsAndCentre) {
        for (const int channels : {1, 2}) {
            const auto clip = clip_from(std::vector<float>(channels, 1), channels);
            alia::audio_mixer mixer(context);
            alia::audio_player player(mixer, clip.view());
            mixer.start();
            for (const float pan : {-1.0f, 0.0f, 1.0f}) {
                player.set_pan(pan);
                EXPECT_EQ(player.pan(), pan);
                player.restart();
                const auto output = pump(1);
                const auto near_gain = channels == 1 ? std::numbers::sqrt2_v<float> : 1;
                EXPECT_EQ(output[0], pan == 1 ? 0 : pan == -1 ? near_gain : 1);
                EXPECT_EQ(output[1], pan == -1 ? 0 : pan == 1 ? near_gain : 1);
                ASSERT_TRUE(mixer.play_one_shot(clip.view(), {.pan = pan}));
                EXPECT_EQ(pump(1), output);
            }
            EXPECT_THROW(player.set_pan(1.01f), std::invalid_argument);
            EXPECT_THROW(player.set_pan(std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
            EXPECT_THROW((alia::audio_player(mixer, clip.view(), {.pan = -2})), std::invalid_argument);
        }
        const auto clip = clip_from(std::array{0.25f}, 1);
        alia::audio_mixer mono(context, {.channels = 1});
        alia::audio_player player(mono, clip.view(), {.pan = 1});
        mono.start();
        player.play();
        EXPECT_EQ(pump(1), (std::vector<float>{0.25f}));
    }
    TEST_F(AudioMixer, TwoPlayersAndMasterGainSumInSlotOrder) {
        const auto a = clip_from(std::array{0.75f, -0.25f});
        const auto b = clip_from(std::array{0.5f, 0.125f});
        alia::audio_mixer mixer(context, {.gain = 2});
        alia::audio_player first(mixer, a.view());
        alia::audio_player second(mixer, b.view(), {.gain = 0.5f});
        mixer.start();
        first.play();
        second.play();
        EXPECT_EQ(pump(1), (std::vector<float>{2, -0.375f}));
        mixer.set_gain(-1);
        EXPECT_EQ(mixer.gain(), -1);
        first.restart();
        second.restart();
        EXPECT_EQ(pump(1), (std::vector<float>{-1, 0.1875f}));
    }
    TEST_F(AudioMixer, AccumulationOrderIsAttachmentSlotOrder) {
        const auto positive = clip_from(std::array{1.0e20f, 1.0e20f});
        const auto negative = clip_from(std::array{-1.0e20f, -1.0e20f});
        const auto small = clip_from(std::array{3.0f, 4.0f});
        alia::audio_mixer mixer(context);
        alia::audio_player first(mixer, positive.view());
        alia::audio_player second(mixer, negative.view());
        alia::audio_player third(mixer, small.view());
        mixer.start();
        // Playback order does not change slot order or rounding.
        third.play();
        second.play();
        first.play();
        EXPECT_EQ(pump(1), (std::vector<float>{3, 4}));
    }
    TEST_F(AudioMixer, PauseResumePreservesResamplerWhileOtherVoicesContinue) {
        const auto points = signal(1300);
        const auto expected = offline(points, 2, 44100);
        const auto clip = clip_from(points, 2, 44100);
        const auto constant = clip_from(std::array{0.125f, -0.125f});
        alia::audio_mixer mixer(context);
        alia::audio_player player(mixer, clip.view());
        alia::audio_player other(mixer, constant.view(), {.mode = alia::play_mode::loop});
        mixer.start();
        player.play();
        auto prefix = pump(3);
        expect_identical(prefix, std::span(expected).first(6));
        player.pause();
        const auto position = player.position_frames();
        other.play();
        const auto paused = pump(300);
        for (std::size_t i = 0; i < paused.size(); i += 2) {
            EXPECT_EQ(paused[i], 0.125f);
            EXPECT_EQ(paused[i + 1], -0.125f);
        }
        EXPECT_EQ(player.position_frames(), position);
        other.pause();
        player.play();
        expect_identical(pump(300), std::span(expected).subspan(6, 600));
        player.stop();
        EXPECT_EQ(player.position_frames(), 0);
        expect_silent(pump(3));
        player.play();
        expect_identical(pump(3), prefix);
        player.restart();
        expect_identical(pump(3), prefix);
    }
    TEST_F(AudioMixer, ClipSeekDiscardsLookaheadAndReplayStartsFresh) {
        for (const int rate : {44100, 96000}) {
            const auto points = signal(1201);
            const auto clip = clip_from(points, 2, rate);
            const auto expected = offline(std::span(points).subspan(74), 2, rate);
            alia::audio_mixer mixer(context);
            alia::audio_player player(mixer, clip.view());
            mixer.start();
            player.play();
            pump(1); // Upsampling can have buffered output requiring no input.
            player.seek(37);
            const auto output = capture(expected.size() / 2 + 16, 1);
            expect_identical(std::span(output).first(expected.size()), expected);
            expect_silent(std::span(output).subspan(expected.size()));
            EXPECT_EQ(player.state(), alia::audio_player_state::stopped);
            player.play();
            const auto fresh = offline(points, 2, rate);
            expect_identical(pump(3), std::span(fresh).first(6));
            EXPECT_THROW(player.seek(1202), std::out_of_range);
        }
    }
    TEST_F(AudioMixer, LoopCrossingsDoNotResetTheEffects) {
        for (const int rate : {48000, 44100, 96000}) {
            const auto points = signal(7);
            std::vector<float> repeated;
            for (int i = 0; i < 600; ++i)
                repeated.insert(repeated.end(), points.begin(), points.end());
            const auto expected = offline(repeated, 2, rate);
            const auto clip = clip_from(points, 2, rate);
            alia::audio_mixer mixer(context);
            alia::audio_player player(mixer, clip.view(), {.mode = alia::play_mode::loop});
            mixer.start();
            player.play();
            expect_identical(capture(1500, 300), std::span(expected).first(3000));
            EXPECT_EQ(player.state(), alia::audio_player_state::playing);
        }
    }
    TEST_F(AudioMixer, AttachedConfigurationMustUseMixerDefaults) {
        const auto clip = clip_from(signal(1));
        alia::audio_mixer mixer(context);
        EXPECT_THROW((alia::audio_player(mixer, clip.view(), {.device = context.default_output_device()})), std::invalid_argument);
        EXPECT_THROW((alia::audio_player(mixer, clip.view(), {.sample_rate = 48000})), std::invalid_argument);
        EXPECT_THROW((alia::audio_player(mixer, clip.view(), {.period_frames = 16})), std::invalid_argument);
        alia::audio_player standalone(context, clip.view(), {.pan = 1});
        EXPECT_EQ(standalone.pan(), 1);
    }
    TEST_F(AudioMixer, StreamedConversionResamplingSeekAndPausePrefill) {
        for (const int rate : {44100, 96000}) {
            const std::array<pcm_frame, 4> pattern{{{16384, -16384}, {8192, -8192}, {-16384, 16384}, {24576, -24576}}};
            std::vector<pcm_frame> pcm;
            for (int i = 0; i < 16; ++i)
                pcm.insert(pcm.end(), pattern.begin(), pattern.end());
            std::vector<float> converted;
            for (const auto &frame : pcm)
                for (const auto point : frame)
                    converted.push_back(alia::convert_sample_point<float>(point));
            const auto expected = offline(converted, 2, rate);
            auto probe = std::make_shared<decoder_probe>();
            alia::audio_mixer mixer(context);
            alia::audio_player player(mixer, reader(pcm, rate, probe), {.buffer_frames = 128});
            mixer.start();
            player.play();
            const auto output = capture(expected.size() / 2 + 4, 1);
            expect_identical(std::span(output).first(expected.size()), expected);
            expect_silent(std::span(output).subspan(expected.size()));
            player.play();
            player.seek(1);
            const auto after_seek = offline(std::span(converted).subspan(2), 2, rate);
            expect_identical(pump(1), std::span(after_seek).first(2));
            player.pause();
            const auto position = player.position_frames();
            expect_silent(pump(3));
            EXPECT_EQ(player.position_frames(), position);
            player.play();
            const auto rest = capture(after_seek.size() / 2 - 1, 1);
            expect_identical(rest, std::span(after_seek).subspan(2));
            player.stop();
            EXPECT_EQ(player.position_frames(), 0);
        }
    }
    TEST_F(AudioMixer, StreamedUnderflowAndDecoderFailure) {
        auto probe = std::make_shared<decoder_probe>();
        alia::audio_mixer mixer(context);
        alia::audio_player player(mixer, reader(std::vector<pcm_frame>(1000, {16384, -16384}), 48000, probe),
                                  {.buffer_frames = 8});
        struct release_guard {
            std::shared_ptr<decoder_probe> probe;
            ~release_guard() { probe->hold.store(false); }
        } guard{probe};
        mixer.start();
        player.play();
        probe->hold.store(true);
        pump(8);
        ASSERT_TRUE(wait_until([&] { return probe->waiting.load(); }));
        expect_silent(pump(16));
        EXPECT_EQ(player.position_frames(), 8);
        EXPECT_EQ(player.state(), alia::audio_player_state::playing);
        probe->hold.store(false);
        player.pause();
        player.play();
        EXPECT_EQ(pump(1), (std::vector<float>{0.5f, -0.5f}));
        probe->fail.store(true);
        pump(8);
        ASSERT_TRUE(wait_until([&] { return player.state() == alia::audio_player_state::failed; }));
        expect_silent(pump(16));
        EXPECT_THROW(player.play(), std::runtime_error);
        EXPECT_EQ(mixer.state(), alia::audio_stream_state::running);
    }
    TEST_F(AudioMixer, UnknownLengthRejectedSeekPreservesResampler) {
        auto probe = std::make_shared<decoder_probe>();
        std::vector<float> constant(2000);
        for (std::size_t i = 0; i < constant.size(); i += 2) {
            constant[i] = 0.5f;
            constant[i + 1] = -0.5f;
        }
        const auto expected = offline(constant, 2, 44100);
        alia::audio_mixer mixer(context);
        alia::audio_player player(mixer, reader(std::vector<pcm_frame>(1000, {16384, -16384}), 44100, probe, false),
                                  {.buffer_frames = 16});
        mixer.start();
        player.play();
        expect_identical(pump(1), std::span(expected).first(2));
        EXPECT_THROW(player.seek(1001), std::out_of_range);
        EXPECT_EQ(player.state(), alia::audio_player_state::playing);
        expect_identical(pump(1), std::span(expected).subspan(2, 2));
    }
    TEST_F(AudioMixer, OutputFailurePropagatesToMixerPlayersAndOneShots) {
        const auto clip = clip_from(signal(8));
        alia::audio_mixer mixer(context);
        alia::audio_player player(mixer, clip.view());
        mixer.start();
        player.play();
        fake().last->state.store(alia::audio_stream_state::failed);
        EXPECT_EQ(mixer.state(), alia::audio_stream_state::failed);
        EXPECT_EQ(player.state(), alia::audio_player_state::failed);
        EXPECT_THROW(player.play(), std::runtime_error);
        EXPECT_THROW(mixer.start(), std::runtime_error);
        EXPECT_THROW(mixer.play_one_shot(clip.view()), std::runtime_error);
        mixer.stop_one_shots();
        player.pause();
    }
    TEST_F(AudioMixer, FailedStartAndCallbackFailureRemainLatched) {
        {
            alia::audio_mixer mixer(context);
            fake().fail_start = true;
            EXPECT_THROW(mixer.start(), std::runtime_error);
            fake().fail_start = false;
            EXPECT_EQ(mixer.state(), alia::audio_stream_state::failed);
            EXPECT_THROW(mixer.start(), std::runtime_error);
        }
        alia::audio_mixer mixer(context);
        const auto clip = clip_from(signal(3));
        alia::audio_player player(mixer, clip.view());
        mixer.start();
        auto &stream = *fake().last;
        std::byte unused{};
        stream.render(stream.user, &unused, stream.properties.max_callback_frames + 1,
                      {0, alia::audio_callback_flags::none});
        EXPECT_EQ(mixer.state(), alia::audio_stream_state::failed);
        EXPECT_EQ(player.state(), alia::audio_player_state::failed);
        EXPECT_THROW(mixer.start(), std::runtime_error);
    }
    TEST_F(AudioMixer, OneShotsOverlapExhaustReuseAndStopSynchronously) {
        const auto clip = clip_from(std::array{0.5f, -0.25f});
        alia::audio_mixer mixer(context, {.one_shot_voices = 2});
        alia::audio_player player(mixer, clip.view(), {.mode = alia::play_mode::loop});
        mixer.start();
        ASSERT_TRUE(mixer.play_one_shot(clip.view()));
        ASSERT_TRUE(mixer.play_one_shot(clip.view(), {.gain = 2}));
        EXPECT_FALSE(mixer.play_one_shot(clip.view()));
        EXPECT_EQ(pump(1), (std::vector<float>{1.5f, -0.75f}));
        ASSERT_TRUE(mixer.play_one_shot(clip.view()));
        player.play();
        mixer.stop_one_shots();
        EXPECT_EQ(pump(1), (std::vector<float>{0.5f, -0.25f}));
        player.pause();
        expect_silent(pump(1));
        ASSERT_TRUE(mixer.play_one_shot(clip.view()));
        EXPECT_EQ(pump(1), (std::vector<float>{0.5f, -0.25f}));
    }
    TEST_F(AudioMixer, OneShotValidationAndEmptyClipsUseNoVoice) {
        const auto clip = clip_from(signal(4));
        const auto empty = clip_from({}, 2);
        alia::audio_mixer mixer(context, {.one_shot_voices = 0});
        EXPECT_TRUE(mixer.play_one_shot(empty.view()));
        EXPECT_FALSE(mixer.play_one_shot(clip.view()));
        EXPECT_THROW(mixer.play_one_shot(clip.view(), {.gain = std::numeric_limits<float>::infinity()}), std::invalid_argument);
        EXPECT_THROW(mixer.play_one_shot(clip.view(), {.pan = -1.01f}), std::invalid_argument);
        EXPECT_THROW(mixer.play_one_shot(empty.view(), {.pan = std::numeric_limits<float>::quiet_NaN()}), std::invalid_argument);
        alia::const_audio_clip_view invalid({{alia::sample_encoding::floating_point, 3}, 2}, 48000, 0, nullptr);
        EXPECT_THROW(mixer.play_one_shot(invalid), std::invalid_argument);
    }
    TEST_F(AudioMixer, OneShotReprovisioningHandlesRateAndChannelChanges) {
        const auto stereo = clip_from(signal(17), 2, 44100);
        const auto mono = clip_from(signal(17, 1), 1, 96000);
        alia::audio_mixer mixer(context, {.one_shot_voices = 1});
        mixer.start();
        for (const auto clip : {stereo.view(), mono.view(), stereo.view()}) {
            ASSERT_TRUE(mixer.play_one_shot(clip));
            pump(1024);
            ASSERT_TRUE(mixer.play_one_shot(clip));
            mixer.stop_one_shots();
            expect_silent(pump(3));
        }
    }
    TEST_F(AudioMixer, OneShotsConvertNativePcmBeforeGainAndPan) {
        const std::array<pcm_frame, 2> frames{{{16384, -16384}, {8192, -8192}}};
        const alia::audio_clip clip(48000, std::span(frames));
        alia::audio_mixer mixer(context);
        mixer.start();
        ASSERT_TRUE(mixer.play_one_shot(clip.view(), {.gain = 0.5f, .pan = 1}));
        EXPECT_EQ(pump(3), (std::vector<float>{0, -0.25f, 0, -0.125f, 0, 0}));
    }
    TEST_F(AudioMixer, PlayerSlotExhaustionReuseAndMovesKeepCallbackAddress) {
        const auto clip = clip_from(signal(4));
        alia::audio_mixer original(context, {.max_players = 1});
        auto player = std::make_unique<alia::audio_player>(original, clip.view());
        EXPECT_THROW((alia::audio_player(original, clip.view())), std::runtime_error);
        const auto *address = fake().last->user;
        alia::audio_mixer moved(std::move(original));
        EXPECT_EQ(original.sample_rate(), 0);
        EXPECT_EQ(fake().last->user, address);
        alia::audio_mixer assigned;
        assigned = std::move(moved);
        EXPECT_EQ(fake().last->user, address);
        assigned.start();
        player->play();
        alia::audio_player moved_player(std::move(*player));
        player.reset();
        expect_identical(pump(4), signal(4));
        // Detach before the mixer even though its wrapper moved twice.
        moved_player = alia::audio_player{};
        alia::audio_player reused(assigned, clip.view());
        reused.play();
        expect_identical(pump(4), signal(4));
    }
    TEST_F(AudioMixer, UnalignedBackendOutputIsWrittenAsBytes) {
        const auto points = signal(600);
        const auto clip = clip_from(points);
        alia::audio_mixer mixer(context);
        alia::audio_player player(mixer, clip.view());
        mixer.start();
        player.play();
        std::vector<std::byte> bytes(points.size() * sizeof(float) + 1);
        auto &stream = *fake().last;
        stream.render(stream.user, bytes.data() + 1, 600, {0, alia::audio_callback_flags::none});
        EXPECT_EQ(std::memcmp(bytes.data() + 1, points.data(), points.size() * sizeof(float)), 0);
    }
    TEST_F(AudioMixer, ConcurrentPumpWithTransportAttachDetachAndOneShots) {
        const auto clip = clip_from(signal(600), 2, 44100);
        alia::audio_mixer mixer(context, {.max_players = 2, .one_shot_voices = 4});
        mixer.start();
        auto *stream = fake().last;
        std::atomic<std::size_t> callbacks{0};
        // Declared last so the pump joins before mixer/clip storage is destroyed.
        std::jthread pumping([&](std::stop_token token) {
            std::array<float, 600> output;
            while (!token.stop_requested()) {
                {
                    std::lock_guard lock(stream->callback_mutex);
                    stream->render(stream->user, reinterpret_cast<std::byte *>(output.data()), 300,
                                   {0, alia::audio_callback_flags::none});
                    callbacks.fetch_add(1);
                }
                std::this_thread::yield();
            }
        });
        ASSERT_TRUE(wait_until([&] { return callbacks.load() != 0; }));
        for (int i = 0; i < 200; ++i) {
            alia::audio_player player(mixer, clip.view(), {.mode = alia::play_mode::loop});
            player.play();
            player.set_pan(static_cast<float>(i % 3 - 1));
            player.seek(i % 500);
            mixer.play_one_shot(clip.view(), {.pan = player.pan()});
            player.pause();
            player.play();
            player.restart();
            player.stop();
            mixer.stop_one_shots();
        }
        pumping.request_stop();
        pumping.join();
        EXPECT_GT(callbacks.load(), 0);
        EXPECT_EQ(mixer.state(), alia::audio_stream_state::running);
    }
} // namespace
