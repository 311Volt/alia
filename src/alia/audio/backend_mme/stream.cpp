#include "mme_ops.hpp"
#ifdef ALIA_COMPILE_AUDIO_BACKEND_MME
#include "../sample_conversions.hpp"
#include <algorithm>
#include <limits>

namespace alia::detail {
    namespace {
        void run_stream(mme_stream &stream) noexcept {
            struct signal_exit {
                HANDLE event;

                ~signal_exit() {
                    SetEvent(event);
                }
            } on_exit{stream.exit_event.value};

            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
            const bool output = stream.direction == audio_direction::output;
            const int period = stream.properties.period_frames;
            const int stride = stream.properties.format.frame.bytes_per_frame();
            bool running = false, quitting = false;
            std::size_t next = 0;
            std::int64_t callback_frame = 0, epoch_base = 0;
            std::uint64_t clock_units = 0;
            DWORD last_clock = 0, clock_type = 0, last_progress = GetTickCount();
            audio_callback_flags pending_flags = audio_callback_flags::none;
            const double timeout_ms = (std::max)(5000.0, stream.properties.latency_seconds * 4000.0);
            const DWORD stall_timeout = static_cast<DWORD>((std::min)(timeout_ms, double(MAXDWORD / 2)));
            auto update_position = [&]() noexcept {
                MMTIME time{};
                time.wType = TIME_SAMPLES;
                const MMRESULT result = output ? waveOutGetPosition(stream.output, &time, sizeof(time))
                                               : waveInGetPosition(stream.input, &time, sizeof(time));
                if (result != MMSYSERR_NOERROR)
                    return false;
                DWORD raw = 0;
                if (time.wType == TIME_SAMPLES)
                    raw = time.u.sample;
                else if (time.wType == TIME_BYTES)
                    raw = time.u.cb;
                else if (time.wType == TIME_MS)
                    raw = time.u.ms;
                else
                    return false;
                if (clock_type && clock_type != time.wType)
                    return false;
                clock_type = time.wType;
                // The MME counters wrap at 32 bits, including TIME_BYTES.
                clock_units += static_cast<DWORD>(raw - last_clock);
                last_clock = raw;
                const auto frames = time.wType == TIME_BYTES ? clock_units / stride
                                    : time.wType == TIME_MS  ? clock_units * stream.properties.format.sample_rate / 1000
                                                             : clock_units;
                const auto position = epoch_base + static_cast<std::int64_t>(frames);
                if (position > stream.position.load(std::memory_order_relaxed)) {
                    stream.position.store(position, std::memory_order_release);
                    last_progress = GetTickCount();
                }
                return true;
            };
            auto reset = [&]() noexcept {
                const MMRESULT result = output ? waveOutReset(stream.output) : waveInReset(stream.input);
                epoch_base = stream.position.load(std::memory_order_relaxed);
                if (!output)
                    callback_frame = (std::max)(callback_frame, epoch_base);
                clock_units = 0;
                last_clock = clock_type = 0;
                next = 0;
                return result == MMSYSERR_NOERROR;
            };
            auto fail = [&]() noexcept {
                running = false;
                stream.state.store(audio_stream_state::failed, std::memory_order_release);
                reset();
            };
            auto queue = [&](WAVEHDR &header) noexcept {
                header.dwBytesRecorded = 0;
                return (output ? waveOutWrite(stream.output, &header, sizeof(header))
                               : waveInAddBuffer(stream.input, &header, sizeof(header))) == MMSYSERR_NOERROR;
            };
            const HANDLE events[] = {stream.command_event.value, stream.audio_event.value};
            while (!quitting) {
                const DWORD wake = WaitForMultipleObjects(2, events, FALSE, running ? 100 : INFINITE);
                if (wake == WAIT_OBJECT_0) {
                    const auto command = stream.command.exchange(mme_command::none, std::memory_order_acq_rel);
                    if (command == mme_command::start) {
                        stream.started = false;
                        if (stream.state.load(std::memory_order_acquire) != audio_stream_state::failed) {
                            bool ok = true;
                            if (!running) {
                                if (output)
                                    ok = waveOutPause(stream.output) == MMSYSERR_NOERROR;
                                for (auto &header : stream.headers) {
                                    if (!ok)
                                        break;
                                    if (output) {
                                        silence_audio_bytes(
                                            reinterpret_cast<std::byte *>(header.lpData),
                                            period,
                                            stream.properties.format.frame
                                        );
                                        callback_frame += period;
                                    }
                                    ok = queue(header);
                                }
                                if (ok)
                                    ok = (output ? waveOutRestart(stream.output) : waveInStart(stream.input)) ==
                                         MMSYSERR_NOERROR;
                                if (ok)
                                    last_progress = GetTickCount();
                            }
                            if (!ok)
                                fail();
                            else {
                                running = true;
                                stream.started = true;
                                stream.state.store(audio_stream_state::running, std::memory_order_release);
                            }
                        }
                    } else if (command == mme_command::stop || command == mme_command::quit) {
                        if (running) {
                            const bool clock_ok = update_position();
                            running = false;
                            if (!reset() || !clock_ok)
                                fail();
                            pending_flags = audio_callback_flags::discontinuity;
                        }
                        if (stream.state.load(std::memory_order_acquire) != audio_stream_state::failed)
                            stream.state.store(audio_stream_state::stopped, std::memory_order_release);
                        quitting = command == mme_command::quit;
                    }
                    SetEvent(stream.ready_event.value);
                    continue;
                }
                if (wake != WAIT_OBJECT_0 + 1 && wake != WAIT_TIMEOUT) {
                    fail();
                    break;
                }
                if (!running)
                    continue;
                if (!update_position() || static_cast<DWORD>(GetTickCount() - last_progress) > stall_timeout) {
                    fail();
                    continue;
                }
                // Process in submission order even when several completions
                // coalesce into a single event notification.
                bool exhausted = true;
                for (const auto &header : stream.headers)
                    if (!(header.dwFlags & WHDR_DONE))
                        exhausted = false;
                if (exhausted)
                    pending_flags = pending_flags | audio_callback_flags::discontinuity |
                                    (output ? audio_callback_flags::underflow : audio_callback_flags::overflow);
                for (std::size_t i = 0; i < stream.buffer_count && running; ++i) {
                    if (stream.command.load(std::memory_order_acquire) != mme_command::none)
                        break;
                    auto &header = stream.headers[next];
                    if (!(header.dwFlags & WHDR_DONE))
                        break;
                    if (output) {
                        const audio_callback_info info{callback_frame, pending_flags};
                        stream.render(stream.user, reinterpret_cast<std::byte *>(header.lpData), period, info);
                        callback_frame += period;
                        pending_flags = audio_callback_flags::none;
                    } else {
                        if (header.dwBytesRecorded > header.dwBufferLength || header.dwBytesRecorded % stride) {
                            fail();
                            break;
                        }
                        const int count = static_cast<int>(header.dwBytesRecorded / stride);
                        if (count) {
                            const audio_callback_info info{callback_frame, pending_flags};
                            stream.capture(
                                stream.user, reinterpret_cast<const std::byte *>(header.lpData), count, info
                            );
                            callback_frame += count;
                            pending_flags = audio_callback_flags::none;
                        }
                    }
                    if (!queue(header)) {
                        fail();
                        break;
                    }
                    next = (next + 1) % stream.buffer_count;
                }
            }
            if (running)
                reset();
        }

