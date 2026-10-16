#ifndef WINDOW_C9F4D198_C82B_40AC_8414_6D11AB9C6591
#define WINDOW_C9F4D198_C82B_40AC_8414_6D11AB9C6591

#include "../core/vec.hpp"
#include "../core/rect.hpp"
#include "../util/cstring_view.hpp"
#include "../events/event_source.hpp"
#include "window_events.hpp"
#include <memory>
#include <span>
#include <variant>

namespace alia {

class any_bitmap_view;
class window;
class os_context;
struct os_context_handle;
struct os_window_handle;
struct os_backend_interface;
struct display_mode;

namespace detail { struct window_pos_centered_t {}; }
inline constexpr detail::window_pos_centered_t window_pos_centered;

enum class window_fullscreen_mode {
    windowed,
    fullscreen,         // exclusive fullscreen
    fullscreen_window,  // borderless fullscreen
};

struct window_size_constraints {
    vec2i min = {0, 0};
    vec2i max = {0, 0};
};

// window creation/configuration options
struct window_options {
    cstring_view title = "ALIA window";
    std::variant<detail::window_pos_centered_t, vec2i> position = window_pos_centered;
    window_fullscreen_mode mode = window_fullscreen_mode::windowed;
    bool resizable  = false;
    bool maximized  = false;
    bool minimized  = false;
    bool borderless = false; // borderless window (no title bar)
    alia::mouse_mode mouse_mode = alia::mouse_mode::normal;
    bool high_dpi   = false;
    int monitor = -1;
    int refresh_rate = 0;
    bool generate_expose_events = false;
    window_size_constraints size_constraints = {};
};

// Move-only window. Its OS context must outlive it. Calls belong to the
// context's creating thread. Dimensions and constraints are client pixels.
// Moves keep the native window and event source; the backend's owner pointer
// and mouse/current-window tracking follow the new object.
class window {
public:
    window(vec2i size, window_options opts = {});
    window(os_context &, vec2i size, window_options opts = {});
    window(window &&) noexcept;
    window &operator=(window &&) noexcept;
    ~window();
    window(const window &) = delete;
    window &operator=(const window &) = delete;
    [[nodiscard]] int width() const { return size().x; }
    [[nodiscard]] int height() const { return size().y; }
    [[nodiscard]] vec2i size() const;
    [[nodiscard]] rect_i rect() const { return {{0, 0}, size()}; }
    [[nodiscard]] float aspect_ratio() const;
    [[nodiscard]] vec2i position() const;
    [[nodiscard]] cstring_view title() const;
    [[nodiscard]] bool is_focused() const;
    [[nodiscard]] bool is_minimized() const;
    [[nodiscard]] bool is_maximized() const;
    [[nodiscard]] window_fullscreen_mode fullscreen_mode() const;
    [[nodiscard]] bool is_fullscreen() const { return fullscreen_mode() != window_fullscreen_mode::windowed; }
    [[nodiscard]] bool is_fullscreen_window() const { return fullscreen_mode() == window_fullscreen_mode::fullscreen_window; }
    [[nodiscard]] bool is_resizable() const;
    [[nodiscard]] bool is_borderless() const;
    [[nodiscard]] bool generates_expose_events() const;
    [[nodiscard]] int refresh_rate() const;
    [[nodiscard]] int monitor() const;
    [[nodiscard]] window_size_constraints size_constraints() const;
    void set_title(cstring_view);
    void set_position(vec2i);
    void set_size(vec2i value) { resize(value); }
    void resize(vec2i);
    void minimize();
    void maximize();
    void restore();
    void set_fullscreen(bool);
    void set_fullscreen(const display_mode &);
    void set_fullscreen_mode(window_fullscreen_mode);
    void toggle_fullscreen() { set_fullscreen(fullscreen_mode() != window_fullscreen_mode::fullscreen); }
    void set_fullscreen_window(bool value) { set_fullscreen_mode(value ? window_fullscreen_mode::fullscreen_window : window_fullscreen_mode::windowed); }
    void set_resizable(bool);
    void set_borderless(bool);
    void set_size_constraints(window_size_constraints);
    void apply_size_constraints(bool enable = true);
    void set_icon(const any_bitmap_view &);
    void set_icons(std::span<const any_bitmap_view>);
    void show_cursor(bool show = true);
    void hide_cursor() { show_cursor(false); }
    bool set_cursor(system_mouse_cursor);
    bool set_cursor(const mouse_cursor &);
    bool set_cursor_position(vec2i);
    [[nodiscard]] bool is_cursor_visible() const;
    // Requested mode persists while unfocused/minimized; active mode may be normal.
    bool set_mouse_mode(mouse_mode);
    [[nodiscard]] mouse_mode requested_mouse_mode() const;
    [[nodiscard]] mouse_mode active_mouse_mode() const;
    event_source &get_event_source();
    // Pumps all windows in this context, including platform/clipboard events.
    void poll();
    [[nodiscard]] void *native_handle() const;
    [[nodiscard]] void *native_display() const;
    [[nodiscard]] static window *current();
    [[nodiscard]] os_window_handle *handle() const noexcept { return handle_; }
    [[nodiscard]] const os_backend_interface *backend() const noexcept { return backend_; }
    [[nodiscard]] os_context_handle *context_handle() const noexcept { return context_; }
private:
    os_window_handle *handle_ = nullptr;
    os_context_handle *context_ = nullptr;
    const os_backend_interface *backend_ = nullptr;
    int preferred_monitor_ = -1, preferred_refresh_rate_ = 0;
    vec2i windowed_size_{};
    void require_valid() const;
    void release() noexcept;
};
bool inhibit_screensaver(bool inhibit);
namespace detail { void set_current_window(window *) noexcept; }
} // namespace alia
#endif
