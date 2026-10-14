#include "alsa_ops.hpp"
#ifdef ALIA_COMPILE_AUDIO_BACKEND_ALSA

namespace alia {
    namespace {
        created_audio_context create_alsa_context(const audio_context_config &) {
            auto context = std::make_unique<detail::alsa_context>();
            audio_backend_interface ops;
            ops.id = audio_backend::alsa;
            ops.caps.renderer_name = "ALSA PCM (nonblocking)";
            ops.caps.has_output = !detail::alsa_enumerate_devices(context.get(), audio_direction::output).empty();
            ops.caps.has_input = !detail::alsa_enumerate_devices(context.get(), audio_direction::input).empty();
            if (!ops.caps.has_output && !ops.caps.has_input) return {};
            ops.destroy_context.operation = [](audio_context_handle *p) { delete detail::as_alsa_context(p); };
            ops.enumerate_devices.operation = detail::alsa_enumerate_devices;
            ops.stream_format_supported.operation = detail::alsa_format_supported;
            if (ops.caps.has_output) ops.create_output_stream.operation = detail::alsa_create_output;
            else ops.create_output_stream.reason_unsupported = "no available ALSA playback endpoints at context creation";
            if (ops.caps.has_input) ops.create_input_stream.operation = detail::alsa_create_input;
            else ops.create_input_stream.reason_unsupported = "no available ALSA capture endpoints at context creation";
            ops.destroy_stream.operation = detail::alsa_destroy_stream;
            ops.stream_properties.operation = detail::alsa_properties;
            ops.stream_start.operation = detail::alsa_start;
            ops.stream_stop.operation = detail::alsa_stop;
            ops.stream_state.operation = detail::alsa_state;
            ops.stream_position.operation = detail::alsa_position;
            return {context.release(), std::move(ops)};
        }
    }
    void register_alsa_audio_backend() { register_audio_backend({audio_backend::alsa, create_alsa_context}); }
}
#endif
