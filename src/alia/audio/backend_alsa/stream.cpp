#include "alsa_ops.hpp"
#ifdef ALIA_COMPILE_AUDIO_BACKEND_ALSA
#include "../sample_conversions.hpp"
#include <sys/eventfd.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>

namespace alia::detail {
    namespace {
        bool wake_worker(alsa_stream &stream) noexcept {
            const std::uint64_t value = 1;
            ssize_t result;
            do {
                result = write(stream.command_fd, &value, sizeof(value));
            } while (result < 0 && errno == EINTR);
            return result == static_cast<ssize_t>(sizeof(value)) || (result < 0 && errno == EAGAIN);
        }

        void acknowledge(alsa_stream &stream) noexcept {
            stream.acknowledgement.fetch_add(1, std::memory_order_release);
            stream.acknowledgement.notify_all();
        }

        void map_channels(alsa_stream &stream, int count, bool output) noexcept {
            if (!stream.mapped_buffer)
                return;
            const int channels = stream.properties.format.frame.channels;
            const auto point_bytes = stream.properties.format.frame.point.bytes;
            const auto stride = stream.properties.format.frame.bytes_per_frame();
            for (int frame = 0; frame < count; ++frame) {
                const auto offset = static_cast<std::size_t>(frame) * stride;
                for (int channel = 0; channel < channels; ++channel) {
                    auto *user = stream.buffer.get() + offset + channel * point_bytes;
                    auto *device = stream.mapped_buffer.get() + offset + stream.channel_map[channel] * point_bytes;
                    if (output)
                        std::memcpy(device, user, point_bytes);
                    else
                        std::memcpy(user, device, point_bytes);
                }
            }
        }

