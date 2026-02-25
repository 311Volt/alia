#ifndef WINDOW_C9F4D198_C82B_40AC_8414_6D11AB9C6591
#define WINDOW_C9F4D198_C82B_40AC_8414_6D11AB9C6591

#include "../core/vec.hpp"
#include "../core/rect.hpp"
#include "../util/cstring_view.hpp"
#include "../events/event_source.hpp"
#include "window_events.hpp"
#include <memory>
#include <variant>

namespace alia {

namespace detail { struct window_pos_centered_t {}; }
inline constexpr detail::window_pos_centered_t window_pos_centered;

enum class window_fullscreen_mode {
    windowed,
    fullscreen,         // exclusive fullscreen
    fullscreen_window,  // borderless fullscreen
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
    bool grab_mouse = false;
    bool high_dpi   = false;
};

    // cursor types
    enum class cursor {
        default_cursor,
        arrow,
        ibeam,
        wait,
        crosshair,
        resize_nwse,
        resize_nesw,
        resize_we,
        resize_ns,
        resize_all,
        hand,
        not_allowed,
        hidden,
    };

// ── Implementation interface (internal) ──────────────────────────────

struct window_impl {
    virtual ~window_impl() = default;

    // Properties
    virtual vec2i        size()         const = 0;
    virtual vec2i        position()     const = 0;
    virtual cstring_view title()        const = 0;
    virtual float        aspect_ratio() const = 0;

    // State
    virtual bool is_focused()    const = 0;
    virtual bool is_minimized()  const = 0;
    virtual bool is_maximized()  const = 0;
    virtual bool is_fullscreen() const = 0;

    // Modification
    virtual void set_title(cstring_view title) = 0;
    virtual void set_position(vec2i pos) = 0;
    virtual void resize(vec2i size) = 0;
    virtual void minimize() = 0;
    virtual void maximize() = 0;
    virtual void restore() = 0;

    // Fullscreen
    virtual void set_fullscreen(bool fullscreen) = 0;
    virtual void set_fullscreen_window(bool enable) = 0;

    // Cursor
    virtual void show_cursor(bool show) = 0;
    virtual void set_cursor(cursor c) = 0;
    virtual void set_cursor_position(vec2i pos) = 0;
    virtual bool is_cursor_visible() const = 0;

    // Mouse grab
    virtual void set_mouse_grab(bool grab) = 0;
    virtual bool is_mouse_grabbed() const = 0;

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
        [[nodiscard]] bool is_fullscreen() const { return impl_->is_fullscreen(); }

        // Modification
        void set_title(cstring_view title)  { impl_->set_title(title); }
        void set_position(vec2i pos)        { impl_->set_position(pos); }
        void set_size(vec2i sz)             { impl_->resize(sz); }
        void resize(vec2i sz)               { impl_->resize(sz); }

        void minimize() { impl_->minimize(); }
        void maximize() { impl_->maximize(); }
        void restore()  { impl_->restore(); }

        // Fullscreen
        void set_fullscreen(bool fullscreen)    { impl_->set_fullscreen(fullscreen); }
        void toggle_fullscreen()                { impl_->set_fullscreen(!impl_->is_fullscreen()); }
        void set_fullscreen_window(bool enable) { impl_->set_fullscreen_window(enable); }

        // Cursor
        void show_cursor(bool show = true)    { impl_->show_cursor(show); }
        void hide_cursor()                    { impl_->show_cursor(false); }
        void set_cursor(cursor c)             { impl_->set_cursor(c); }
        void set_cursor_position(vec2i pos)   { impl_->set_cursor_position(pos); }
        [[nodiscard]] bool is_cursor_visible() const { return impl_->is_cursor_visible(); }

        // Mouse grab
        void set_mouse_grab(bool grab)              { impl_->set_mouse_grab(grab); }
        [[nodiscard]] bool is_mouse_grabbed() const { return impl_->is_mouse_grabbed(); }

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

} // namespace alia

#endif /* WINDOW_C9F4D198_C82B_40AC_8414_6D11AB9C6591 */
