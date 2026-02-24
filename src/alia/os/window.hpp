#ifndef WINDOW_F58624F0_BF07_420F_8964_19D3B7ACDB84
#define WINDOW_F58624F0_BF07_420F_8964_19D3B7ACDB84

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

    // window class
    class window {
    public:
        // Construction
        window() = default;
        window(vec2i size, window_options opts = {});

        // Move only
        window(window &&) noexcept;
        window &operator=(window &&) noexcept;
        ~window();

        window(const window &) = delete;
        window &operator=(const window &) = delete;

        [[nodiscard]] bool valid() const;
        [[nodiscard]] explicit operator bool() const { return valid(); }

        // Properties
        [[nodiscard]] int width() const;
        [[nodiscard]] int height() const;
        [[nodiscard]] vec2i size() const;
        [[nodiscard]] rect_i rect() const { return rect_i{{0, 0}, size()}; }
        [[nodiscard]] float aspect_ratio() const;
        [[nodiscard]] vec2i position() const;
        [[nodiscard]] cstring_view title() const;

        // State
        [[nodiscard]] bool is_focused() const;
        [[nodiscard]] bool is_minimized() const;
        [[nodiscard]] bool is_maximized() const;
        [[nodiscard]] bool is_fullscreen() const;

        // Modification
        void set_title(cstring_view title);
        void set_position(vec2i pos);
        void set_size(vec2i size);
        void resize(vec2i size);

        void minimize();
        void maximize();
        void restore();

        // Fullscreen
        void set_fullscreen(bool fullscreen);
        void toggle_fullscreen();
        void set_fullscreen_window(bool enable); // borderless fullscreen

        // Cursor
        void show_cursor(bool show = true);
        void hide_cursor();
        void set_cursor(cursor c);
        void set_cursor_position(vec2i pos);
        [[nodiscard]] bool is_cursor_visible() const;

        // Mouse grab
        void set_mouse_grab(bool grab);
        [[nodiscard]] bool is_mouse_grabbed() const;

        // Event source
        event_source &get_event_source();

        // Pump pending OS messages and emit events; call once per frame
        void poll();

        // Backend handle (HWND, NSWindow*, etc.)
        [[nodiscard]] void *native_handle() const;

        // Most recently focused window
        [[nodiscard]] static window *current();

    private:
        struct impl;
        std::unique_ptr<impl> impl_;
    };

} // namespace alia

#endif /* WINDOW_F58624F0_BF07_420F_8964_19D3B7ACDB84 */
