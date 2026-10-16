#ifndef ALIA_AUDIO_WASAPI_OPS_HPP
#define ALIA_AUDIO_WASAPI_OPS_HPP

#ifdef ALIA_COMPILE_AUDIO_BACKEND_WASAPI
// Request modern declarations in these translation units only; all optional
// post-XP entry points are loaded dynamically.
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <propsys.h>

#include "../audio_backend_interface.hpp"
#include <atomic>
#include <memory>
#include <thread>
#include <utility>

namespace alia::detail {
    template <class T>
    class wasapi_com_ptr {
    public:
        wasapi_com_ptr() = default;

        ~wasapi_com_ptr() {
            if (ptr_)
                ptr_->Release();
        }

        wasapi_com_ptr(const wasapi_com_ptr &) = delete;
        wasapi_com_ptr &operator=(const wasapi_com_ptr &) = delete;

        T *get() const noexcept {
            return ptr_;
        }

        T *operator->() const noexcept {
            return ptr_;
        }

        explicit operator bool() const noexcept {
            return ptr_ != nullptr;
        }

        T **put() noexcept {
            if (ptr_)
                ptr_->Release();
            ptr_ = nullptr;
            return &ptr_;
        }

    private:
        T *ptr_ = nullptr;
    };

    class wasapi_com_scope {
    public:
        explicit wasapi_com_scope(DWORD mode = COINIT_APARTMENTTHREADED) noexcept
            : result_(CoInitializeEx(nullptr, mode)) {
        }

        ~wasapi_com_scope() {
            if (SUCCEEDED(result_))
                CoUninitialize();
        }

        bool usable() const noexcept {
            return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
        }

    private:
        HRESULT result_;
    };

    struct wasapi_task_free {
        void operator()(void *ptr) const noexcept {
            CoTaskMemFree(ptr);
        }
    };
    template <class T>
    using wasapi_task_ptr = std::unique_ptr<T, wasapi_task_free>;

    struct wasapi_event {
        HANDLE value = nullptr;

        explicit wasapi_event(bool manual = false) : value(CreateEventW(nullptr, manual, FALSE, nullptr)) {
        }

        ~wasapi_event() {
            if (value)
                CloseHandle(value);
        }

        wasapi_event(const wasapi_event &) = delete;
        wasapi_event &operator=(const wasapi_event &) = delete;
    };

    // Local GUIDs avoid imports from uuid, ksuser, or propsys.
    inline constexpr GUID wasapi_enumerator_class{
        0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}
    };
    inline constexpr GUID wasapi_enumerator_iid{
        0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}
    };
    inline constexpr GUID wasapi_endpoint_iid{
        0x1be09788, 0x6894, 0x4089, {0x85, 0x86, 0x9a, 0x2a, 0x6c, 0x26, 0x5a, 0xc5}
    };
    inline constexpr GUID wasapi_client_iid{
        0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}
    };
    inline constexpr GUID wasapi_render_iid{
        0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2}
    };
    inline constexpr GUID wasapi_capture_iid{
        0xc8adbd64, 0xe71e, 0x48a0, {0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17}
    };
    inline constexpr GUID wasapi_clock_iid{
        0xcd63314f, 0x3fba, 0x4a1b, {0x81, 0x2c, 0xef, 0x96, 0x35, 0x87, 0x28, 0xe7}
    };
    inline constexpr GUID wasapi_pcm_subtype{1, 0, 0x0010, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
    inline constexpr GUID wasapi_float_subtype{3, 0, 0x0010, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
    inline constexpr PROPERTYKEY wasapi_friendly_name{
        {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14
    };

    struct wasapi_context : audio_context_handle {
        HMODULE avrt = nullptr;
        HANDLE(WINAPI *set_mm_thread)(LPCWSTR, LPDWORD) = nullptr;
        BOOL(WINAPI *revert_mm_thread)(HANDLE) = nullptr;

        ~wasapi_context() {
            if (avrt)
                FreeLibrary(avrt);
        }
    };
    enum class wasapi_command {
        none,
        start,
        stop,
        quit
    };

    // Shared-mode, event-driven stream. Its worker thread owns an MTA apartment
    // and every COM object; control calls reach it through command/ready events.
    // The worker registers with MMCSS when the context loaded avrt.dll.
    struct wasapi_stream : audio_stream_handle {
        wasapi_context *context = nullptr;
        std::string device_id;
        audio_direction direction{};
        audio_stream_properties properties{};
        int requested_period = 0;
        void *user = nullptr;
        audio_render_fn render = nullptr;
        audio_capture_fn capture = nullptr;
        wasapi_event command_event, ready_event, audio_event;
        wasapi_event exit_event{true};
        std::atomic<wasapi_command> command{wasapi_command::none};
        std::atomic<audio_stream_state> state{audio_stream_state::stopped};
        std::atomic<std::int64_t> position{0};
        bool opened = false, started = false; // Published through ready_event.
        std::thread worker;
        ~wasapi_stream();
    };

    inline wasapi_context *as_wasapi_context(audio_context_handle *p) {
        return static_cast<wasapi_context *>(p);
    }

    inline wasapi_stream *as_wasapi_stream(audio_stream_handle *p) {
        return static_cast<wasapi_stream *>(p);
    }

    inline const wasapi_stream *as_wasapi_stream(const audio_stream_handle *p) {
        return static_cast<const wasapi_stream *>(p);
    }

    bool wasapi_enumerator(wasapi_com_ptr<IMMDeviceEnumerator> &);
    bool wasapi_endpoint(IMMDeviceEnumerator *, audio_direction, std::string_view, wasapi_com_ptr<IMMDevice> &);
    bool wasapi_wave_format(const audio_stream_format &, WAVEFORMATEXTENSIBLE &);
    bool wasapi_initialize(IMMDevice *, const audio_stream_format &, int period_frames, wasapi_com_ptr<IAudioClient> &);
    std::vector<audio_device_info> wasapi_enumerate_devices(audio_context_handle *, audio_direction);
    bool
    wasapi_format_supported(audio_context_handle *, audio_direction, std::string_view, const audio_stream_format &);
    audio_stream_handle *wasapi_create_output(audio_context_handle *, const audio_stream_desc &, audio_render_fn);
    audio_stream_handle *wasapi_create_input(audio_context_handle *, const audio_stream_desc &, audio_capture_fn);
    void wasapi_destroy_stream(audio_stream_handle *);
    audio_stream_properties wasapi_properties(const audio_stream_handle *);
    bool wasapi_start(audio_stream_handle *);
    void wasapi_stop(audio_stream_handle *);
    audio_stream_state wasapi_state(const audio_stream_handle *);
    std::int64_t wasapi_position(const audio_stream_handle *);
} // namespace alia::detail
#endif
#endif
