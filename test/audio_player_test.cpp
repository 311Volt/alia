#include "alia/audio/audio.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <thread>

namespace {
    constexpr alia::audio_stream_format native_format{alia::sample_frame_format_of_v<alia::vec2f>, 48000};
    struct fake_context;
    struct fake_stream : alia::audio_stream_handle {
        fake_context *context = nullptr;
        alia::audio_stream_properties properties;
        alia::audio_render_fn render = nullptr;
        void *user = nullptr;
        alia::audio_stream_state state = alia::audio_stream_state::stopped;
        std::int64_t position = 0;
    };
    struct decoder_probe {
        std::atomic<bool> fail{false}, backend_destroyed{false}, decoder_destroyed{false};
        std::atomic<bool> backend_gone_before_decoder{false};
        std::atomic<bool> hold_reads{false}, read_waiting{false};
        std::atomic<std::size_t> reads{0};
    };
    struct fake_context : alia::audio_context_handle {
        fake_stream *last = nullptr;
        std::shared_ptr<decoder_probe> probe;
        std::vector<std::string> calls;
    };
    alia::created_audio_context create_fake(const alia::audio_context_config &) {
        auto context = std::make_unique<fake_context>();
        alia::audio_backend_interface ops;
        ops.id = alia::audio_backend::null;
        ops.caps = {true, false, 1, 4, "player manually pumped fake"};
        ops.destroy_context.operation = [](alia::audio_context_handle *p) { delete static_cast<fake_context *>(p); };
        ops.enumerate_devices.operation = [](alia::audio_context_handle *, alia::audio_direction direction) {
            return std::vector<alia::audio_device_info>{{"fake", "fake output", direction, true, native_format}};
        };
        ops.stream_format_supported.operation = [](alia::audio_context_handle *, alia::audio_direction,
            std::string_view, const alia::audio_stream_format &format) { return format == native_format; };
        ops.create_output_stream.operation = [](alia::audio_context_handle *p, const alia::audio_stream_desc &desc,
            alia::audio_render_fn render) -> alia::audio_stream_handle * {
            auto &context = *static_cast<fake_context *>(p);
            auto stream = std::make_unique<fake_stream>();
            stream->context = &context;
            stream->properties = {desc.format, 4, 4, 4.0 / 48000};
            stream->render = render;
            stream->user = desc.user;
            context.calls.push_back("create");
            context.last = stream.get();
            return stream.release();
        };
        ops.destroy_stream.operation = [](alia::audio_stream_handle *p) {
            auto *stream = static_cast<fake_stream *>(p);
            stream->context->calls.push_back("destroy");
            stream->context->last = nullptr;
            if (stream->context->probe)
                stream->context->probe->backend_destroyed.store(true);
            delete stream;
        };
        ops.stream_properties.operation = [](const alia::audio_stream_handle *p) {
            return static_cast<const fake_stream *>(p)->properties;
        };
        ops.stream_start.operation = [](alia::audio_stream_handle *p) {
            auto &stream = *static_cast<fake_stream *>(p);
            stream.context->calls.push_back("start");
            if (stream.state == alia::audio_stream_state::failed)
                return false;
            stream.state = alia::audio_stream_state::running;
            return true;
        };
        ops.stream_stop.operation = [](alia::audio_stream_handle *p) {
            auto &stream = *static_cast<fake_stream *>(p);
            stream.context->calls.push_back("stop");
            if (stream.state != alia::audio_stream_state::failed)
                stream.state = alia::audio_stream_state::stopped;
        };
        ops.stream_state.operation = [](const alia::audio_stream_handle *p) {
            return static_cast<const fake_stream *>(p)->state;
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
    using pcm_frame = std::array<std::int16_t, 2>;
    const std::vector<pcm_frame> short_pcm{{16384, -16384}, {8192, -8192}, {-16384, 16384}};
    class memory_decoder final : public alia::audio_decoder {
    public:
        memory_decoder(std::vector<pcm_frame> frames, bool known, std::shared_ptr<decoder_probe> probe)
            : frames_(std::move(frames)), known_(known), probe_(std::move(probe)) {}
        ~memory_decoder() override {
            probe_->backend_gone_before_decoder.store(probe_->backend_destroyed.load());
            probe_->decoder_destroyed.store(true);
        }
        alia::audio_stream_format format() const override {
            return {alia::sample_frame_format_of_v<pcm_frame>, 48000};
        }
        std::optional<std::size_t> frame_count() const override {
            return known_ ? std::optional<std::size_t>(frames_.size()) : std::nullopt;
        }
        std::size_t read(std::span<std::byte> output) override {
            if (probe_->hold_reads.load()) {
                probe_->read_waiting.store(true);
                while (probe_->hold_reads.load())
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (probe_->fail.load())
                throw std::runtime_error("test decoder failure");
            const auto n = (std::min)(output.size() / sizeof(pcm_frame), frames_.size() - position_);
            if (n)
                std::memcpy(output.data(), frames_.data() + position_, n * sizeof(pcm_frame));
            position_ += n;
            probe_->reads.fetch_add(1);
            return n;
        }
        void seek(std::size_t frame) override {
            if (frame > frames_.size())
                throw std::out_of_range("test decoder seek");
            position_ = frame;
        }
    private:
        std::vector<pcm_frame> frames_;
        bool known_;
        std::shared_ptr<decoder_probe> probe_;
        std::size_t position_ = 0;
    };
    class AudioPlayer : public ::testing::Test {
    protected:
        alia::audio_context context;
        fake_context &fake() { return *static_cast<fake_context *>(context.handle()); }
        void SetUp() override {
            alia::register_audio_backend({alia::audio_backend::null, create_fake});
            context = alia::audio_context::create(alia::audio_backend::null);
        }
        alia::audio_reader reader(std::vector<pcm_frame> frames = short_pcm, bool known = true) {
            fake().probe = std::make_shared<decoder_probe>();
            return alia::audio_reader(std::make_unique<memory_decoder>(std::move(frames), known, fake().probe));
        }
        void pump(std::span<alia::vec2f> output) {
            auto &stream = *fake().last;
            ASSERT_EQ(stream.state, alia::audio_stream_state::running);
            ASSERT_LE(output.size(), 4);
            stream.render(stream.user, reinterpret_cast<std::byte *>(output.data()), static_cast<int>(output.size()),
                          {stream.position, alia::audio_callback_flags::none});
            stream.position += output.size();
        }
        void expect_silent(std::span<const alia::vec2f> output) {
            for (auto frame : output)
                EXPECT_EQ(frame, (alia::vec2f{0, 0}));
        }
    };

    TEST_F(AudioPlayer, ClipGainOnceEofSilenceAndReplay) {
        const std::array<alia::vec2f, 3> frames{{{1, -1}, {0.5f, -0.5f}, {-0.5f, 0.5f}}};
        const alia::audio_clip clip(48000, std::span(frames));
        alia::audio_player player(context, clip.view(), {.gain = 0.5f});
        EXPECT_EQ(player.state(), alia::audio_player_state::stopped);
        EXPECT_EQ(player.frame_count(), 3);
        EXPECT_EQ(player.sample_rate(), 48000);
        player.play();
        std::array<alia::vec2f, 4> output;
        pump(output);
        EXPECT_EQ(output[0], (alia::vec2f{0.5f, -0.5f}));
        EXPECT_EQ(output[1], (alia::vec2f{0.25f, -0.25f}));
        EXPECT_EQ(output[2], (alia::vec2f{-0.25f, 0.25f}));
        EXPECT_EQ(output[3], (alia::vec2f{0, 0}));
        EXPECT_EQ(player.state(), alia::audio_player_state::stopped);
        EXPECT_EQ(fake().last->state, alia::audio_stream_state::running);
        EXPECT_EQ(player.position_frames(), 0);
        pump(output);
        expect_silent(output);
        player.play();
        pump(output);
        EXPECT_EQ(output[0], (alia::vec2f{0.5f, -0.5f}));
        player.pause();
        EXPECT_EQ(fake().last->state, alia::audio_stream_state::stopped);
    }
    TEST_F(AudioPlayer, ClipLoopPauseSeekRestartAndModeChange) {
        const std::array<alia::vec2f, 3> frames{{{0.25f, 0.25f}, {0.5f, 0.5f}, {0.75f, 0.75f}}};
        const alia::audio_clip clip(48000, std::span(frames));
        alia::audio_player player(context, clip.view(), {.mode = alia::play_mode::loop});
        player.play();
        std::array<alia::vec2f, 4> output;
        pump(output);
        EXPECT_EQ(output[3], frames[0]);
        EXPECT_EQ(player.position_frames(), 1);
        player.pause();
        EXPECT_EQ(player.state(), alia::audio_player_state::paused);
        EXPECT_EQ(player.position_frames(), 1);
        player.play();
        pump(std::span(output).first(1));
        EXPECT_EQ(output[0], frames[1]);
        player.seek(1);
        EXPECT_EQ(player.position_frames(), 1);
        pump(std::span(output).first(1));
        EXPECT_EQ(output[0], frames[1]);
        player.restart();
        pump(std::span(output).first(1));
        EXPECT_EQ(output[0], frames[0]);
        player.set_play_mode(alia::play_mode::once);
        player.set_gain(0.5f);
        EXPECT_EQ(player.mode(), alia::play_mode::once);
        EXPECT_EQ(player.gain(), 0.5f);
        pump(output);
        EXPECT_EQ(output[0], (alia::vec2f{0.25f, 0.25f}));
        EXPECT_EQ(output[1], (alia::vec2f{0.375f, 0.375f}));
        expect_silent(std::span(output).subspan(2));
        EXPECT_EQ(player.state(), alia::audio_player_state::stopped);
        player.stop();
        EXPECT_EQ(player.position_frames(), 0);
        EXPECT_EQ(fake().last->state, alia::audio_stream_state::stopped);
        EXPECT_THROW(player.seek(4), std::out_of_range);
    }
    TEST_F(AudioPlayer, ClipConvertsSigned16AndMonoThroughExistingOutputFallback) {
        const std::array<std::int16_t, 2> frames{16384, -16384};
        const alia::audio_clip clip(48000, std::span(frames));
        alia::audio_player player(context, clip.view());
        player.play();
        std::array<alia::vec2f, 4> output;
        pump(output);
        EXPECT_EQ(output[0], (alia::vec2f{0.5f, 0.5f}));
        EXPECT_EQ(output[1], (alia::vec2f{-0.5f, -0.5f}));
        expect_silent(std::span(output).subspan(2));
    }
    TEST_F(AudioPlayer, SourceRateMustMatchConfigurationAndBackendMustAcceptSourceRate) {
        const alia::audio_clip clip(3, 48000, alia::vec2f{});
        EXPECT_THROW((alia::audio_player(context, clip.view(), {.sample_rate = 44100})), std::invalid_argument);
        EXPECT_THROW((alia::audio_player(context, reader(), {.sample_rate = 44100})), std::invalid_argument);
        const alia::audio_clip other_rate(3, 44100, alia::vec2f{});
        try {
            alia::audio_player player(context, other_rate.view());
            FAIL() << "an unsupported source rate must fail";
        } catch (const std::runtime_error &error) {
            EXPECT_NE(std::string(error.what()).find("L3 resampling is unavailable"), std::string::npos);
        }
        EXPECT_EQ(fake().last, nullptr);
    }
    TEST_F(AudioPlayer, StreamedNativeConversionGainOnceAndReplay) {
        alia::audio_player player(context, reader(), {.gain = 0.5f, .buffer_frames = 16});
        ASSERT_TRUE(wait_until([&] { return fake().probe->reads.load() != 0; }));
        player.play();
        std::array<alia::vec2f, 4> output;
        pump(output);
        EXPECT_EQ(output[0], (alia::vec2f{0.25f, -0.25f}));
        EXPECT_EQ(output[1], (alia::vec2f{0.125f, -0.125f}));
        EXPECT_EQ(output[2], (alia::vec2f{-0.25f, 0.25f}));
        EXPECT_EQ(output[3], (alia::vec2f{0, 0}));
        EXPECT_EQ(player.state(), alia::audio_player_state::stopped);
        pump(output);
        expect_silent(output);
        player.play();
        pump(output);
        EXPECT_EQ(output[0], (alia::vec2f{0.25f, -0.25f}));
        EXPECT_EQ(player.state(), alia::audio_player_state::stopped);
    }
    TEST_F(AudioPlayer, StreamedLoopSeekPauseRestartAndModeChange) {
        alia::audio_player player(context, reader(), {.mode = alia::play_mode::loop, .buffer_frames = 16});
        player.play();
        std::array<alia::vec2f, 4> output;
        pump(output);
        EXPECT_EQ(output[0], (alia::vec2f{0.5f, -0.5f}));
        EXPECT_EQ(output[3], output[0]);
        EXPECT_EQ(player.position_frames(), 1);
        player.pause();
        EXPECT_EQ(player.state(), alia::audio_player_state::paused);
        player.seek(2);
        EXPECT_EQ(player.position_frames(), 2);
        EXPECT_EQ(player.state(), alia::audio_player_state::paused);
        player.play();
        pump(std::span(output).first(1));
        EXPECT_EQ(output[0], (alia::vec2f{-0.5f, 0.5f}));
        player.seek(1);
        EXPECT_EQ(player.state(), alia::audio_player_state::playing);
        pump(std::span(output).first(1));
        EXPECT_EQ(output[0], (alia::vec2f{0.25f, -0.25f}));
        player.restart();
        pump(std::span(output).first(1));
        EXPECT_EQ(output[0], (alia::vec2f{0.5f, -0.5f}));
        // Buffered loop iterations must not leak beyond the once-mode boundary.
        player.set_play_mode(alia::play_mode::once);
        pump(output);
        EXPECT_EQ(output[0], (alia::vec2f{0.25f, -0.25f}));
        EXPECT_EQ(output[1], (alia::vec2f{-0.5f, 0.5f}));
        expect_silent(std::span(output).subspan(2));
        EXPECT_EQ(player.state(), alia::audio_player_state::stopped);
        player.stop();
        EXPECT_EQ(player.position_frames(), 0);
    }
    TEST_F(AudioPlayer, StreamedUnknownLengthIsLearnedAtFirstEof) {
        alia::audio_player player(context, reader(short_pcm, false), {.buffer_frames = 16});
        ASSERT_TRUE(wait_until([&] { return player.frame_count().has_value(); }));
        EXPECT_EQ(player.frame_count(), 3);
        player.play();
        std::array<alia::vec2f, 4> output;
        pump(output);
        EXPECT_EQ(player.state(), alia::audio_player_state::stopped);
        EXPECT_EQ(player.position_frames(), 0);
        player.restart();
        pump(std::span(output).first(1));
        EXPECT_EQ(output[0], (alia::vec2f{0.5f, -0.5f}));
    }
    TEST_F(AudioPlayer, InvalidUnknownLengthSeekPreservesPlaybackAndPosition) {
        alia::audio_player player(context, reader(std::vector<pcm_frame>(64, {16384, -16384}), false),
                                   {.buffer_frames = 8});
        EXPECT_FALSE(player.frame_count());
        player.play();
        EXPECT_THROW(player.seek(100), std::out_of_range);
        EXPECT_EQ(player.state(), alia::audio_player_state::playing);
        EXPECT_EQ(player.position_frames(), 0);
        std::array<alia::vec2f, 4> output;
        pump(output);
        for (auto frame : output)
            EXPECT_EQ(frame, (alia::vec2f{0.5f, -0.5f}));
        EXPECT_EQ(player.position_frames(), 4);
    }
    TEST_F(AudioPlayer, DecoderFailureIsLatchedSilencesOutputAndIsRethrown) {
        alia::audio_player player(context, reader(std::vector<pcm_frame>(64, {16384, -16384})),
                                   {.buffer_frames = 8});
        player.play();
        fake().probe->fail.store(true);
        std::array<alia::vec2f, 4> output;
        pump(output); // Frees ring space so the worker attempts another decode.
        ASSERT_TRUE(wait_until([&] { return player.state() == alia::audio_player_state::failed; }));
        pump(output);
        expect_silent(output);
        try {
            player.play();
            FAIL() << "play must rethrow the decoder failure";
        } catch (const std::runtime_error &error) {
            EXPECT_STREQ(error.what(), "test decoder failure");
        }
        player.pause();
        EXPECT_EQ(player.state(), alia::audio_player_state::failed);
    }
    TEST_F(AudioPlayer, StreamUnderflowProducesSilenceWithoutAdvancingPosition) {
        alia::audio_player player(context, reader(std::vector<pcm_frame>(64, {16384, -16384})),
                                   {.buffer_frames = 8});
        auto probe = fake().probe;
        // Release a blocked decoder before destroying the player, including
        // assertion-failure exits from this test.
        struct release_guard {
            std::shared_ptr<decoder_probe> probe;
            ~release_guard() { probe->hold_reads.store(false); }
        } guard{probe};
        player.play();
        probe->hold_reads.store(true);
        std::array<alia::vec2f, 4> output;
        pump(output);
        pump(output);
        ASSERT_TRUE(wait_until([&] { return probe->read_waiting.load(); }));
        EXPECT_EQ(player.position_frames(), 8);
        pump(output);
        expect_silent(output);
        EXPECT_EQ(player.position_frames(), 8);
        EXPECT_EQ(player.state(), alia::audio_player_state::playing);
        probe->hold_reads.store(false);
        player.pause();
        player.play(); // Waits for worker prefill before resuming.
        pump(output);
        for (auto frame : output)
            EXPECT_EQ(frame, (alia::vec2f{0.5f, -0.5f}));
        EXPECT_EQ(player.position_frames(), 12);
    }
    TEST_F(AudioPlayer, InitialDecoderFailureSurvivesConstructionAndPlayRethrows) {
        auto source = reader();
        fake().probe->fail.store(true);
        alia::audio_player player(context, std::move(source), {.buffer_frames = 8});
        EXPECT_EQ(player.state(), alia::audio_player_state::failed);
        EXPECT_THROW(player.play(), std::runtime_error);
    }
    TEST_F(AudioPlayer, DeviceFailureIsReportedAndDoesNotRestart) {
        const alia::audio_clip clip(3, 48000, alia::vec2f{});
        alia::audio_player player(context, clip.view());
        player.play();
        fake().last->state = alia::audio_stream_state::failed;
        EXPECT_EQ(player.state(), alia::audio_player_state::failed);
        EXPECT_THROW(player.play(), std::runtime_error);
    }
    TEST_F(AudioPlayer, MovesKeepCallbackStorageAndDestroyBackendBeforeDecoder) {
        std::shared_ptr<decoder_probe> probe;
        {
            alia::audio_player original(context, reader(), {.mode = alia::play_mode::loop, .buffer_frames = 16});
            probe = fake().probe;
            original.play();
            const auto *address = fake().last->user;
            auto moved = std::move(original);
            EXPECT_EQ(original.state(), alia::audio_player_state::stopped);
            EXPECT_EQ(fake().last->user, address);
            std::array<alia::vec2f, 4> output;
            pump(output);
            EXPECT_EQ(output[0], (alia::vec2f{0.5f, -0.5f}));
            EXPECT_EQ(moved.state(), alia::audio_player_state::playing);
        }
        EXPECT_TRUE(probe->decoder_destroyed.load());
        EXPECT_TRUE(probe->backend_destroyed.load());
        EXPECT_TRUE(probe->backend_gone_before_decoder.load());
        EXPECT_EQ(fake().last, nullptr);
    }
    TEST_F(AudioPlayer, EmptyClipAndStreamStopEvenInLoopMode) {
        {
            const alia::audio_clip clip(0, 48000, alia::vec2f{});
            alia::audio_player player(context, clip.view(), {.mode = alia::play_mode::loop});
            player.play();
            std::array<alia::vec2f, 4> output;
            pump(output);
            expect_silent(output);
            EXPECT_EQ(player.state(), alia::audio_player_state::stopped);
        }
        alia::audio_player player(context, reader({}, false), {.mode = alia::play_mode::loop, .buffer_frames = 8});
        player.play();
        std::array<alia::vec2f, 4> output;
        pump(output);
        expect_silent(output);
        EXPECT_EQ(player.state(), alia::audio_player_state::stopped);
        EXPECT_EQ(player.frame_count(), 0);
    }
    TEST_F(AudioPlayer, RejectsInvalidGainAndDefaultPlayersHaveNoSource) {
        const alia::audio_clip clip(1, 48000, alia::vec2f{});
        EXPECT_THROW((alia::audio_player(context, clip.view(), {.gain = std::numeric_limits<float>::infinity()})), std::invalid_argument);
        alia::audio_player player(context, clip.view());
        EXPECT_THROW(player.set_gain(std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
        alia::audio_player empty;
        EXPECT_EQ(empty.state(), alia::audio_player_state::stopped);
        EXPECT_EQ(empty.position_frames(), 0);
        EXPECT_EQ(empty.sample_rate(), 0);
        EXPECT_FALSE(empty.frame_count());
        EXPECT_THROW(empty.play(), std::logic_error);
        empty.pause();
        empty.stop();
    }
} // namespace
