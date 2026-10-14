#include "wasapi_ops.hpp"
#ifdef ALIA_COMPILE_AUDIO_BACKEND_WASAPI
#include "../sample_conversions.hpp"
#include <limits>
#include <vector>

namespace alia::detail {
    namespace {
        struct mmcss_scope {
            wasapi_context &context;
            HANDLE task = nullptr;
            explicit mmcss_scope(wasapi_context &context) : context(context) {
                DWORD index = 0;
                if (context.set_mm_thread && context.revert_mm_thread) task = context.set_mm_thread(L"Pro Audio", &index);
            }
            ~mmcss_scope() { if (task) context.revert_mm_thread(task); }
        };
        void run_stream(wasapi_stream &stream) noexcept {
            struct signal_exit {
                HANDLE event;
                ~signal_exit() { SetEvent(event); }
            } on_exit{stream.exit_event.value};
            bool startup_published = false;
            try {
                wasapi_com_scope apartment(COINIT_MULTITHREADED);
                wasapi_com_ptr<IMMDeviceEnumerator> enumerator;
                wasapi_com_ptr<IMMDevice> endpoint;
                wasapi_com_ptr<IAudioClient> client;
                wasapi_com_ptr<IAudioRenderClient> render;
                wasapi_com_ptr<IAudioCaptureClient> capture;
                wasapi_com_ptr<IAudioClock> clock;
                if (!apartment.usable() || !wasapi_enumerator(enumerator) ||
                    !wasapi_endpoint(enumerator.get(), stream.direction, stream.device_id, endpoint) ||
                    !wasapi_initialize(endpoint.get(), stream.properties.format, stream.requested_period, client))
                    throw std::runtime_error("WASAPI stream initialization failed");
                if (FAILED(client->SetEventHandle(stream.audio_event.value))) throw std::runtime_error("WASAPI event setup failed");
                const bool output = stream.direction == audio_direction::output;
                if (output) {
                    if (FAILED(client->GetService(wasapi_render_iid, reinterpret_cast<void **>(render.put()))))
                        throw std::runtime_error("WASAPI render service unavailable");
                } else if (FAILED(client->GetService(wasapi_capture_iid, reinterpret_cast<void **>(capture.put()))))
                    throw std::runtime_error("WASAPI capture service unavailable");
                if (FAILED(client->GetService(wasapi_clock_iid, reinterpret_cast<void **>(clock.put()))))
                    throw std::runtime_error("WASAPI clock unavailable");
                UINT64 frequency = 0;
                if (FAILED(clock->GetFrequency(&frequency)) || !frequency) throw std::runtime_error("WASAPI clock frequency unavailable");
                UINT32 capacity = 0;
                REFERENCE_TIME period = 0, minimum = 0, latency = 0;
                if (FAILED(client->GetBufferSize(&capacity)) || !capacity || capacity > static_cast<UINT32>((std::numeric_limits<int>::max)()) ||
                    FAILED(client->GetDevicePeriod(&period, &minimum)) || FAILED(client->GetStreamLatency(&latency)))
                    throw std::runtime_error("WASAPI buffer properties unavailable");
                stream.properties.max_callback_frames = static_cast<int>(capacity);
                stream.properties.period_frames = static_cast<int>((std::max<REFERENCE_TIME>)(1,
                    (period * stream.properties.format.sample_rate + 9999999) / 10000000));
                stream.properties.latency_seconds = double(latency) / 10000000.0;
                std::vector<std::byte> silent;
                if (!output) {
                    silent.resize(audio_byte_count(stream.properties.format.frame, capacity));
                    silence_audio_bytes(silent.data(), capacity, stream.properties.format.frame);
                }
                mmcss_scope scheduling(*stream.context);
                stream.opened = true;
                startup_published = true;
                SetEvent(stream.ready_event.value);
                bool running = false, quitting = false, rendered = false;
                std::int64_t render_frame = 0, capture_frame = 0;
                const HANDLE events[] = {stream.command_event.value, stream.audio_event.value};
                auto update_position = [&]() noexcept -> bool {
                    UINT64 ticks = 0;
                    if (FAILED(clock->GetPosition(&ticks, nullptr))) return false;
                    // Clock units vary by device: position/frequency is seconds.
                    const long double frames = static_cast<long double>(ticks) * stream.properties.format.sample_rate / frequency;
                    const auto position = frames >= static_cast<long double>((std::numeric_limits<std::int64_t>::max)()) ?
                        (std::numeric_limits<std::int64_t>::max)() : static_cast<std::int64_t>(frames);
                    const auto previous = stream.position.load(std::memory_order_relaxed);
                    stream.position.store((std::max)(previous, position), std::memory_order_release);
                    return true;
                };
                auto fail = [&]() noexcept {
                    if (running) client->Stop();
                    running = false;
                    stream.state.store(audio_stream_state::failed, std::memory_order_release);
                };
                while (!quitting) {
                    // A timeout also notices device loss when the engine stops signaling.
                    const DWORD wake = WaitForMultipleObjects(2, events, FALSE, 1000);
                    if (wake == WAIT_OBJECT_0) {
                        const auto command = stream.command.exchange(wasapi_command::none, std::memory_order_acq_rel);
                        if (command == wasapi_command::start) {
                            stream.started = false;
                            if (stream.state.load(std::memory_order_acquire) != audio_stream_state::failed) {
                                HRESULT result = S_OK;
                                if (!running && output) {
                                    UINT32 padding = 0;
                                    result = client->GetCurrentPadding(&padding);
                                    if (SUCCEEDED(result) && padding > capacity) result = E_FAIL;
                                    if (SUCCEEDED(result) && capacity > padding) {
                                        BYTE *buffer = nullptr;
                                        result = render->GetBuffer(capacity - padding, &buffer);
                                        if (SUCCEEDED(result)) {
                                            result = render->ReleaseBuffer(capacity - padding, AUDCLNT_BUFFERFLAGS_SILENT);
                                            if (SUCCEEDED(result)) render_frame += capacity - padding;
                                        }
                                    }
                                }
                                if (SUCCEEDED(result) && !running) result = client->Start();
                                if (FAILED(result)) fail();
                                else {
                                    running = true;
                                    stream.started = true;
                                    stream.state.store(audio_stream_state::running, std::memory_order_release);
                                }
                            }
                        } else if (command == wasapi_command::stop || command == wasapi_command::quit) {
                            if (running) {
                                const HRESULT stopped = client->Stop();
                                if (!update_position() || FAILED(stopped)) fail();
                            }
                            running = false;
                            if (stream.state.load(std::memory_order_acquire) != audio_stream_state::failed)
                                stream.state.store(audio_stream_state::stopped, std::memory_order_release);
                            quitting = command == wasapi_command::quit;
                        }
                        SetEvent(stream.ready_event.value);
                        continue;
                    }
                    if (wake != WAIT_OBJECT_0 + 1 && wake != WAIT_TIMEOUT) { fail(); break; }
                    if (!running) continue;
                    if (!update_position()) { fail(); continue; }
                    if (stream.command.load(std::memory_order_acquire) != wasapi_command::none) continue;
                    if (output) {
                        UINT32 padding = 0;
                        if (FAILED(client->GetCurrentPadding(&padding)) || padding > capacity) { fail(); continue; }
                        const UINT32 count = capacity - padding;
                        if (!count) continue;
                        BYTE *data = nullptr;
                        if (FAILED(render->GetBuffer(count, &data))) { fail(); continue; }
                        const audio_callback_info info{render_frame,
                            rendered && !padding ? audio_callback_flags::underflow : audio_callback_flags::none};
                        stream.render(stream.user, reinterpret_cast<std::byte *>(data), static_cast<int>(count), info);
                        render_frame += count;
                        rendered = true;
                        if (FAILED(render->ReleaseBuffer(count, 0))) fail();
                    } else {
                        while (running && stream.command.load(std::memory_order_acquire) == wasapi_command::none) {
                            UINT32 packet = 0;
                            if (FAILED(capture->GetNextPacketSize(&packet))) { fail(); break; }
                            if (!packet) break;
                            BYTE *data = nullptr;
                            UINT32 count = 0;
                            DWORD flags = 0;
                            UINT64 device_frame = 0, qpc = 0;
                            if (FAILED(capture->GetBuffer(&data, &count, &flags, &device_frame, &qpc))) { fail(); break; }
                            if (!count) break; // AUDCLNT_S_BUFFER_EMPTY has no packet to release.
                            if (count > capacity) { capture->ReleaseBuffer(count); fail(); break; }
                            audio_callback_flags callback_flags = audio_callback_flags::none;
                            if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
                                callback_flags = audio_callback_flags::discontinuity | audio_callback_flags::overflow;
                            if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
                                callback_flags = callback_flags | audio_callback_flags::discontinuity;
                            else capture_frame = static_cast<std::int64_t>(device_frame);
                            const audio_callback_info info{capture_frame, callback_flags};
                            const auto *source = flags & AUDCLNT_BUFFERFLAGS_SILENT ? silent.data() : reinterpret_cast<const std::byte *>(data);
                            stream.capture(stream.user, source, static_cast<int>(count), info);
                            capture_frame += count;
                            if (FAILED(capture->ReleaseBuffer(count))) { fail(); break; }
                        }
                    }
                }
                if (running) client->Stop();
                // COM objects are released here, on their owning apartment,
                // before CoUninitialize and before the worker is joined.
            } catch (...) {
                stream.state.store(audio_stream_state::failed, std::memory_order_release);
                if (!startup_published) {
                    stream.opened = false;
                    SetEvent(stream.ready_event.value);
                }
            }
        }
        audio_stream_handle *create_stream(audio_context_handle *context, const audio_stream_desc &desc,
                                           audio_render_fn render, audio_capture_fn capture) {
            auto stream = std::make_unique<wasapi_stream>();
            stream->context = as_wasapi_context(context);
            stream->device_id = desc.device_id;
            stream->direction = render ? audio_direction::output : audio_direction::input;
            stream->properties.format = desc.format;
            stream->requested_period = desc.period_frames;
            stream->user = desc.user;
            stream->render = render;
            stream->capture = capture;
            if (!stream->command_event.value || !stream->ready_event.value || !stream->audio_event.value || !stream->exit_event.value) return nullptr;
            stream->worker = std::thread([ptr = stream.get()] { run_stream(*ptr); });
            if (WaitForSingleObject(stream->ready_event.value, INFINITE) != WAIT_OBJECT_0 || !stream->opened) return nullptr;
            return stream.release();
        }
        void require_control_thread(const wasapi_stream &stream) {
            if (stream.worker.get_id() == std::this_thread::get_id())
                throw std::logic_error("audio stream start/stop/destroy is forbidden from its callback");
        }
        void send_command(wasapi_stream &stream, wasapi_command command) {
            require_control_thread(stream);
            if (WaitForSingleObject(stream.exit_event.value, 0) == WAIT_OBJECT_0) {
                stream.started = false;
                return;
            }
            stream.command.store(command, std::memory_order_release);
            const HANDLE events[] = {stream.ready_event.value, stream.exit_event.value};
            if (!SetEvent(stream.command_event.value))
                throw std::runtime_error("WASAPI worker command failed");
            const DWORD result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
            if (result == WAIT_OBJECT_0 + 1) stream.started = false;
            else if (result != WAIT_OBJECT_0) throw std::runtime_error("WASAPI worker command failed");
        }
    }
    wasapi_stream::~wasapi_stream() {
        if (worker.joinable()) {
            if (worker.get_id() == std::this_thread::get_id()) std::terminate();
            if (opened) {
                command.store(wasapi_command::quit, std::memory_order_release);
                SetEvent(command_event.value);
            }
            worker.join();
        }
    }
    audio_stream_handle *wasapi_create_output(audio_context_handle *context, const audio_stream_desc &desc, audio_render_fn callback) {
        return callback ? create_stream(context, desc, callback, nullptr) : nullptr;
    }
    audio_stream_handle *wasapi_create_input(audio_context_handle *context, const audio_stream_desc &desc, audio_capture_fn callback) {
        return callback ? create_stream(context, desc, nullptr, callback) : nullptr;
    }
    void wasapi_destroy_stream(audio_stream_handle *handle) { delete as_wasapi_stream(handle); }
    audio_stream_properties wasapi_properties(const audio_stream_handle *handle) { return as_wasapi_stream(handle)->properties; }
    bool wasapi_start(audio_stream_handle *handle) {
        auto &stream = *as_wasapi_stream(handle);
        send_command(stream, wasapi_command::start);
        return stream.started;
    }
    void wasapi_stop(audio_stream_handle *handle) { send_command(*as_wasapi_stream(handle), wasapi_command::stop); }
    audio_stream_state wasapi_state(const audio_stream_handle *handle) { return as_wasapi_stream(handle)->state.load(std::memory_order_acquire); }
    std::int64_t wasapi_position(const audio_stream_handle *handle) { return as_wasapi_stream(handle)->position.load(std::memory_order_acquire); }
}
#endif