        void run_stream(alsa_stream &stream) noexcept {
            struct signal_exit {
                alsa_stream &stream;

                ~signal_exit() {
                    stream.exited.store(true, std::memory_order_release);
                    acknowledge(stream);
                }
            } on_exit{stream};

            const bool output = stream.direction == audio_direction::output;
            auto *pcm = stream.pcm.get();
            auto *device_buffer = stream.mapped_buffer ? stream.mapped_buffer.get() : stream.buffer.get();
            const int period = stream.properties.period_frames;
            const int stride = stream.properties.format.frame.bytes_per_frame();
            const auto pcm_poll_count = static_cast<unsigned>(stream.poll_descriptors.size() - 1);
            bool running = false, quitting = false;
            std::int64_t callback_frame = 0, epoch_base = 0, transferred = 0;
            int pending = 0, pending_offset = 0;
            audio_callback_flags pending_flags = audio_callback_flags::none;
            auto fail = [&]() noexcept {
                running = false;
                snd_pcm_drop(pcm);
                stream.state.store(audio_stream_state::failed, std::memory_order_release);
            };
            auto update_position = [&](snd_pcm_sframes_t available, snd_pcm_sframes_t delay) noexcept {
                const auto frames = output ? transferred - (std::max<snd_pcm_sframes_t>)(0, delay)
                                           : transferred + (std::max<snd_pcm_sframes_t>)(0, available);
                const auto previous = stream.position.load(std::memory_order_relaxed);
                stream.position.store((std::max)(previous, epoch_base + frames), std::memory_order_release);
            };
            auto begin = [&]() noexcept {
                epoch_base = stream.position.load(std::memory_order_relaxed);
                transferred = 0;
                pending = pending_offset = 0;
                if (snd_pcm_prepare(pcm) < 0)
                    return false;
                if (output) {
                    // Prime with silence while automatic start is disabled.
                    // User callbacks begin only after the explicit start.
                    silence_audio_bytes(device_buffer, period, stream.properties.format.frame);
                    snd_pcm_uframes_t remaining = stream.buffer_frames;
                    while (remaining) {
                        const auto count = (std::min)(remaining, static_cast<snd_pcm_uframes_t>(period));
                        const auto written = snd_pcm_writei(pcm, device_buffer, count);
                        if (written == -EINTR)
                            continue;
                        if (written == -EAGAIN || written == 0)
                            break;
                        if (written < 0 || static_cast<snd_pcm_uframes_t>(written) > count)
                            return false;
                        remaining -= written;
                        transferred += written;
                        callback_frame += written;
                    }
                    if (!transferred)
                        return false;
                } else
                    callback_frame = (std::max)(callback_frame, epoch_base);
                if (snd_pcm_start(pcm) < 0)
                    return false;
                running = true;
                return true;
            };
            auto recover = [&](int error) noexcept {
                if (error == -EAGAIN || error == -EINTR)
                    return;
                // XRUNs retain the endpoint. Device removal, suspension and
                // other backend failures latch failed rather than reopening.
                if (error == -EPIPE) {
                    if (output)
                        stream.position.store(epoch_base + transferred, std::memory_order_release);
                    else
                        callback_frame = (std::max)(callback_frame, stream.position.load(std::memory_order_relaxed));
                    pending_flags = pending_flags | audio_callback_flags::discontinuity |
                                    (output ? audio_callback_flags::underflow : audio_callback_flags::overflow);
                    if (begin())
                        return;
                }
                fail();
            };
            while (!quitting) {
                auto *descriptors = running ? stream.poll_descriptors.data() : &stream.poll_descriptors.back();
                const auto count = running ? stream.poll_descriptors.size() : std::size_t(1);
                const int polled = poll(descriptors, static_cast<nfds_t>(count), running ? 1000 : -1);
                if (polled < 0) {
                    if (errno == EINTR)
                        continue;
                    fail();
                    break;
                }
                if (stream.poll_descriptors.back().revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    fail();
                    break;
                }
                if (stream.poll_descriptors.back().revents & POLLIN) {
                    std::uint64_t value;
                    ssize_t result;
                    do {
                        result = read(stream.command_fd, &value, sizeof(value));
                    } while (result < 0 && errno == EINTR);
                    if (result < 0 && errno != EAGAIN) {
                        fail();
                        break;
                    }
                }
                // Drain the wakeup before taking the command: a stale wakeup
                // must not consume a concurrently posted command's signal.
                const auto command = stream.command.exchange(alsa_command::none, std::memory_order_acq_rel);
                if (command != alsa_command::none) {
                    if (command == alsa_command::start) {
                        stream.started = false;
                        if (stream.state.load(std::memory_order_acquire) != audio_stream_state::failed) {
                            if (!running && !begin())
                                fail();
                            if (running) {
                                stream.started = true;
                                stream.state.store(audio_stream_state::running, std::memory_order_release);
                            }
                        }
                    } else if (command == alsa_command::stop || command == alsa_command::quit) {
                        if (running) {
                            snd_pcm_sframes_t available = 0, delay = 0;
                            const int result = snd_pcm_avail_delay(pcm, &available, &delay);
                            if (result >= 0)
                                update_position(available, delay);
                            else if (result != -EPIPE)
                                fail();
                            running = false;
                            if (snd_pcm_drop(pcm) < 0)
                                fail();
                            pending = pending_offset = 0;
                            pending_flags = audio_callback_flags::discontinuity;
                        }
                        if (stream.state.load(std::memory_order_acquire) != audio_stream_state::failed)
                            stream.state.store(audio_stream_state::stopped, std::memory_order_release);
                        quitting = command == alsa_command::quit;
                    }
                    acknowledge(stream);
                    continue;
                }
                if (!running)
                    continue;
                unsigned short revents = 0;
                if (snd_pcm_poll_descriptors_revents(pcm, stream.poll_descriptors.data(), pcm_poll_count, &revents) <
                        0 ||
                    (revents & (POLLHUP | POLLNVAL)) || snd_pcm_state(pcm) == SND_PCM_STATE_DISCONNECTED) {
                    fail();
                    continue;
                }
                snd_pcm_sframes_t available = 0, delay = 0;
                const int status = snd_pcm_avail_delay(pcm, &available, &delay);
                if (status < 0) {
                    recover(status);
                    continue;
                }
                if (revents & POLLERR) {
                    fail();
                    continue;
                }
                update_position(available, delay);
                if (available <= 0 || stream.command.load(std::memory_order_acquire) != alsa_command::none)
                    continue;
                const int frames = static_cast<int>((std::min)(available, static_cast<snd_pcm_sframes_t>(period)));
                if (output) {
                    if (!pending) {
                        const audio_callback_info info{callback_frame, pending_flags};
                        stream.render(stream.user, stream.buffer.get(), frames, info);
                        map_channels(stream, frames, true);
                        callback_frame += frames;
                        pending = frames;
                        pending_offset = 0;
                        pending_flags = audio_callback_flags::none;
                    }
                    const auto requested = static_cast<snd_pcm_uframes_t>((std::min)(pending, frames));
                    const auto written = snd_pcm_writei(
                        pcm, device_buffer + static_cast<std::size_t>(pending_offset) * stride, requested
                    );
                    if (written < 0) {
                        recover(static_cast<int>(written));
                        continue;
                    }
                    if (static_cast<snd_pcm_uframes_t>(written) > requested) {
                        fail();
                        continue;
                    }
                    pending -= static_cast<int>(written);
                    pending_offset += static_cast<int>(written);
                    transferred += written;
                } else {
                    const auto read_frames = snd_pcm_readi(pcm, device_buffer, static_cast<snd_pcm_uframes_t>(frames));
                    if (read_frames < 0) {
                        recover(static_cast<int>(read_frames));
                        continue;
                    }
                    if (read_frames > frames) {
                        fail();
                        continue;
                    }
                    if (!read_frames)
                        continue;
                    const int captured = static_cast<int>(read_frames);
                    map_channels(stream, captured, false);
                    const audio_callback_info info{callback_frame, pending_flags};
                    stream.capture(stream.user, stream.buffer.get(), captured, info);
                    callback_frame += captured;
                    transferred += captured;
                    pending_flags = audio_callback_flags::none;
                }
            }
            if (running)
                snd_pcm_drop(pcm);
        }

