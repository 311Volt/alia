#ifndef ALIA_AUDIO_MME_OPS_HPP
#define ALIA_AUDIO_MME_OPS_HPP

#ifdef ALIA_COMPILE_AUDIO_BACKEND_MME
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>

#include "../audio_backend_interface.hpp"
#include <array>
#include <atomic>
#include <memory>
#include <thread>
#include <utility>

namespace alia::detail {
    struct mme_context : audio_context_handle {};

    struct mme_event {
        HANDLE value;

        explicit mme_event(bool manual = false) : value(CreateEventW(nullptr, manual, FALSE, nullptr)) {
        }

        ~mme_event() {
            if (value)
                CloseHandle(value);
        }

        mme_event(const mme_event &) = delete;
        mme_event &operator=(const mme_event &) = delete;
    };
    enum class mme_command {
        none,
        start,
        stop,
        quit
    };

    struct mme_stream : audio_stream_handle {
        mme_context *context = nullptr;
        audio_direction direction{};
        audio_stream_properties properties{};
        HWAVEOUT output = nullptr;
        HWAVEIN input = nullptr;
        void *user = nullptr;
        audio_render_fn render = nullptr;
        audio_capture_fn capture = nullptr;
        // Headers and their data stay at fixed addresses until unprepared.
        static constexpr std::size_t buffer_count = 4;
        std::array<WAVEHDR, buffer_count> headers{};
        std::unique_ptr<std::byte[]> data;
        mme_event command_event, ready_event, audio_event;
        mme_event exit_event{true};
        std::atomic<mme_command> command{mme_command::none};
        std::atomic<audio_stream_state> state{audio_stream_state::stopped};
        std::atomic<std::int64_t> position{0};
        bool started = false; // Published through ready_event.
        std::thread worker;
        ~mme_stream();
    };

    inline mme_context *as_mme_context(audio_context_handle *p) {
        return static_cast<mme_context *>(p);
    }

    inline mme_stream *as_mme_stream(audio_stream_handle *p) {
        return static_cast<mme_stream *>(p);
    }

    inline const mme_stream *as_mme_stream(const audio_stream_handle *p) {
        return static_cast<const mme_stream *>(p);
    }

    bool mme_device_id(audio_direction, std::string_view, UINT &);
    bool mme_wave_format(const audio_stream_format &, WAVEFORMATEXTENSIBLE &);
    std::vector<audio_device_info> mme_enumerate_devices(audio_context_handle *, audio_direction);
    bool mme_format_supported(audio_context_handle *, audio_direction, std::string_view, const audio_stream_format &);
    audio_stream_handle *mme_create_output(audio_context_handle *, const audio_stream_desc &, audio_render_fn);
    audio_stream_handle *mme_create_input(audio_context_handle *, const audio_stream_desc &, audio_capture_fn);
    void mme_destroy_stream(audio_stream_handle *);
    audio_stream_properties mme_properties(const audio_stream_handle *);
    bool mme_start(audio_stream_handle *);
    void mme_stop(audio_stream_handle *);
    audio_stream_state mme_state(const audio_stream_handle *);
    std::int64_t mme_position(const audio_stream_handle *);
} // namespace alia::detail
#endif
#endif
