#ifndef ALIA_OS_BACKEND_INTERFACE_HPP
#define ALIA_OS_BACKEND_INTERFACE_HPP

#include "../core/backend_op.hpp"
#include "../io/keyboard.hpp"
#include "window.hpp"
#include "monitor.hpp"
#include "dialog.hpp"

namespace alia {
template<class Signature> using os_backend_op = backend_op<Signature>;
struct os_context_handle {};
struct os_window_handle {};
struct os_cursor_handle {};
struct os_text_log_handle {};
enum class os_backend { auto_, win32, x11, null };

// One table per OS context. All calls, including event polling and destruction,
// belong to the creating thread. Contexts outlive their windows, cursors and logs.
// Null slots describe absent platform services and carry reason_unsupported.
struct os_backend_interface {
    os_backend id = os_backend::null;
    os_backend_op<void(os_context_handle *)> destroy_context;
    os_backend_op<void(os_context_handle *)> poll;
    os_backend_op<os_window_handle *(os_context_handle *, vec2i, const window_options &)> create_window;
    os_backend_op<void(os_window_handle *)> destroy_window;
    os_backend_op<void(os_window_handle *, window *)> window_set_owner;
    os_backend_op<vec2i(const os_window_handle *)> window_size;
    os_backend_op<vec2i(const os_window_handle *)> window_position;
    os_backend_op<cstring_view(const os_window_handle *)> window_title;
    os_backend_op<bool(const os_window_handle *)> window_is_focused;
    os_backend_op<bool(const os_window_handle *)> window_is_minimized;
    os_backend_op<bool(const os_window_handle *)> window_is_maximized;
    os_backend_op<alia::window_fullscreen_mode(const os_window_handle *)> window_fullscreen_mode;
    os_backend_op<bool(const os_window_handle *)> window_is_resizable;
    os_backend_op<bool(const os_window_handle *)> window_is_borderless;
    os_backend_op<bool(const os_window_handle *)> window_generates_expose_events;
    os_backend_op<int(const os_window_handle *)> window_refresh_rate;
    os_backend_op<int(const os_window_handle *)> window_monitor;
    os_backend_op<window_size_constraints(const os_window_handle *)> window_constraints;
    os_backend_op<void(os_window_handle *, cstring_view)> window_set_title;
    os_backend_op<void(os_window_handle *, vec2i)> window_set_position;
    os_backend_op<void(os_window_handle *, vec2i)> window_resize;
    os_backend_op<void(os_window_handle *)> window_minimize;
    os_backend_op<void(os_window_handle *)> window_maximize;
    os_backend_op<void(os_window_handle *)> window_restore;
    // Exclusive mode is exact: selection/fallback belongs to L3.
    os_backend_op<void(os_window_handle *, alia::window_fullscreen_mode, const display_mode *)> window_set_fullscreen_mode;
    os_backend_op<void(os_window_handle *, bool)> window_set_resizable;
    os_backend_op<void(os_window_handle *, bool)> window_set_borderless;
    os_backend_op<void(os_window_handle *, window_size_constraints)> window_set_constraints;
    os_backend_op<void(os_window_handle *, bool)> window_apply_constraints;
    os_backend_op<void(os_window_handle *, std::span<const any_bitmap_view>)> window_set_icons;
    os_backend_op<void(os_window_handle *, bool)> window_show_cursor;
    os_backend_op<bool(os_window_handle *, system_mouse_cursor)> window_set_system_cursor;
    os_backend_op<bool(os_window_handle *, std::shared_ptr<const detail::mouse_cursor_impl>)> window_set_cursor;
    os_backend_op<bool(os_window_handle *, vec2i)> window_set_cursor_position;
    os_backend_op<bool(const os_window_handle *)> window_is_cursor_visible;
    os_backend_op<bool(os_window_handle *, mouse_mode)> window_set_mouse_mode;
    os_backend_op<mouse_mode(const os_window_handle *)> window_requested_mouse_mode;
    os_backend_op<mouse_mode(const os_window_handle *)> window_active_mouse_mode;
    os_backend_op<event_source &(os_window_handle *)> window_event_source;
    // X11 native_handle is the Window XID encoded as a pointer-sized integer;
    // native_display is its borrowed Display*. Win32 returns HWND and nullptr.
    os_backend_op<void *(const os_window_handle *)> window_native_handle;
    os_backend_op<void *(os_context_handle *)> native_display;
    os_backend_op<std::vector<monitor_info>(os_context_handle *)> monitors;
    os_backend_op<std::vector<display_mode>(os_context_handle *, int)> display_modes;
    os_backend_op<display_mode(os_context_handle *, int)> desktop_mode;
    os_backend_op<bool(os_context_handle *, bool)> inhibit_screensaver;
    os_backend_op<bool(os_context_handle *)> clipboard_has_text;
    os_backend_op<std::string(os_context_handle *)> clipboard_get_text;
    os_backend_op<bool(os_context_handle *, std::string_view)> clipboard_set_text;
    os_backend_op<bool(os_context_handle *)> clipboard_has_image;
    os_backend_op<void(os_context_handle *)> clipboard_clear;
    os_backend_op<key_led(os_context_handle *)> keyboard_leds;
    os_backend_op<void(os_context_handle *, key_led)> set_keyboard_leds;
    os_backend_op<unsigned(os_context_handle *)> mouse_num_axes;
    os_backend_op<unsigned(os_context_handle *)> mouse_num_buttons;
    os_backend_op<std::optional<vec2i>(os_context_handle *)> mouse_cursor_position;
    os_backend_op<bool(os_context_handle *)> ungrab_mouse;
    os_backend_op<os_cursor_handle *(os_context_handle *, const any_bitmap_view &, vec2i)> create_cursor;
    os_backend_op<void(os_cursor_handle *)> destroy_cursor;
    os_backend_op<void *(const os_cursor_handle *)> cursor_native_handle;
    os_backend_op<std::optional<std::string>(os_context_handle *, const window *, const char *, const file_dialog_options &)> open_file_dialog;
    os_backend_op<std::vector<std::string>(os_context_handle *, const window *, const char *, const file_dialog_options &)> open_files_dialog;
    os_backend_op<std::optional<std::string>(os_context_handle *, const window *, const char *, const file_dialog_options &)> save_file_dialog;
    os_backend_op<std::optional<std::string>(os_context_handle *, const window *, const char *, const char *)> folder_dialog;
    os_backend_op<message_box_result(os_context_handle *, const char *, const char *, message_box_buttons, message_box_icon, const window *)> message_box;
    os_backend_op<os_text_log_handle *(os_context_handle *, const char *, bool)> create_text_log;
    os_backend_op<void(os_text_log_handle *)> destroy_text_log;
    os_backend_op<void(os_text_log_handle *, std::string_view)> text_log_append;
    os_backend_op<void(os_text_log_handle *)> text_log_clear;
    os_backend_op<void(os_text_log_handle *, bool)> text_log_show;
    os_backend_op<bool(const os_text_log_handle *)> text_log_is_visible;
    os_backend_op<event_source &(os_text_log_handle *)> text_log_event_source;
};
struct created_os_context {
    os_context_handle *handle = nullptr;
    os_backend_interface iface;
};
struct os_backend_factory {
    os_backend id;
    created_os_context (*create)();
};
// Thread-safe registration. An existing id is replaced.
void register_os_backend(os_backend_factory);
}
#endif
