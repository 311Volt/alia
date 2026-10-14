#include "alia/audio/audio.hpp"
#include <gtest/gtest.h>
#include <cstring>

namespace {
    struct fake_context;
    struct fake_stream : alia::audio_stream_handle {
        fake_context *context = nullptr;
        alia::audio_stream_properties properties;
        alia::audio_render_fn render = nullptr;
        alia::audio_capture_fn capture = nullptr;
        void *user = nullptr;
        alia::audio_stream_state state = alia::audio_stream_state::stopped;
        std::int64_t position = 0;
    };
    struct fake_context : alia::audio_context_handle {
        fake_stream *last = nullptr;
        bool reject_creates = false;
        std::vector<std::string> calls;
    };
    constexpr alia::audio_stream_format native_format{alia::sample_frame_format_of_v<alia::vec2f>, 48000};
    alia::created_audio_context create_fake(const alia::audio_context_config &) {
        auto context = std::make_unique<fake_context>();
        alia::audio_backend_interface ops;
        ops.id = alia::audio_backend::null;
        ops.caps = {true, true, 1, 4, "manually pumped fake"};
        ops.destroy_context.operation = [](alia::audio_context_handle *p) { delete static_cast<fake_context *>(p); };
        ops.enumerate_devices.operation = [](alia::audio_context_handle *, alia::audio_direction direction) {
            return std::vector<alia::audio_device_info>{{"fake", "fake endpoint", direction, true, native_format}};
        };
        ops.stream_format_supported.operation = [](alia::audio_context_handle *, alia::audio_direction,
                                                    std::string_view, const alia::audio_stream_format &format) { return format == native_format; };
        ops.create_output_stream.operation = [](alia::audio_context_handle *p, const alia::audio_stream_desc &desc,
                                                alia::audio_render_fn render) -> alia::audio_stream_handle * {
            auto &context = *static_cast<fake_context *>(p);
            if (context.reject_creates) return nullptr;
            auto stream = std::make_unique<fake_stream>();
            stream->context = &context;
            stream->properties = {desc.format, 4, 4, 4.0 / 48000};
            stream->render = render;
            stream->user = desc.user;
            context.calls.push_back("create");
            context.last = stream.get();
            return stream.release();
        };
        ops.create_input_stream.operation = [](alia::audio_context_handle *p, const alia::audio_stream_desc &desc,
                                               alia::audio_capture_fn capture) -> alia::audio_stream_handle * {
            auto &context = *static_cast<fake_context *>(p);
            auto stream = std::make_unique<fake_stream>();
            stream->context = &context;
            stream->properties = {desc.format, 4, 4, 4.0 / 48000};
            stream->capture = capture;
            stream->user = desc.user;
            context.calls.push_back("create");
            context.last = stream.get();
            return stream.release();
        };
        ops.destroy_stream.operation = [](alia::audio_stream_handle *p) {
            auto *stream = static_cast<fake_stream *>(p);
            stream->context->calls.push_back("destroy");
            stream->context->last = nullptr;
            delete stream;
        };
        ops.stream_properties.operation = [](const alia::audio_stream_handle *p) { return static_cast<const fake_stream *>(p)->properties; };
        ops.stream_start.operation = [](alia::audio_stream_handle *p) {
            auto &stream = *static_cast<fake_stream *>(p);
            stream.context->calls.push_back("start");
            if (stream.state == alia::audio_stream_state::failed) return false;
            stream.state = alia::audio_stream_state::running;
            return true;
        };
        ops.stream_stop.operation = [](alia::audio_stream_handle *p) {
            auto &stream = *static_cast<fake_stream *>(p);
            stream.context->calls.push_back("stop");
            if (stream.state != alia::audio_stream_state::failed) stream.state = alia::audio_stream_state::stopped;
        };
        ops.stream_state.operation = [](const alia::audio_stream_handle *p) { return static_cast<const fake_stream *>(p)->state; };
        ops.stream_position.operation = [](const alia::audio_stream_handle *p) { return static_cast<const fake_stream *>(p)->position; };
        return {context.release(), std::move(ops)};
    }
    class AudioStream : public ::testing::Test {
    protected:
        alia::audio_context context;
        fake_context &fake() { return *static_cast<fake_context *>(context.handle()); }
        void SetUp() override {
            alia::register_audio_backend({alia::audio_backend::null, create_fake});
            context = alia::audio_context::create(alia::audio_backend::null);
        }
        void pump(std::span<alia::vec2f> frames, alia::audio_callback_flags flags = alia::audio_callback_flags::none) {
            auto &stream = *fake().last;
            ASSERT_EQ(stream.state, alia::audio_stream_state::running);
            ASSERT_LE(frames.size(), 4);
            const alia::audio_callback_info info{stream.position, flags};
            if (stream.render) stream.render(stream.user, reinterpret_cast<std::byte *>(frames.data()), static_cast<int>(frames.size()), info);
            else stream.capture(stream.user, reinterpret_cast<const std::byte *>(frames.data()), static_cast<int>(frames.size()), info);
            stream.position += frames.size();
        }
    };
    TEST_F(AudioStream, ExactPassThroughAndPartialOutputSilence) {
        std::array<alia::vec2f, 4> frames;
        int calls = 0;
        alia::output_stream<alia::vec2f> stream(context, {.exact_format = true},
            [&](std::span<alia::vec2f> output, const alia::audio_callback_info &info) {
                EXPECT_EQ(output.data(), frames.data());
                EXPECT_EQ(info.stream_frame, 0);
                EXPECT_EQ(info.flags, alia::audio_callback_flags::underflow);
                output[0] = {0.25f, -0.25f};
                ++calls;
            });
        EXPECT_EQ(stream.state(), alia::audio_stream_state::stopped);
        stream.start();
        pump(frames, alia::audio_callback_flags::underflow);
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(frames[0], (alia::vec2f{0.25f, -0.25f}));
        EXPECT_EQ(frames[1], (alia::vec2f{0, 0}));
        EXPECT_EQ(stream.position_frames(), 4);
        stream.stop();
        EXPECT_EQ(stream.state(), alia::audio_stream_state::stopped);
    }
    TEST_F(AudioStream, ConvertsSigned16MonoToFloatStereo) {
        alia::output_stream<std::int16_t> stream(context, {}, [](std::span<std::int16_t> frames) {
            std::fill(frames.begin(), frames.end(), std::int16_t{16384});
        });
        EXPECT_EQ(stream.properties().format.frame, alia::sample_frame_format_of_v<std::int16_t>);
        EXPECT_EQ(fake().last->properties.format, native_format);
        std::array<alia::vec2f, 4> output;
        stream.start();
        pump(output);
        for (auto frame : output) EXPECT_EQ(frame, (alia::vec2f{0.5f, 0.5f}));
    }
    TEST_F(AudioStream, ExactFormatAndUnsupportedRateThrow) {
        EXPECT_THROW((alia::output_stream<std::int16_t>(context, {.exact_format = true}, [](std::span<std::int16_t>) {})), std::runtime_error);
        EXPECT_THROW((alia::output_stream<alia::vec2f>(context, {.sample_rate = 44100}, [](std::span<alia::vec2f>) {})), std::runtime_error);
        EXPECT_EQ(fake().last, nullptr);
        fake().reject_creates = true;
        EXPECT_THROW((alia::output_stream<alia::vec2f>(context, {}, [](std::span<alia::vec2f>) {})), std::runtime_error);
    }
    TEST_F(AudioStream, ThrowingCallbackLatchesFailureAndSilencesEveryLaterBuffer) {
        int calls = 0;
        alia::output_stream<alia::vec2f> stream(context, {}, [&](std::span<alia::vec2f> output) {
            ++calls;
            output[0] = {1, 1};
            throw std::runtime_error("callback failed");
        });
        stream.start();
        std::array<alia::vec2f, 4> output;
        pump(output);
        EXPECT_EQ(stream.state(), alia::audio_stream_state::failed);
        for (auto frame : output) EXPECT_EQ(frame, (alia::vec2f{0, 0}));
        pump(output);
        EXPECT_EQ(calls, 1);
        for (auto frame : output) EXPECT_EQ(frame, (alia::vec2f{0, 0}));
        stream.stop();
        EXPECT_EQ(stream.state(), alia::audio_stream_state::failed);
        EXPECT_THROW(stream.start(), std::runtime_error);
    }
    TEST_F(AudioStream, MovesRunningStreamAndContextWithoutMovingCallbackStorage) {
        int calls = 0;
        alia::output_stream<alia::vec2f> original(context, {}, [&](std::span<alia::vec2f>) { ++calls; });
        original.start();
        const auto *user = fake().last->user;
        auto moved = std::move(original);
        auto moved_context = std::move(context);
        EXPECT_EQ(moved.state(), alia::audio_stream_state::running);
        std::array<alia::vec2f, 4> output;
        auto &backend_stream = *static_cast<fake_context *>(moved_context.handle())->last;
        EXPECT_EQ(backend_stream.user, user);
        backend_stream.render(backend_stream.user, reinterpret_cast<std::byte *>(output.data()), 4, {});
        EXPECT_EQ(calls, 1);
        // Restore the context so it outlives the stream at scope exit.
        context = std::move(moved_context);
    }
    TEST_F(AudioStream, StopsAndDestroysBackendBeforeDestroyingCallback) {
        struct callback_lifetime {
            std::vector<std::string> &calls;
            ~callback_lifetime() { calls.push_back("callback destroyed"); }
        };
        {
            alia::output_stream<alia::vec2f> stream(context, {},
                [guard = std::make_unique<callback_lifetime>(fake().calls)](std::span<alia::vec2f>) {});
            stream.start();
        }
        EXPECT_EQ(fake().calls, (std::vector<std::string>{"create", "start", "stop", "destroy", "callback destroyed"}));
    }
    TEST_F(AudioStream, CapturesAndConvertsStereoToSigned16Mono) {
        std::array<std::int16_t, 4> captured{};
        alia::input_stream<std::int16_t> stream(context, {},
            [&](std::span<const std::int16_t> frames, const alia::audio_callback_info &info) {
                EXPECT_EQ(info.flags, alia::audio_callback_flags::discontinuity);
                std::copy(frames.begin(), frames.end(), captured.begin());
            });
        stream.start();
        std::array<alia::vec2f, 4> input;
        input.fill({0.75f, 0.25f});
        pump(input, alia::audio_callback_flags::discontinuity);
        for (auto point : captured) EXPECT_EQ(point, 16384);
        EXPECT_EQ(stream.sample_rate(), 48000);
        EXPECT_EQ(stream.position_frames(), 4);
    }
    TEST_F(AudioStream, ThrowingCaptureCallbackStopsUserDelivery) {
        int calls = 0;
        alia::input_stream<alia::vec2f> stream(context, {}, [&](std::span<const alia::vec2f>) {
            ++calls;
            throw 1;
        });
        stream.start();
        std::array<alia::vec2f, 4> input;
        pump(input);
        pump(input);
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(stream.state(), alia::audio_stream_state::failed);
    }
}
