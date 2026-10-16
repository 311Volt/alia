#ifndef ALIA_AUDIO_ALSA_OPS_HPP
#define ALIA_AUDIO_ALSA_OPS_HPP

#ifdef ALIA_COMPILE_AUDIO_BACKEND_ALSA
#include <alsa/asoundlib.h>
#include <poll.h>
#include "../audio_backend_interface.hpp"
#include <array>
#include <atomic>
#include <memory>
#include <thread>
#include <utility>

namespace alia::detail {
    struct alsa_context : audio_context_handle {};

    struct alsa_pcm_delete {
        void operator()(snd_pcm_t *p) const noexcept {
            if (p)
                snd_pcm_close(p);
        }
    };

    using alsa_pcm_ptr = std::unique_ptr<snd_pcm_t, alsa_pcm_delete>;
    enum class alsa_command {
        none,
        start,
        stop,
        quit
    };

    struct alsa_stream : audio_stream_handle {
        alsa_context *context = nullptr;
        audio_direction direction{};
        audio_stream_properties properties{};
        alsa_pcm_ptr pcm;
        snd_pcm_uframes_t buffer_frames = 0;
        void *user = nullptr;
        audio_render_fn render = nullptr;
        audio_capture_fn capture = nullptr;
        std::unique_ptr<std::byte[]> buffer, mapped_buffer;
        // For each alia channel, the corresponding ALSA channel index.
        std::array<unsigned, 8> channel_map{};
        std::vector<pollfd> poll_descriptors; // Last descriptor is command_fd.
        int command_fd = -1;
        std::atomic<alsa_command> command{alsa_command::none};
        std::atomic<unsigned> acknowledgement{0};
        std::atomic<bool> exited{false};
        std::atomic<audio_stream_state> state{audio_stream_state::stopped};
        std::atomic<std::int64_t> position{0};
        bool started = false; // Published through acknowledgement.
        std::thread worker;
        ~alsa_stream();
    };

    inline alsa_context *as_alsa_context(audio_context_handle *p) {
        return static_cast<alsa_context *>(p);
    }

    inline alsa_stream *as_alsa_stream(audio_stream_handle *p) {
        return static_cast<alsa_stream *>(p);
    }

    inline const alsa_stream *as_alsa_stream(const audio_stream_handle *p) {
        return static_cast<const alsa_stream *>(p);
    }

    snd_pcm_format_t alsa_point_format(sample_point_format);
    bool alsa_open(audio_direction, std::string_view, alsa_pcm_ptr &);
    bool alsa_configure(
        snd_pcm_t *, const audio_stream_format &, int period_frames, audio_stream_properties &,
        snd_pcm_uframes_t &buffer_frames
    );
    bool alsa_channel_map(snd_pcm_t *, int channels, std::array<unsigned, 8> &);
    std::vector<audio_device_info> alsa_enumerate_devices(audio_context_handle *, audio_direction);
    bool alsa_format_supported(audio_context_handle *, audio_direction, std::string_view, const audio_stream_format &);
    audio_stream_handle *alsa_create_output(audio_context_handle *, const audio_stream_desc &, audio_render_fn);
    audio_stream_handle *alsa_create_input(audio_context_handle *, const audio_stream_desc &, audio_capture_fn);
    void alsa_destroy_stream(audio_stream_handle *);
    audio_stream_properties alsa_properties(const audio_stream_handle *);
    bool alsa_start(audio_stream_handle *);
    void alsa_stop(audio_stream_handle *);
    audio_stream_state alsa_state(const audio_stream_handle *);
    std::int64_t alsa_position(const audio_stream_handle *);
} // namespace alia::detail
#endif
#endif
