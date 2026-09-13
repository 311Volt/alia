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

// ── Implementation interface (internal) ──────────────────────────────

struct window_impl {
    virtual ~window_impl() = default;

    virtual void set_owner(window *owner) noexcept = 0;

    // Properties
    virtual vec2i        size()         const = 0;
    virtual vec2i        position()     const = 0;
    virtual cstring_view title()        const = 0;
    virtual float        aspect_ratio() const = 0;

    // State
    virtual bool is_focused()    const = 0;
    virtual bool is_minimized()  const = 0;
    virtual bool is_maximized()  const = 0;
    virtual window_fullscreen_mode fullscreen_mode() const = 0;
    virtual bool is_resizable() const = 0;
    virtual bool is_borderless() const = 0;
    virtual bool generates_expose_events() const = 0;
    virtual int refresh_rate() const = 0;
    virtual int monitor() const = 0;
    virtual window_size_constraints size_constraints() const = 0;

    // Modification
    virtual void set_title(cstring_view title) = 0;
    virtual void set_position(vec2i pos) = 0;
    virtual void resize(vec2i size) = 0;
    virtual void minimize() = 0;
    virtual void maximize() = 0;
    virtual void restore() = 0;

    // Fullscreen
    virtual void set_fullscreen_mode(
        window_fullscreen_mode mode,
        const display_mode *exclusive_mode) = 0;
    virtual void set_resizable(bool value) = 0;
    virtual void set_borderless(bool value) = 0;
    virtual void set_size_constraints(window_size_constraints value) = 0;
    virtual void apply_size_constraints(bool enable) = 0;
    virtual void set_icons(std::span<const any_bitmap_view> icons) = 0;

    // Cursor
    virtual void show_cursor(bool show) = 0;
    virtual bool set_cursor(system_mouse_cursor cursor) = 0;
    virtual bool set_cursor(
        std::shared_ptr<const detail::mouse_cursor_impl> cursor) = 0;
    virtual bool set_cursor_position(vec2i pos) = 0;
    virtual bool is_cursor_visible() const = 0;

    // Mouse input mode
    virtual bool set_mouse_mode(mouse_mode mode) = 0;
    virtual mouse_mode requested_mouse_mode() const = 0;
    virtual mouse_mode active_mouse_mode() const = 0;

    // Events
    virtual event_source& get_event_source() = 0;

    // Message pump
    virtual void poll() = 0;

    // Native handle (HWND, Window, etc.)
    virtual void* native_handle() const = 0;
};

// ── Backend registry ──────────────────────────────────────────────────

struct window_backend_entry {
    const char* name;
    // Returns nullptr on failure (backend unavailable on this platform)
    std::unique_ptr<window_impl> (*create)(vec2i size, window_options opts);
};

void register_window_backend(window_backend_entry entry);