        void require_control_thread(const mme_stream &stream) {
            if (stream.worker.get_id() == std::this_thread::get_id())
                throw std::logic_error("audio stream start/stop/destroy is forbidden from its callback");
        }

        void send_command(mme_stream &stream, mme_command command) {
            require_control_thread(stream);
            if (WaitForSingleObject(stream.exit_event.value, 0) == WAIT_OBJECT_0) {
                stream.started = false;
                return;
            }
            stream.command.store(command, std::memory_order_release);
            if (!SetEvent(stream.command_event.value))
                throw std::runtime_error("MME worker command failed");
            const HANDLE events[] = {stream.ready_event.value, stream.exit_event.value};
            const DWORD result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
            if (result == WAIT_OBJECT_0 + 1)
                stream.started = false;
            else if (result != WAIT_OBJECT_0)
                throw std::runtime_error("MME worker command failed");
        }

        audio_stream_handle *create_stream(
            audio_context_handle *context, const audio_stream_desc &desc, audio_render_fn render,
            audio_capture_fn capture
        ) {
            const auto direction = render ? audio_direction::output : audio_direction::input;
            UINT device = 0;
            WAVEFORMATEXTENSIBLE wave;
            if (desc.period_frames < 0 || !mme_device_id(direction, desc.device_id, device) ||
                !mme_wave_format(desc.format, wave))
                return nullptr;
            auto stream = std::make_unique<mme_stream>();
            stream->context = as_mme_context(context);
            stream->direction = direction;
            stream->user = desc.user;
            stream->render = render;
            stream->capture = capture;
            const int period = desc.period_frames ? desc.period_frames : (std::max)(1, desc.format.sample_rate / 100);
            const auto bytes = audio_byte_count(desc.format.frame, period);
            if (bytes > (std::numeric_limits<DWORD>::max)() ||
                bytes > (std::numeric_limits<std::size_t>::max)() / stream->buffer_count)
                return nullptr;
            stream->properties = {
                desc.format, period, period, double(period) * stream->buffer_count / desc.format.sample_rate
            };
            stream->data = std::make_unique<std::byte[]>(bytes * stream->buffer_count);
            if (!stream->command_event.value || !stream->ready_event.value || !stream->audio_event.value ||
                !stream->exit_event.value)
                return nullptr;
            const auto event = reinterpret_cast<DWORD_PTR>(stream->audio_event.value);
            const MMRESULT opened = render
                                        ? waveOutOpen(&stream->output, device, &wave.Format, event, 0, CALLBACK_EVENT)
                                        : waveInOpen(&stream->input, device, &wave.Format, event, 0, CALLBACK_EVENT);
            if (opened != MMSYSERR_NOERROR)
                return nullptr;
            for (std::size_t i = 0; i < stream->buffer_count; ++i) {
                auto &header = stream->headers[i];
                header.lpData = reinterpret_cast<LPSTR>(stream->data.get() + i * bytes);
                header.dwBufferLength = static_cast<DWORD>(bytes);
                const MMRESULT prepared = render ? waveOutPrepareHeader(stream->output, &header, sizeof(header))
                                                 : waveInPrepareHeader(stream->input, &header, sizeof(header));
                if (prepared != MMSYSERR_NOERROR)
                    return nullptr;
            }
            stream->worker = std::thread([ptr = stream.get()] { run_stream(*ptr); });
            return stream.release();
        }
    } // namespace

