#ifndef ALIA_OS_X11_OPS_HPP
#define ALIA_OS_X11_OPS_HPP
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_X11
#include "../os_backend_interface.hpp"
#include "../../io/mouse_impl.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/scrnsaver.h>
#include <array>
#include <chrono>
#include <unordered_map>
#include <utility>

namespace alia {
    struct x11_window;
    struct x11_text_log;

    // Expected X errors (e.g. a clipboard peer closing mid-transfer) are contained
    // within an operation. Unrelated errors retain the application's Xlib handler.
    class x11_error_scope {
    public:
        explicit x11_error_scope(Display *);
        x11_error_scope(const x11_error_scope &) = delete;
        x11_error_scope &operator=(const x11_error_scope &) = delete;
        ~x11_error_scope();
        int sync();
        Display *display;
        unsigned long first_request;
        int error = 0;
        x11_error_scope *previous;
    };

    void x11_install_error_handler();

    struct x11_transfer {
        ::Window requestor;
        Atom property, target;
        std::string data;
        std::size_t offset = 0;
        std::chrono::steady_clock::time_point deadline;
    };

    struct x11_context : os_context_handle {
        Display *display = nullptr;
        int screen = 0;
        ::Window root = None, clipboard_window = None;
        XIM input_method = nullptr;
        bool randr = false, raw_motion = false, screensaver = false, inhibited = false;
        int xi_opcode = 0, randr_event = 0, xkb_event = 0;
        bool xkb = false;
        std::array<key, 256> keymap{};
        vec2i saved_screen_size{}, saved_screen_mm{}, expanded_screen_size{};
        Atom wm_delete = None, wm_protocols = None, utf8 = None;
        Atom clipboard = None, targets = None, incr = None, selection_property = None;
        unsigned num_lock_mask = 0, scroll_lock_mask = 0;
        std::string clipboard_text, zenity_path;
        std::vector<x11_transfer> transfers;
        std::unordered_map<::Window, x11_window *> windows;
        std::unordered_map<::Window, x11_text_log *> logs;
        x11_window *mouse_owner = nullptr;
        ~x11_context();

        Atom atom(const char *name) const {
            return XInternAtom(display, name, False);
        }
    };

    struct x11_window : os_window_handle {
        x11_context *context = nullptr;
        ::Window xid = None;
        window *owner = nullptr;
        event_source source;
        XIC input_context = nullptr;
        vec2i client_size{}, fixed_size{}, client_position{}, saved_position{}, saved_size{};
        std::string title;
        bool focused = false, minimized = false, maximized = false, mapped = false;
        bool resizable = false, borderless = false, expose = false, cursor_visible = true;
        bool constraints_enabled = true;
        window_size_constraints constraints{};
        alia::window_fullscreen_mode mode = window_fullscreen_mode::windowed;
        int preferred_monitor = -1;
        Cursor cursor = None, hidden_cursor = None;
        std::shared_ptr<const detail::mouse_cursor_impl> custom_cursor;
        mouse_mode requested = mouse_mode::normal, active = mouse_mode::normal;
        vec2i mouse_position{};
        std::uint32_t buttons = 0;
        std::array<bool, 256> keys{};
        detail::mouse_warp_filter warps;
        double raw_x = 0, raw_y = 0;
        RRCrtc exclusive_crtc = None;
        RRMode saved_mode = None;
        Rotation saved_rotation = RR_Rotate_0;
        vec2i saved_crtc_position{};
        std::vector<RROutput> saved_outputs;
        bool display_mode_applied = false, display_mode_suspended = false;
        display_mode exclusive_mode{};
        ~x11_window();
    };

    struct x11_cursor : os_cursor_handle {
        x11_context *context;
        Cursor cursor;
    };

    struct x11_text_log : os_text_log_handle {
        x11_context *context = nullptr;
        ::Window xid = None;
        event_source source;
        GC gc = nullptr;
        XFontSet font = nullptr;
        std::string text;
        bool visible = false;
        int width = 600, height = 400;
        ~x11_text_log();
    };