        void require_control_thread(const alsa_stream &stream) {
            if (stream.worker.get_id() == std::this_thread::get_id())
                throw std::logic_error("audio stream start/stop/destroy is forbidden from its callback");
        }

        void send_command(alsa_stream &stream, alsa_command command) {
            require_control_thread(stream);
            const auto previous = stream.acknowledgement.load(std::memory_order_acquire);
            if (stream.exited.load(std::memory_order_acquire)) {
                stream.started = false;
                return;
            }
            stream.command.store(command, std::memory_order_release);
            if (!wake_worker(stream))
                throw std::runtime_error("ALSA worker command failed");
            while (stream.acknowledgement.load(std::memory_order_acquire) == previous)
                stream.acknowledgement.wait(previous, std::memory_order_acquire);
            if (stream.exited.load(std::memory_order_acquire))
                stream.started = false;
        }

        audio_stream_handle *create_stream(
            audio_context_handle *context, const audio_stream_desc &desc, audio_render_fn render,
            audio_capture_fn capture
        ) {
            if (desc.period_frames < 0)
                return nullptr;
            auto stream = std::make_unique<alsa_stream>();
            stream->context = as_alsa_context(context);
            stream->direction = render ? audio_direction::output : audio_direction::input;
            stream->user = desc.user;
            stream->render = render;
            stream->capture = capture;
            if (!alsa_open(stream->direction, desc.device_id, stream->pcm) ||
                !alsa_configure(
                    stream->pcm.get(), desc.format, desc.period_frames, stream->properties, stream->buffer_frames
                ) ||
                !alsa_channel_map(stream->pcm.get(), desc.format.frame.channels, stream->channel_map))
                return nullptr;
            const auto bytes = audio_byte_count(desc.format.frame, stream->properties.max_callback_frames);
            stream->buffer = std::make_unique<std::byte[]>(bytes);
            for (int i = 0; i < desc.format.frame.channels; ++i)
                if (stream->channel_map[i] != static_cast<unsigned>(i)) {
                    stream->mapped_buffer = std::make_unique<std::byte[]>(bytes);
                    break;
                }
            stream->command_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            if (stream->command_fd < 0)
                return nullptr;
            const int descriptors = snd_pcm_poll_descriptors_count(stream->pcm.get());
            if (descriptors <= 0)
                return nullptr;
            stream->poll_descriptors.resize(static_cast<std::size_t>(descriptors) + 1);
            if (snd_pcm_poll_descriptors(
                    stream->pcm.get(), stream->poll_descriptors.data(), static_cast<unsigned>(descriptors)
                ) != descriptors)
                return nullptr;
            stream->poll_descriptors.back() = {stream->command_fd, POLLIN, 0};
            stream->worker = std::thread([ptr = stream.get()] { run_stream(*ptr); });
            return stream.release();
        }
    } // namespace

    alsa_stream::~alsa_stream() {
        if (worker.joinable()) {
            if (worker.get_id() == std::this_thread::get_id())
                std::terminate();
            command.store(alsa_command::quit, std::memory_order_release);
            wake_worker(*this);
            worker.join();
        }
        if (pcm)
            snd_pcm_drop(pcm.get());
        if (command_fd >= 0)
            close(command_fd);
    }

    audio_stream_handle *
    alsa_create_output(audio_context_handle *context, const audio_stream_desc &desc, audio_render_fn callback) {
        return callback ? create_stream(context, desc, callback, nullptr) : nullptr;
    }

    audio_stream_handle *
    alsa_create_input(audio_context_handle *context, const audio_stream_desc &desc, audio_capture_fn callback) {
        return callback ? create_stream(context, desc, nullptr, callback) : nullptr;
    }

    void alsa_destroy_stream(audio_stream_handle *p) {
        require_control_thread(*as_alsa_stream(p));
        delete as_alsa_stream(p);
    }

    audio_stream_properties alsa_properties(const audio_stream_handle *p) {
        return as_alsa_stream(p)->properties;
    }

    bool alsa_start(audio_stream_handle *p) {
        auto &stream = *as_alsa_stream(p);
        send_command(stream, alsa_command::start);
        return stream.started;
    }

    void alsa_stop(audio_stream_handle *p) {
        send_command(*as_alsa_stream(p), alsa_command::stop);
    }

    audio_stream_state alsa_state(const audio_stream_handle *p) {
        return as_alsa_stream(p)->state.load(std::memory_order_acquire);
    }

    std::int64_t alsa_position(const audio_stream_handle *p) {
        return as_alsa_stream(p)->position.load(std::memory_order_acquire);
    }
} // namespace alia::detail
#endif
