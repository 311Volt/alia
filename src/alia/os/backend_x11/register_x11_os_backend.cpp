#include "x11_ops.hpp"
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_X11
#include <X11/keysym.h>
#include <algorithm>
#include <cstdlib>
#include <fcntl.h>
#include <mutex>
#include <unistd.h>

namespace alia {
x11_context::~x11_context() {
    if (!display) return;
    if (inhibited && screensaver) XScreenSaverSuspend(display, False);
    if (input_method) XCloseIM(input_method);
    if (clipboard_window) XDestroyWindow(display, clipboard_window);
    XCloseDisplay(display);
}
key_led x11_keyboard_leds(os_context_handle *h) {
    auto &c = *as_x11_context(h); key_led result = key_led::none;
    for (auto entry : {std::pair{"Num Lock", key_led::num_lock}, {"Caps Lock", key_led::caps_lock}, {"Scroll Lock", key_led::scroll_lock}}) {
        Bool enabled = False;
        if (XkbGetNamedIndicator(c.display, c.atom(entry.first), nullptr, &enabled, nullptr, nullptr) && enabled) result |= entry.second;
    }
    return result;
}
void x11_set_keyboard_leds(os_context_handle *h, key_led leds) {
    auto &c = *as_x11_context(h);
    for (auto entry : {std::pair{XK_Num_Lock, key_led::num_lock}, {XK_Caps_Lock, key_led::caps_lock}, {XK_Scroll_Lock, key_led::scroll_lock}}) {
        const auto mask = XkbKeysymToModifiers(c.display, entry.first);
        const bool enabled = (leds & entry.second) != key_led::none;
        if (mask) XkbLockModifiers(c.display, XkbUseCoreKbd, mask, enabled ? mask : 0);
    }
    XFlush(c.display);
}
namespace {
std::string find_zenity() {
    const char *environment_path = std::getenv("PATH");
    if (!environment_path) return {};
    std::string_view path(environment_path);
    for (;;) {
        const auto separator = path.find(':'); auto part = path.substr(0, separator);
        std::string candidate = (part.empty() ? "." : std::string(part)) + "/zenity";
        if (access(candidate.c_str(), X_OK) == 0) return candidate;
        if (separator == path.npos) break;
        path.remove_prefix(separator + 1);
    }
    return {};
}
created_os_context create_x11_context() {
    // XInitThreads must precede any connection opened by this backend. Each
    // context is still confined to its creating thread.
    static const bool initialized = [] { return XInitThreads() != 0; }();
    if (!initialized) return {};
    auto context = std::make_unique<x11_context>(); auto &c = *context;
    c.display = XOpenDisplay(nullptr); if (!c.display) return {};
    x11_install_error_handler();
    fcntl(ConnectionNumber(c.display), F_SETFD, FD_CLOEXEC);
    c.screen = DefaultScreen(c.display); c.root = RootWindow(c.display, c.screen);
    c.wm_delete = c.atom("WM_DELETE_WINDOW"); c.wm_protocols = c.atom("WM_PROTOCOLS"); c.utf8 = c.atom("UTF8_STRING");
    c.clipboard = c.atom("CLIPBOARD"); c.targets = c.atom("TARGETS"); c.incr = c.atom("INCR"); c.selection_property = c.atom("ALIA_SELECTION");
    c.clipboard_window = XCreateSimpleWindow(c.display, c.root, 0, 0, 1, 1, 0, 0, 0);
    if (!c.clipboard_window) return {};
    XSelectInput(c.display, c.clipboard_window, PropertyChangeMask);
    XSetLocaleModifiers(""); c.input_method = XOpenIM(c.display, nullptr, nullptr, nullptr);
    int event_base = 0, error_base = 0, major = 1, minor = 3;
    c.randr = XRRQueryExtension(c.display, &c.randr_event, &error_base) && XRRQueryVersion(c.display, &major, &minor) && (major > 1 || (major == 1 && minor >= 3));
    if (c.randr) XRRSelectInput(c.display, c.root, RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask | RROutputChangeNotifyMask);
    major = 1; minor = 1;
    c.screensaver = XScreenSaverQueryExtension(c.display, &event_base, &error_base) && XScreenSaverQueryVersion(c.display, &major, &minor) && (major > 1 || (major == 1 && minor >= 1));
    major = 2; minor = 1;
    if (XQueryExtension(c.display, "XInputExtension", &c.xi_opcode, &event_base, &error_base) && XIQueryVersion(c.display, &major, &minor) == Success && (major > 2 || (major == 2 && minor >= 1))) {
        unsigned char mask[XIMaskLen(XI_RawMotion)]{}; XISetMask(mask, XI_RawMotion);
        XIEventMask selection{XIAllMasterDevices, static_cast<int>(sizeof(mask)), mask};
        c.raw_motion = XISelectEvents(c.display, c.root, &selection, 1) == Success;
    }
    major = XkbMajorVersion; minor = XkbMinorVersion;
    int opcode = 0;
    c.xkb = XkbQueryExtension(c.display, &opcode, &c.xkb_event, &error_base, &major, &minor);
    if (c.xkb) {
        Bool supported; XkbSetDetectableAutoRepeat(c.display, True, &supported);
        c.num_lock_mask = XkbKeysymToModifiers(c.display, XK_Num_Lock); c.scroll_lock_mask = XkbKeysymToModifiers(c.display, XK_Scroll_Lock);
        XkbSelectEvents(c.display, XkbUseCoreKbd, XkbNewKeyboardNotifyMask | XkbMapNotifyMask, XkbNewKeyboardNotifyMask | XkbMapNotifyMask);
    }
    x11_update_keymap(c);
    c.zenity_path = find_zenity();
    os_backend_interface ops; ops.id = os_backend::x11;
    ops.destroy_context.operation = [](os_context_handle *h) { delete as_x11_context(h); };
    ops.poll.operation = x11_poll; ops.create_window.operation = x11_create_window;
    ops.destroy_window.operation = [](os_window_handle *h) { delete as_x11_window(h); };
    ops.window_set_owner.operation = [](os_window_handle *h, window *owner) { auto &w = *as_x11_window(h); w.owner = owner; if (w.focused) detail::set_current_window(owner); };
    ops.window_size.operation = [](const os_window_handle *h) -> vec2i { const auto &w = *as_x11_window(h); return w.client_size; };
    ops.window_position.operation = [](const os_window_handle *h) -> vec2i { const auto &w = *as_x11_window(h); return w.client_position; };
    ops.window_title.operation = [](const os_window_handle *h) -> cstring_view { const auto &w = *as_x11_window(h); return cstring_view(w.title.c_str()); };
    ops.window_is_focused.operation = [](const os_window_handle *h) -> bool { const auto &w = *as_x11_window(h); return w.focused; };
    ops.window_is_minimized.operation = [](const os_window_handle *h) -> bool { const auto &w = *as_x11_window(h); return w.minimized; };
    ops.window_is_maximized.operation = [](const os_window_handle *h) -> bool { const auto &w = *as_x11_window(h); return w.maximized; };
    ops.window_fullscreen_mode.operation = [](const os_window_handle *h) -> alia::window_fullscreen_mode { const auto &w = *as_x11_window(h); return w.mode; };
    ops.window_is_resizable.operation = [](const os_window_handle *h) -> bool { const auto &w = *as_x11_window(h); return w.resizable; };
    ops.window_is_borderless.operation = [](const os_window_handle *h) -> bool { const auto &w = *as_x11_window(h); return w.borderless; };
    ops.window_generates_expose_events.operation = [](const os_window_handle *h) -> bool { const auto &w = *as_x11_window(h); return w.expose; };
    ops.window_refresh_rate.operation = [](const os_window_handle *h) -> int { const auto &w = *as_x11_window(h); return x11_desktop_mode(w.context, x11_window_monitor(h)).refresh_rate; };
    ops.window_monitor.operation = [](const os_window_handle *h) -> int { const auto &w = *as_x11_window(h); return x11_window_monitor(h); };
    ops.window_constraints.operation = [](const os_window_handle *h) -> window_size_constraints { const auto &w = *as_x11_window(h); return w.constraints; };
    ops.window_is_cursor_visible.operation = [](const os_window_handle *h) -> bool { const auto &w = *as_x11_window(h); return w.cursor_visible; };
    ops.window_requested_mouse_mode.operation = [](const os_window_handle *h) -> mouse_mode { const auto &w = *as_x11_window(h); return w.requested; };
    ops.window_active_mouse_mode.operation = [](const os_window_handle *h) -> mouse_mode { const auto &w = *as_x11_window(h); return w.active; };
    ops.window_native_handle.operation = [](const os_window_handle *h) -> void * { const auto &w = *as_x11_window(h); return reinterpret_cast<void *>(static_cast<std::uintptr_t>(w.xid)); };
    ops.window_set_title.operation = [](os_window_handle *h, cstring_view title) { x11_set_title(*as_x11_window(h), title); };
    ops.window_set_position.operation = [](os_window_handle *h, vec2i position) { x11_set_position(*as_x11_window(h), position); };
    ops.window_resize.operation = [](os_window_handle *h, vec2i size) { x11_resize(*as_x11_window(h), size); };
    ops.window_minimize.operation = [](os_window_handle *h) { auto &w = *as_x11_window(h); XIconifyWindow(w.context->display, w.xid, w.context->screen); XFlush(w.context->display); };
    if (x11_supports(c, "_NET_WM_STATE_MAXIMIZED_HORZ") && x11_supports(c, "_NET_WM_STATE_MAXIMIZED_VERT")) {
        ops.window_maximize.operation = [](os_window_handle *h) { x11_state(*as_x11_window(h), true, "_NET_WM_STATE_MAXIMIZED_HORZ", "_NET_WM_STATE_MAXIMIZED_VERT"); };
    } else ops.window_maximize.reason_unsupported = "X11 window manager does not support EWMH maximizing";
    ops.window_restore.operation = [](os_window_handle *h) {
        auto &w = *as_x11_window(h); XMapRaised(w.context->display, w.xid);
        if (w.maximized) x11_state(w, false, "_NET_WM_STATE_MAXIMIZED_HORZ", "_NET_WM_STATE_MAXIMIZED_VERT");
        XFlush(w.context->display);
    };
    if (x11_supports(c, "_NET_WM_STATE_FULLSCREEN")) ops.window_set_fullscreen_mode.operation = x11_set_fullscreen;
    else ops.window_set_fullscreen_mode.reason_unsupported = "X11 window manager has no EWMH fullscreen support";
    ops.window_set_resizable.operation = [](os_window_handle *h, bool value) { auto &w = *as_x11_window(h); w.fixed_size = w.client_size; w.resizable = value; x11_update_hints(w); };
    ops.window_set_borderless.operation = [](os_window_handle *h, bool value) { x11_set_borderless(*as_x11_window(h), value); };
    ops.window_set_constraints.operation = [](os_window_handle *h, window_size_constraints value) { x11_validate_constraints(value); auto &w = *as_x11_window(h); w.constraints = value; x11_update_hints(w); };
    ops.window_apply_constraints.operation = [](os_window_handle *h, bool value) { auto &w = *as_x11_window(h); w.constraints_enabled = value; x11_update_hints(w); };
    ops.window_set_icons.operation = x11_set_icons;
    ops.window_show_cursor.operation = [](os_window_handle *h, bool show) { auto &w = *as_x11_window(h); w.cursor_visible = show; x11_update_cursor(w); };
    ops.window_set_system_cursor.operation = x11_set_system_cursor;
    ops.window_set_cursor.operation = [](os_window_handle *h, std::shared_ptr<const detail::mouse_cursor_impl> cursor) {
        if (!cursor || !cursor->handle) return false;
        auto &w = *as_x11_window(h); w.custom_cursor = std::move(cursor); x11_update_cursor(w); return true;
    };
    ops.window_set_cursor_position.operation = x11_set_cursor_position;
    ops.window_set_mouse_mode.operation = x11_set_mouse_mode;
    ops.window_event_source.operation = [](os_window_handle *h) -> event_source & { return as_x11_window(h)->source; };
    ops.native_display.operation = [](os_context_handle *h) -> void * { return as_x11_context(h)->display; };
    ops.monitors.operation = x11_monitors; ops.display_modes.operation = x11_display_modes; ops.desktop_mode.operation = x11_desktop_mode;
    if (c.screensaver) ops.inhibit_screensaver.operation = [](os_context_handle *h, bool inhibit) {
        auto &c = *as_x11_context(h); XScreenSaverSuspend(c.display, inhibit ? True : False); c.inhibited = inhibit; XFlush(c.display); return true;
    }; else ops.inhibit_screensaver.reason_unsupported = "X11 server has no MIT-SCREEN-SAVER 1.1 suspension service";
    ops.clipboard_has_text.operation = x11_clipboard_has_text; ops.clipboard_get_text.operation = x11_clipboard_get_text;
    ops.clipboard_set_text.operation = x11_clipboard_set_text; ops.clipboard_has_image.operation = x11_clipboard_has_image; ops.clipboard_clear.operation = x11_clipboard_clear;
    if (c.xkb) { ops.keyboard_leds.operation = x11_keyboard_leds; ops.set_keyboard_leds.operation = x11_set_keyboard_leds; }
    else { ops.keyboard_leds.reason_unsupported = "X11 server has no XKB keyboard service"; ops.set_keyboard_leds.reason_unsupported = ops.keyboard_leds.reason_unsupported; }
    ops.mouse_num_axes.operation = [](os_context_handle *) { return 4u; }; ops.mouse_num_buttons.operation = x11_mouse_num_buttons;
    ops.mouse_cursor_position.operation = x11_mouse_cursor_position; ops.ungrab_mouse.operation = x11_ungrab_mouse;
    ops.create_cursor.operation = x11_create_cursor; ops.destroy_cursor.operation = x11_destroy_cursor; ops.cursor_native_handle.operation = x11_cursor_native_handle;
    if (!c.zenity_path.empty()) {
        ops.open_file_dialog.operation = x11_open_file_dialog; ops.open_files_dialog.operation = x11_open_files_dialog;
        ops.save_file_dialog.operation = x11_save_file_dialog; ops.folder_dialog.operation = x11_folder_dialog;
    } else {
        const std::string reason = "X11 has no native file-picker protocol and the zenity desktop dialog service is unavailable";
        ops.open_file_dialog.reason_unsupported = reason; ops.open_files_dialog.reason_unsupported = reason;
        ops.save_file_dialog.reason_unsupported = reason; ops.folder_dialog.reason_unsupported = reason;
    }
    ops.message_box.operation = x11_message_box; ops.create_text_log.operation = x11_create_text_log;
    ops.destroy_text_log.operation = [](os_text_log_handle *h) { delete as_x11_log(h); };
    ops.text_log_append.operation = [](os_text_log_handle *h, std::string_view text) { auto &log = *as_x11_log(h); log.text.append(text); if (log.visible) x11_draw_text_log(log); };
    ops.text_log_clear.operation = [](os_text_log_handle *h) { auto &log = *as_x11_log(h); log.text.clear(); if (log.visible) x11_draw_text_log(log); };
    ops.text_log_show.operation = [](os_text_log_handle *h, bool show) { auto &log = *as_x11_log(h); if (!log.xid) return; if (show) XMapRaised(log.context->display, log.xid); else XUnmapWindow(log.context->display, log.xid); log.visible = show; XFlush(log.context->display); };
    ops.text_log_is_visible.operation = [](const os_text_log_handle *h) { return as_x11_log(h)->visible; };
    ops.text_log_event_source.operation = [](os_text_log_handle *h) -> event_source & { return as_x11_log(h)->source; };
    return {context.release(), std::move(ops)};
}
}
void register_x11_os_backend() { register_os_backend({os_backend::x11, create_x11_context}); }
}
#endif
