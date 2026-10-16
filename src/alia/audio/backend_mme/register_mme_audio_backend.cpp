#include "mme_ops.hpp"
#ifdef ALIA_COMPILE_AUDIO_BACKEND_MME

namespace alia {
    namespace {
        created_audio_context create_mme_context(const audio_context_config &) {
            auto context = std::make_unique<detail::mme_context>();
            audio_backend_interface ops;
            ops.id = audio_backend::mme;
            ops.caps.renderer_name = "MME waveform audio";
            ops.caps.has_output = waveOutGetNumDevs() != 0;
            ops.caps.has_input = waveInGetNumDevs() != 0;
            if (!ops.caps.has_output && !ops.caps.has_input)
                return {};
            ops.destroy_context.operation = [](audio_context_handle *p) { delete detail::as_mme_context(p); };
            ops.enumerate_devices.operation = detail::mme_enumerate_devices;
            ops.stream_format_supported.operation = detail::mme_format_supported;
            if (ops.caps.has_output)
                ops.create_output_stream.operation = detail::mme_create_output;
            else
                ops.create_output_stream.reason_unsupported = "no MME output devices at context creation";
            if (ops.caps.has_input)
                ops.create_input_stream.operation = detail::mme_create_input;
            else
                ops.create_input_stream.reason_unsupported = "no MME capture devices at context creation";
            ops.destroy_stream.operation = detail::mme_destroy_stream;
            ops.stream_properties.operation = detail::mme_properties;
            ops.stream_start.operation = detail::mme_start;
            ops.stream_stop.operation = detail::mme_stop;
            ops.stream_state.operation = detail::mme_state;
            ops.stream_position.operation = detail::mme_position;
            return {context.release(), std::move(ops)};
        }
    } // namespace

    void register_mme_audio_backend() {
        register_audio_backend({audio_backend::mme, create_mme_context});
    }
} // namespace alia
#endif