    mme_stream::~mme_stream() {
        if (worker.joinable()) {
            if (worker.get_id() == std::this_thread::get_id())
                std::terminate();
            command.store(mme_command::quit, std::memory_order_release);
            SetEvent(command_event.value);
            worker.join();
        }
        if (output) {
            waveOutReset(output);
            for (auto &header : headers)
                if (header.dwFlags & WHDR_PREPARED)
                    waveOutUnprepareHeader(output, &header, sizeof(header));
            waveOutClose(output);
        }
        if (input) {
            waveInReset(input);
            for (auto &header : headers)
                if (header.dwFlags & WHDR_PREPARED)
                    waveInUnprepareHeader(input, &header, sizeof(header));
            waveInClose(input);
        }
    }

    audio_stream_handle *
    mme_create_output(audio_context_handle *context, const audio_stream_desc &desc, audio_render_fn callback) {
        return callback ? create_stream(context, desc, callback, nullptr) : nullptr;
    }

    audio_stream_handle *
    mme_create_input(audio_context_handle *context, const audio_stream_desc &desc, audio_capture_fn callback) {
        return callback ? create_stream(context, desc, nullptr, callback) : nullptr;
    }

    void mme_destroy_stream(audio_stream_handle *p) {
        require_control_thread(*as_mme_stream(p));
        delete as_mme_stream(p);
    }

    audio_stream_properties mme_properties(const audio_stream_handle *p) {
        return as_mme_stream(p)->properties;
    }

    bool mme_start(audio_stream_handle *p) {
        auto &stream = *as_mme_stream(p);
        send_command(stream, mme_command::start);
        return stream.started;
    }

    void mme_stop(audio_stream_handle *p) {
        send_command(*as_mme_stream(p), mme_command::stop);
    }

    audio_stream_state mme_state(const audio_stream_handle *p) {
        return as_mme_stream(p)->state.load(std::memory_order_acquire);
    }

    std::int64_t mme_position(const audio_stream_handle *p) {
        return as_mme_stream(p)->position.load(std::memory_order_acquire);
    }
} // namespace alia::detail
#endif