// ── window class ──────────────────────────────────────────────────────

    class window {
    public:
        // Construction
        window(vec2i size, window_options opts = {});

        // Move only
        window(window &&) noexcept;
        window &operator=(window &&) noexcept;
        ~window();

        window(const window &) = delete;
        window &operator=(const window &) = delete;

        // Properties
        [[nodiscard]] int          width()        const { return impl_->size().x; }
        [[nodiscard]] int          height()       const { return impl_->size().y; }
        [[nodiscard]] vec2i        size()         const { return impl_->size(); }
        [[nodiscard]] rect_i       rect()         const { return rect_i{{0, 0}, size()}; }
        [[nodiscard]] float        aspect_ratio() const { return impl_->aspect_ratio(); }
        [[nodiscard]] vec2i        position()     const { return impl_->position(); }
        [[nodiscard]] cstring_view title()        const { return impl_->title(); }

        // State
        [[nodiscard]] bool is_focused()    const { return impl_->is_focused(); }
        [[nodiscard]] bool is_minimized()  const { return impl_->is_minimized(); }
        [[nodiscard]] bool is_maximized()  const { return impl_->is_maximized(); }
        [[nodiscard]] window_fullscreen_mode fullscreen_mode() const { return impl_->fullscreen_mode(); }
        [[nodiscard]] bool is_fullscreen() const { return fullscreen_mode() != window_fullscreen_mode::windowed; }
        [[nodiscard]] bool is_fullscreen_window() const { return fullscreen_mode() == window_fullscreen_mode::fullscreen_window; }
        [[nodiscard]] bool is_resizable() const { return impl_->is_resizable(); }
        [[nodiscard]] bool is_borderless() const { return impl_->is_borderless(); }
        [[nodiscard]] bool generates_expose_events() const { return impl_->generates_expose_events(); }
        [[nodiscard]] int refresh_rate() const { return impl_->refresh_rate(); }
        [[nodiscard]] int monitor() const { return impl_->monitor(); }
        [[nodiscard]] window_size_constraints size_constraints() const { return impl_->size_constraints(); }

        // Modification
        void set_title(cstring_view title)  { impl_->set_title(title); }
        void set_position(vec2i pos)        { impl_->set_position(pos); }
        void set_size(vec2i sz)             { impl_->resize(sz); }
        void resize(vec2i sz)               { impl_->resize(sz); }

        void minimize() { impl_->minimize(); }
        void maximize() { impl_->maximize(); }
        void restore()  { impl_->restore(); }

        // Fullscreen
        void set_fullscreen(bool fullscreen) {
            impl_->set_fullscreen_mode(
                fullscreen ? window_fullscreen_mode::fullscreen : window_fullscreen_mode::windowed,
                nullptr);
        }
        void set_fullscreen(const display_mode &mode) {
            impl_->set_fullscreen_mode(window_fullscreen_mode::fullscreen, &mode);
        }
        void set_fullscreen_mode(window_fullscreen_mode mode) {
            impl_->set_fullscreen_mode(mode, nullptr);
        }
        void toggle_fullscreen() { set_fullscreen(fullscreen_mode() != window_fullscreen_mode::fullscreen); }
        void set_fullscreen_window(bool enable) {
            impl_->set_fullscreen_mode(
                enable ? window_fullscreen_mode::fullscreen_window : window_fullscreen_mode::windowed,
                nullptr);
        }
        void set_resizable(bool value) { impl_->set_resizable(value); }
        void set_borderless(bool value) { impl_->set_borderless(value); }
        void set_size_constraints(window_size_constraints value) { impl_->set_size_constraints(value); }
        void apply_size_constraints(bool enable = true) { impl_->apply_size_constraints(enable); }
        void set_icon(const any_bitmap_view &icon);
        void set_icons(std::span<const any_bitmap_view> icons);

        // Cursor
        void show_cursor(bool show = true)    { impl_->show_cursor(show); }
        void hide_cursor()                    { impl_->show_cursor(false); }
        bool set_cursor(system_mouse_cursor cursor) {
            return impl_->set_cursor(cursor);
        }
        bool set_cursor(const mouse_cursor &cursor) {
            return impl_->set_cursor(cursor.impl_);
        }
        bool set_cursor_position(vec2i pos) {
            return impl_->set_cursor_position(pos);
        }
        [[nodiscard]] bool is_cursor_visible() const { return impl_->is_cursor_visible(); }

        // Mouse input mode. requested_mouse_mode() records the caller's choice;
        // active_mouse_mode() can temporarily be normal while inactive/minimized.
        bool set_mouse_mode(mouse_mode mode) { return impl_->set_mouse_mode(mode); }
        [[nodiscard]] mouse_mode requested_mouse_mode() const {
            return impl_->requested_mouse_mode();
        }
        [[nodiscard]] mouse_mode active_mouse_mode() const {
            return impl_->active_mouse_mode();
        }

        // Event source
        event_source &get_event_source() { return impl_->get_event_source(); }

        // Pump pending OS messages and emit events; call once per frame
        void poll() { impl_->poll(); }

        // Backend handle (HWND, NSWindow*, etc.)
        [[nodiscard]] void *native_handle() const { return impl_->native_handle(); }

        // Most recently focused window
        [[nodiscard]] static window *current();

        // Direct impl access (for use by backends / advanced users)
        [[nodiscard]] window_impl* impl() const noexcept { return impl_.get(); }

    private:
        std::unique_ptr<window_impl> impl_;
    };

    bool inhibit_screensaver(bool inhibit);

namespace detail {
void set_current_window(window *value) noexcept;
}

} // namespace alia

#endif /* WINDOW_C9F4D198_C82B_40AC_8414_6D11AB9C6591 */
