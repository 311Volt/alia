#ifndef ALIA_OS_WIN32_OPS_HPP
#define ALIA_OS_WIN32_OPS_HPP
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../os_backend_interface.hpp"
#include "../../io/mouse_impl.hpp"
#include <string>

namespace alia {
    struct win32_context : os_context_handle {};

    struct win32_cursor : os_cursor_handle {
        HCURSOR cursor = nullptr;
    };

    class win32_window : public os_window_handle {
    public:
        ~win32_window();

        HWND hwnd = nullptr;
        window *owner = nullptr;
        event_source source;
        vec2i client_size = {};
        std::string title_utf8;
        bool cursor_visible = true;
        system_mouse_cursor system_cursor_ = system_mouse_cursor::default_cursor;
        std::shared_ptr<const detail::mouse_cursor_impl> custom_cursor_;
        bool tracking_mouse_leave_ = false;
        std::uint32_t pressed_mouse_buttons_ = 0;
        vec2i last_mouse_position_{};
        detail::mouse_warp_filter pending_warps_;
        key last_key_down = key::unknown;
        WCHAR high_surrogate = 0;
        bool suppress_altgr_control_up = false;

        void set_owner(window *value) noexcept;

        vec2i size() const;
        cstring_view title() const;
        vec2i position() const;
        bool is_focused() const;
        bool is_minimized() const;
        bool is_maximized() const;
        window_fullscreen_mode fullscreen_mode() const;
        bool is_resizable() const;
        bool is_borderless() const;
        bool generates_expose_events() const;
        int refresh_rate() const;
        int monitor() const;
        window_size_constraints size_constraints() const;

        void set_title(cstring_view value);
        void set_position(vec2i value);
        void resize(vec2i value);
        void minimize();
        void maximize();
        void restore();

        void set_fullscreen_mode(window_fullscreen_mode requested, const display_mode *exclusive_mode);

        void set_resizable(bool value);
        void set_borderless(bool value);
        void set_size_constraints(window_size_constraints value);
        void apply_size_constraints(bool enable);
        void set_icons(std::span<const any_bitmap_view> icons);

        void show_cursor(bool show);
        bool set_cursor(system_mouse_cursor value);
        bool set_cursor(std::shared_ptr<const detail::mouse_cursor_impl> value);
        bool set_cursor_position(vec2i value);
        bool is_cursor_visible() const;
        bool set_mouse_mode(mouse_mode mode);
        mouse_mode requested_mouse_mode() const;
        mouse_mode active_mouse_mode() const;
        void *native_handle() const;

        HCURSOR selected_cursor() const;

        void apply_cursor() const;

        bool begin_mouse_tracking(vec2i position);

        bool suppress_warp_message(vec2i position);

        void handle_mouse_button(mouse_button button, bool down, vec2i position);

        void release_mouse_buttons();

        static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
        static bool ensure_class_registered();

        DWORD windowed_style() const;

        DWORD style_with_runtime(DWORD base) const;

        void apply_windowed_style_preserving_client();

        void restore_windowed_placement();

        void apply_display_mode(int monitor_index, const display_mode &mode);

        void restore_display_mode() noexcept;

        bool reapply_display_mode() noexcept;

        window_fullscreen_mode mode_ = window_fullscreen_mode::windowed;
        bool resizable_ = false;
        bool borderless_ = false;
        bool expose_ = false;
        int monitor_opt_ = -1;
        display_mode active_mode_{};
        std::wstring mode_device_;
        bool mode_applied_ = false;
        bool mode_suspended_ = false;
        bool activation_transition_ = false;
        WINDOWPLACEMENT saved_placement_{sizeof(WINDOWPLACEMENT)};
        window_size_constraints constraints_{};
        bool constraints_enabled_ = false;
        HICON big_icon_ = nullptr;
        HICON small_icon_ = nullptr;
    };

    struct win32_text_log : os_text_log_handle {
        HWND hwnd = nullptr;
        HWND edit_hwnd = nullptr;
        event_source events;
        bool monospace = true;
        std::wstring title;
        HFONT font = nullptr;

        win32_text_log(const char *title_str, bool mono);

        ~win32_text_log();

        static void register_class();

        static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

        void append(const char *text);

        void clear();
    };

    inline win32_window *as_win32_window(os_window_handle *h) {
        return static_cast<win32_window *>(h);
    }

    inline const win32_window *as_win32_window(const os_window_handle *h) {
        return static_cast<const win32_window *>(h);
    }

    inline win32_text_log *as_win32_log(os_text_log_handle *h) {
        return static_cast<win32_text_log *>(h);
    }

    inline const win32_text_log *as_win32_log(const os_text_log_handle *h) {
        return static_cast<const win32_text_log *>(h);
    }

    os_window_handle *win32_create_window(os_context_handle *, vec2i, const window_options &);
    bool win32_inhibit_screensaver(os_context_handle *, bool);
    std::vector<monitor_info> win32_monitors(os_context_handle *);
    std::vector<display_mode> win32_display_modes(os_context_handle *, int);
    display_mode win32_desktop_mode(os_context_handle *, int);
    key_led win32_keyboard_leds(os_context_handle *);
    void win32_set_keyboard_leds(os_context_handle *, key_led);
    std::optional<std::string>
    win32_show_open_file_dialog(os_context_handle *, const window *, const char *, const file_dialog_options &);
    std::vector<std::string>
    win32_show_open_files_dialog(os_context_handle *, const window *, const char *, const file_dialog_options &);
    std::optional<std::string>
    win32_show_save_file_dialog(os_context_handle *, const window *, const char *, const file_dialog_options &);
    std::optional<std::string>
    win32_show_folder_dialog(os_context_handle *, const window *, const char *, const char *);
    message_box_result
    win32_message_box(os_context_handle *, const char *, const char *, message_box_buttons, message_box_icon, const window *);

    namespace detail {
        void *win32_monitor_handle(int);
        int win32_monitor_index(void *);
        std::wstring win32_monitor_device_name(int);
        os_cursor_handle *win32_create_cursor(os_context_handle *, const any_bitmap_view &, vec2i);
        void win32_destroy_cursor(os_cursor_handle *);
        void *win32_cursor_native_handle(const os_cursor_handle *);
        unsigned win32_platform_mouse_num_axes();
        unsigned win32_platform_mouse_num_buttons();
        bool win32_platform_can_get_mouse_cursor_position();
        std::optional<vec2i> win32_platform_get_mouse_cursor_position();
        bool win32_platform_ungrab_mouse();

        bool win32_platform_set_mouse_mode(
            void *owner_token, void *native_window, event_source &source, mouse_mode requested
        );
        mouse_mode win32_platform_requested_mouse_mode(void *owner_token);
        mouse_mode win32_platform_active_mouse_mode(void *owner_token);
        void win32_platform_mouse_window_focus_changed(void *owner_token, bool focused, bool minimized);
        void win32_platform_refresh_mouse_bounds(void *owner_token);
        void win32_platform_mouse_window_destroyed(void *owner_token);
        bool win32_platform_mouse_handle_raw_input(void *owner_token, void *raw_input);

    } // namespace detail
} // namespace alia
#endif
#endif
