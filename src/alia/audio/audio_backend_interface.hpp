#ifndef ALIA_AUDIO_BACKEND_INTERFACE_HPP
#define ALIA_AUDIO_BACKEND_INTERFACE_HPP

#include "../core/backend_op.hpp"
#include "sample.hpp"
#include <string_view>
#include <vector>

namespace alia {
    struct audio_context_handle {};

    struct audio_stream_handle {};
    /// auto_ tries the compiled backends in WASAPI, MME, ALSA order.
    /// MME uses Windows waveform audio; ALSA uses Linux PCM devices.
    /// MME/ALSA discard queued buffers on stop; restart reports discontinuity.
    enum class audio_backend {
        auto_,
        wasapi,
        mme,
        alsa,
        null
    };
    enum class audio_direction {
        output,
        input
    };
    enum class audio_stream_state {
        stopped,
        running,
        failed
    };
    enum class audio_callback_flags : std::uint32_t {
        none = 0,
        underflow = 1,
        overflow = 2,
        discontinuity = 4
    };

    constexpr audio_callback_flags operator|(audio_callback_flags a, audio_callback_flags b) noexcept {
        return static_cast<audio_callback_flags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
    }

    constexpr audio_callback_flags operator&(audio_callback_flags a, audio_callback_flags b) noexcept {
        return static_cast<audio_callback_flags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
    }

    struct audio_caps {
        bool has_output = false, has_input = false;
        /// Zero means endpoint-dependent; stream properties report actual values.
        int min_period_frames = 0, max_period_frames = 0;
        std::string renderer_name;
    };

    struct audio_context_config {};

    struct audio_device_info {
        /// Backend-local endpoint id. MME exposes "default", "out:<index>"
        /// and "in:<index>"; ALSA exposes PCM names such as "default" and
        /// "hw:CARD=PCH,DEV=0". An opened stream retains its selected endpoint.
        std::string id, name;
        audio_direction direction{};
        bool is_default = false;
        audio_stream_format preferred{};
    };

    struct audio_callback_info {
        std::int64_t stream_frame = 0;
        audio_callback_flags flags = audio_callback_flags::none;
    };

    using audio_render_fn = void (*)(void *, std::byte *, int frame_count, const audio_callback_info &) noexcept;
    using audio_capture_fn = void (*)(void *, const std::byte *, int frame_count, const audio_callback_info &) noexcept;

    struct audio_stream_desc {
        std::string_view device_id; // Empty selects the default endpoint at open.
        audio_stream_format format;
        int period_frames = 0;
        void *user = nullptr;
    };

    struct audio_stream_properties {
        audio_stream_format format;
        int period_frames = 0, max_callback_frames = 0;
        double latency_seconds = 0;
    };

    /// One table per context. Callbacks run on a backend thread, never exceed
    /// max_callback_frames, and must not call stop/destroy. All callback memory
    /// is provisioned at open. Contexts must outlive streams.
    struct audio_backend_interface {
        audio_backend id = audio_backend::null;
        audio_caps caps;
        /// Releases the context; all streams must already have been destroyed.
        backend_op<void(audio_context_handle *)> destroy_context;
        /// Active endpoints and their exact preferred callback formats.
        backend_op<std::vector<audio_device_info>(audio_context_handle *, audio_direction)> enumerate_devices;
        /// True only if the exact callback format/rate can be opened. Host-side
        /// conversion is permitted; a closest match alone is not support.
        backend_op<
            bool(audio_context_handle *, audio_direction, std::string_view, const audio_stream_format &)>
            stream_format_supported;
        /// Creates a stopped stream, or returns null. Never substitutes a
        /// format/rate, and never calls the callback before stream_start.
        optional_backend_op<audio_stream_handle *(audio_context_handle *, const audio_stream_desc &, audio_render_fn)>
            create_output_stream;
        /// Capture counterpart. A null slot denotes no capture hardware/platform support.
        optional_backend_op<audio_stream_handle *(audio_context_handle *, const audio_stream_desc &, audio_capture_fn)>
            create_input_stream;
        /// Stops and joins the worker before releasing storage. No callbacks
        /// are active or possible after return; forbidden from a callback.
        backend_op<void(audio_stream_handle *)> destroy_stream;
        /// Actual callback format, period, maximum buffer size and latency.
        backend_op<audio_stream_properties(const audio_stream_handle *)> stream_properties;
        /// Starts/resumes a stopped stream. False on failure; idempotent when running.
        backend_op<bool(audio_stream_handle *)> stream_start;
        /// Synchronously quiesces the callback; idempotent. Must not be called
        /// from a callback. A subsequent start may resume the stream.
        backend_op<void(audio_stream_handle *)> stream_stop;
        /// Device loss/backend errors latch failed; no later callbacks or rerouting.
        backend_op<audio_stream_state(const audio_stream_handle *)> stream_state;
        /// Hardware-presented/captured frame position in the callback's rate.
        backend_op<std::int64_t(const audio_stream_handle *)> stream_position;
    };

    struct created_audio_context {
        audio_context_handle *handle = nullptr;
        audio_backend_interface iface;
    };

    struct audio_backend_factory {
        audio_backend id;
        created_audio_context (*create)(const audio_context_config &);
    };

    /// Thread-safe registration; an existing backend id is replaced.
    void register_audio_backend(audio_backend_factory);
} // namespace alia
#endif