    inline x11_context *as_x11_context(os_context_handle *h) {
        return static_cast<x11_context *>(h);
    }

    inline x11_window *as_x11_window(os_window_handle *h) {
        return static_cast<x11_window *>(h);
    }

    inline const x11_window *as_x11_window(const os_window_handle *h) {
        return static_cast<const x11_window *>(h);
    }

    inline x11_text_log *as_x11_log(os_text_log_handle *h) {
        return static_cast<x11_text_log *>(h);
    }

    inline const x11_text_log *as_x11_log(const os_text_log_handle *h) {
        return static_cast<const x11_text_log *>(h);
    }

    void x11_poll(os_context_handle *);
    void x11_update_keymap(x11_context &);
    void x11_dispatch(x11_context &, XEvent &);
    bool x11_selection_event(x11_context &, XEvent &);
    os_window_handle *x11_create_window(os_context_handle *, vec2i, const window_options &);
    void x11_set_title(x11_window &, cstring_view);
    void x11_set_position(x11_window &, vec2i);
    void x11_resize(x11_window &, vec2i);
    void x11_state(x11_window &, bool add, const char *first, const char *second = nullptr);
    bool x11_supports(const x11_context &, const char *);
    void x11_set_fullscreen(os_window_handle *, alia::window_fullscreen_mode, const display_mode *);
    void x11_restore_display(x11_window &) noexcept;
    void x11_apply_display(x11_window &, const display_mode &);
    void x11_update_hints(x11_window &);
    void x11_validate_constraints(window_size_constraints);
    void x11_set_borderless(x11_window &, bool);
    void x11_set_icons(os_window_handle *, std::span<const any_bitmap_view>);
    void x11_update_cursor(x11_window &);
    bool x11_set_system_cursor(os_window_handle *, system_mouse_cursor);
    bool x11_set_mouse_mode(os_window_handle *, mouse_mode);
    void x11_refresh_mouse(x11_window &);
    bool x11_ungrab_mouse(os_context_handle *);
    bool x11_set_cursor_position(os_window_handle *, vec2i);
    os_cursor_handle *x11_create_cursor(os_context_handle *, const any_bitmap_view &, vec2i);
    void x11_destroy_cursor(os_cursor_handle *);
    void *x11_cursor_native_handle(const os_cursor_handle *);
    unsigned x11_mouse_num_buttons(os_context_handle *);
    std::optional<vec2i> x11_mouse_cursor_position(os_context_handle *);
    int x11_window_monitor(const os_window_handle *);
    std::vector<monitor_info> x11_monitors(os_context_handle *);
    std::vector<display_mode> x11_display_modes(os_context_handle *, int);
    display_mode x11_desktop_mode(os_context_handle *, int);
    bool x11_clipboard_has_text(os_context_handle *);
    bool x11_clipboard_has_image(os_context_handle *);
    std::string x11_clipboard_get_text(os_context_handle *);
    bool x11_clipboard_set_text(os_context_handle *, std::string_view);
    void x11_clipboard_clear(os_context_handle *);
    key_led x11_keyboard_leds(os_context_handle *);
    void x11_set_keyboard_leds(os_context_handle *, key_led);
    message_box_result
    x11_message_box(os_context_handle *, const char *, const char *, message_box_buttons, message_box_icon, const window *);
    std::optional<std::string>
    x11_open_file_dialog(os_context_handle *, const window *, const char *, const file_dialog_options &);
    std::vector<std::string>
    x11_open_files_dialog(os_context_handle *, const window *, const char *, const file_dialog_options &);
    std::optional<std::string>
    x11_save_file_dialog(os_context_handle *, const window *, const char *, const file_dialog_options &);
    std::optional<std::string> x11_folder_dialog(os_context_handle *, const window *, const char *, const char *);
    os_text_log_handle *x11_create_text_log(os_context_handle *, const char *, bool);
    void x11_draw_text_log(x11_text_log &);
} // namespace alia
#endif
#endif
