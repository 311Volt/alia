#include "wasapi_ops.hpp"
#ifdef ALIA_COMPILE_AUDIO_BACKEND_WASAPI

namespace alia {
    namespace {
        created_audio_context create_wasapi_context(const audio_context_config &) {
            detail::wasapi_com_scope apartment;
            detail::wasapi_com_ptr<IMMDeviceEnumerator> enumerator;
            // On XP the COM class is absent. No mmdevapi/avrt static imports
            // prevent the application from starting before backend selection.
            if (!apartment.usable() || !detail::wasapi_enumerator(enumerator))
                return {};
            auto context = std::make_unique<detail::wasapi_context>();
            wchar_t directory[MAX_PATH + 1]{};
            const UINT length = GetSystemDirectoryW(directory, MAX_PATH);
            if (length && length < MAX_PATH) {
                std::wstring path(directory, length);
                path += L"\\avrt.dll";
                context->avrt = LoadLibraryW(path.c_str());
                if (context->avrt) {
                    context->set_mm_thread = reinterpret_cast<decltype(context->set_mm_thread)>(
                        GetProcAddress(context->avrt, "AvSetMmThreadCharacteristicsW")
                    );
                    context->revert_mm_thread = reinterpret_cast<decltype(context->revert_mm_thread)>(
                        GetProcAddress(context->avrt, "AvRevertMmThreadCharacteristics")
                    );
                }
            }
            audio_backend_interface ops;
            ops.id = audio_backend::wasapi;
            ops.caps.renderer_name = "WASAPI shared (event driven)";
            ops.caps.has_output = !detail::wasapi_enumerate_devices(context.get(), audio_direction::output).empty();
            ops.caps.has_input = !detail::wasapi_enumerate_devices(context.get(), audio_direction::input).empty();
            ops.destroy_context.operation = [](audio_context_handle *handle) {
                delete detail::as_wasapi_context(handle);
            };
            ops.enumerate_devices.operation = detail::wasapi_enumerate_devices;
            ops.stream_format_supported.operation = detail::wasapi_format_supported;
            if (ops.caps.has_output)
                ops.create_output_stream.operation = detail::wasapi_create_output;
            else
                ops.create_output_stream.reason_unsupported = "no active WASAPI output endpoints at context creation";
            if (ops.caps.has_input)
                ops.create_input_stream.operation = detail::wasapi_create_input;
            else
                ops.create_input_stream.reason_unsupported = "no active WASAPI capture endpoints at context creation";
            ops.destroy_stream.operation = detail::wasapi_destroy_stream;
            ops.stream_properties.operation = detail::wasapi_properties;
            ops.stream_start.operation = detail::wasapi_start;
            ops.stream_stop.operation = detail::wasapi_stop;
            ops.stream_state.operation = detail::wasapi_state;
            ops.stream_position.operation = detail::wasapi_position;
            return {context.release(), std::move(ops)};
        }
    } // namespace

    void register_wasapi_audio_backend() {
        register_audio_backend({audio_backend::wasapi, create_wasapi_context});
    }
} // namespace alia
#endif
