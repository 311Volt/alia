#include "win32_ops.hpp"
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
#include <cstring>
#include <climits>
namespace alia {
namespace {
created_os_context create_win32_context() {
    auto context = std::make_unique<win32_context>();
    os_backend_interface ops;
    ops.id = os_backend::win32;
    ops.destroy_context.operation = [](os_context_handle *h) { delete static_cast<win32_context *>(h); };
    ops.poll.operation = [](os_context_handle *) { MSG message; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); } };
    ops.create_window.operation = win32_create_window;
    ops.destroy_window.operation = [](os_window_handle *h) { delete as_win32_window(h); };
    ops.native_display.operation = [](os_context_handle *) -> void * { return nullptr; };
    ops.window_size.operation = [](const os_window_handle *h) -> vec2i { return as_win32_window(h)->size(); };
    ops.window_position.operation = [](const os_window_handle *h) -> vec2i { return as_win32_window(h)->position(); };
    ops.window_title.operation = [](const os_window_handle *h) -> cstring_view { return as_win32_window(h)->title(); };
    ops.window_is_focused.operation = [](const os_window_handle *h) -> bool { return as_win32_window(h)->is_focused(); };
    ops.window_is_minimized.operation = [](const os_window_handle *h) -> bool { return as_win32_window(h)->is_minimized(); };
    ops.window_is_maximized.operation = [](const os_window_handle *h) -> bool { return as_win32_window(h)->is_maximized(); };
    ops.window_fullscreen_mode.operation = [](const os_window_handle *h) -> alia::window_fullscreen_mode { return as_win32_window(h)->fullscreen_mode(); };
    ops.window_is_resizable.operation = [](const os_window_handle *h) -> bool { return as_win32_window(h)->is_resizable(); };
    ops.window_is_borderless.operation = [](const os_window_handle *h) -> bool { return as_win32_window(h)->is_borderless(); };
    ops.window_generates_expose_events.operation = [](const os_window_handle *h) -> bool { return as_win32_window(h)->generates_expose_events(); };
    ops.window_refresh_rate.operation = [](const os_window_handle *h) -> int { return as_win32_window(h)->refresh_rate(); };
    ops.window_monitor.operation = [](const os_window_handle *h) -> int { return as_win32_window(h)->monitor(); };
    ops.window_constraints.operation = [](const os_window_handle *h) -> window_size_constraints { return as_win32_window(h)->size_constraints(); };
    ops.window_is_cursor_visible.operation = [](const os_window_handle *h) -> bool { return as_win32_window(h)->is_cursor_visible(); };
    ops.window_requested_mouse_mode.operation = [](const os_window_handle *h) -> mouse_mode { return as_win32_window(h)->requested_mouse_mode(); };
    ops.window_active_mouse_mode.operation = [](const os_window_handle *h) -> mouse_mode { return as_win32_window(h)->active_mouse_mode(); };
    ops.window_native_handle.operation = [](const os_window_handle *h) -> void * { return as_win32_window(h)->native_handle(); };
    ops.window_set_owner.operation = [](os_window_handle *h, window * value) { as_win32_window(h)->set_owner(value); };
    ops.window_set_title.operation = [](os_window_handle *h, cstring_view value) { as_win32_window(h)->set_title(value); };
    ops.window_set_position.operation = [](os_window_handle *h, vec2i value) { as_win32_window(h)->set_position(value); };
    ops.window_resize.operation = [](os_window_handle *h, vec2i value) { as_win32_window(h)->resize(value); };
    ops.window_set_resizable.operation = [](os_window_handle *h, bool value) { as_win32_window(h)->set_resizable(value); };
    ops.window_set_borderless.operation = [](os_window_handle *h, bool value) { as_win32_window(h)->set_borderless(value); };
    ops.window_set_constraints.operation = [](os_window_handle *h, window_size_constraints value) { as_win32_window(h)->set_size_constraints(value); };
    ops.window_apply_constraints.operation = [](os_window_handle *h, bool value) { as_win32_window(h)->apply_size_constraints(value); };
    ops.window_set_icons.operation = [](os_window_handle *h, std::span<const any_bitmap_view> value) { as_win32_window(h)->set_icons(value); };
    ops.window_show_cursor.operation = [](os_window_handle *h, bool value) { as_win32_window(h)->show_cursor(value); };
    ops.window_minimize.operation = [](os_window_handle *h) { as_win32_window(h)->minimize(); };
    ops.window_maximize.operation = [](os_window_handle *h) { as_win32_window(h)->maximize(); };
    ops.window_restore.operation = [](os_window_handle *h) { as_win32_window(h)->restore(); };
    ops.window_set_system_cursor.operation = [](os_window_handle *h, system_mouse_cursor value) { return as_win32_window(h)->set_cursor(value); };
    ops.window_set_cursor.operation = [](os_window_handle *h, std::shared_ptr<const detail::mouse_cursor_impl> value) { return as_win32_window(h)->set_cursor(value); };
    ops.window_set_cursor_position.operation = [](os_window_handle *h, vec2i value) { return as_win32_window(h)->set_cursor_position(value); };
    ops.window_set_mouse_mode.operation = [](os_window_handle *h, mouse_mode value) { return as_win32_window(h)->set_mouse_mode(value); };
    ops.window_set_fullscreen_mode.operation = [](os_window_handle *h, alia::window_fullscreen_mode mode, const display_mode *d) { as_win32_window(h)->set_fullscreen_mode(mode, d); };
    ops.window_event_source.operation = [](os_window_handle *h) -> event_source & { return as_win32_window(h)->source; };
    ops.monitors.operation = win32_monitors;
    ops.display_modes.operation = win32_display_modes;
    ops.desktop_mode.operation = win32_desktop_mode;
    ops.inhibit_screensaver.operation = win32_inhibit_screensaver;
    ops.keyboard_leds.operation = win32_keyboard_leds;
    ops.set_keyboard_leds.operation = win32_set_keyboard_leds;
    ops.mouse_num_axes.operation = [](os_context_handle *) { return detail::win32_platform_mouse_num_axes(); };
    ops.mouse_num_buttons.operation = [](os_context_handle *) { return detail::win32_platform_mouse_num_buttons(); };
    ops.mouse_cursor_position.operation = [](os_context_handle *) { return detail::win32_platform_get_mouse_cursor_position(); };
    ops.ungrab_mouse.operation = [](os_context_handle *) { return detail::win32_platform_ungrab_mouse(); };
    ops.create_cursor.operation = detail::win32_create_cursor;
    ops.destroy_cursor.operation = detail::win32_destroy_cursor;
    ops.cursor_native_handle.operation = detail::win32_cursor_native_handle;
    ops.open_file_dialog.operation = win32_show_open_file_dialog;
    ops.open_files_dialog.operation = win32_show_open_files_dialog;
    ops.save_file_dialog.operation = win32_show_save_file_dialog;
    ops.folder_dialog.operation = win32_show_folder_dialog;
    ops.message_box.operation = win32_message_box;
    ops.create_text_log.operation = [](os_context_handle *, const char *title, bool mono) -> os_text_log_handle * { return new win32_text_log(title, mono); };
    ops.destroy_text_log.operation = [](os_text_log_handle *h) { delete as_win32_log(h); };
    ops.text_log_append.operation = [](os_text_log_handle *h, std::string_view text) { std::string copy(text); as_win32_log(h)->append(copy.c_str()); };
    ops.text_log_clear.operation = [](os_text_log_handle *h) { as_win32_log(h)->clear(); };
    ops.text_log_show.operation = [](os_text_log_handle *h, bool show) { ShowWindow(as_win32_log(h)->hwnd, show ? SW_SHOW : SW_HIDE); };
    ops.text_log_is_visible.operation = [](const os_text_log_handle *h) { return IsWindowVisible(as_win32_log(h)->hwnd) != FALSE; };
    ops.text_log_event_source.operation = [](os_text_log_handle *h) -> event_source & { return as_win32_log(h)->events; };
    ops.clipboard_has_text.operation = [](os_context_handle *) { return IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE; };
    ops.clipboard_has_image.operation = [](os_context_handle *) { return IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_BITMAP); };
    ops.clipboard_get_text.operation = [](os_context_handle *) -> std::string {
        if (!OpenClipboard(nullptr)) return {};
        struct close_clipboard { ~close_clipboard() { CloseClipboard(); } } closer;
        HANDLE data = GetClipboardData(CF_UNICODETEXT);
        if (!data) return {};
        const auto *text = static_cast<const wchar_t *>(GlobalLock(data));
        if (!text) return {};
        struct unlock { HANDLE data; ~unlock() { GlobalUnlock(data); } } lock{data};
        int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        if (n <= 1) return {};
        std::string result(static_cast<std::size_t>(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), n, nullptr, nullptr);
        result.pop_back(); return result;
    };
    ops.clipboard_set_text.operation = [](os_context_handle *, std::string_view text) {
        if (text.size() > static_cast<std::size_t>(INT_MAX)) return false;
        int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (!text.empty() && !n) return false;
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (static_cast<std::size_t>(n) + 1) * sizeof(wchar_t));
        if (!memory) return false;
        auto *destination = static_cast<wchar_t *>(GlobalLock(memory));
        if (!destination) { GlobalFree(memory); return false; }
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), destination, n);
        destination[n] = 0; GlobalUnlock(memory);
        // SetClipboardData needs an owner HWND, rather than OpenClipboard(NULL).
        HWND owner = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!owner) { GlobalFree(memory); return false; }
        bool ok = false;
        if (OpenClipboard(owner)) { if (EmptyClipboard()) ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr; CloseClipboard(); }
        DestroyWindow(owner);
        if (!ok) GlobalFree(memory);
        return ok;
    };
    ops.clipboard_clear.operation = [](os_context_handle *) { if (OpenClipboard(nullptr)) { EmptyClipboard(); CloseClipboard(); } };
    return {context.release(), std::move(ops)};
}
}
void register_win32_os_backend() { register_os_backend({os_backend::win32, create_win32_context}); }
}
#endif
