#ifndef WINDOW_F58624F0_BF07_420F_8964_19D3B7ACDB84
#define WINDOW_F58624F0_BF07_420F_8964_19D3B7ACDB84

#include "../core/vec.hpp"
#include "../core/rect.hpp"
#include "../util/cstring_view.hpp"
#include "../events/event_source.hpp"
#include "window_events.hpp"
#include <memory>
#include <cstdint>

namespace alia {

// window creation flags
enum class window_flags : uint32_t {
    none = 0,
    
    // Style flags
    windowed = 1 << 0,           // default windowed mode
    fullscreen = 1 << 1,         // Exclusive fullscreen
    fullscreen_window = 1 << 2,   // Borderless fullscreen window
    resizable = 1 << 3,          // Allow resizing
    maximized = 1 << 4,          // start maximized
    minimized = 1 << 5,          // start minimized
    no_frame = 1 << 6,            // Borderless window
    
    // Input flags
    grab_mouse = 1 << 16,         // Confine mouse to window
    high_dpi = 1 << 17,           // Enable High DPI support
};

constexpr window_flags operator|(window_flags a, window_flags b) {
    return static_cast<window_flags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr window_flags operator&(window_flags a, window_flags b) {
    return static_cast<window_flags>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

// Boolean test: any bits set?
constexpr bool any(window_flags f) {
    return static_cast<uint32_t>(f) != 0;
}

// window options for advanced configuration
struct window_options {
    vec2i position = {-1, -1};   // -1 = centered
    cstring_view title = "ALIA window";
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
    window(vec2i size, cstring_view title = "ALIA window",
           window_flags flags = window_flags::windowed);
    window(vec2i size, window_flags flags);
    window(vec2i size, window_flags flags,
           const window_options& options);
    
    // Move only
    window(window&&) noexcept;
    window& operator=(window&&) noexcept;
    ~window();
    
    window(const window&) = delete;
    window& operator=(const window&) = delete;
    
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
    [[nodiscard]] window_flags flags() const;
    
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
    
    // fullscreen
    void set_fullscreen(bool fullscreen);
    void toggle_fullscreen();
    void set_fullscreen_window(bool enable);  // Borderless fullscreen
    
    // cursor
    void show_cursor(bool show = true);
    void hide_cursor();
    void set_cursor(cursor cursor);
    void set_cursor_position(vec2i pos);
    [[nodiscard]] bool is_cursor_visible() const;
    
    // Grab
    void set_mouse_grab(bool grab);
    [[nodiscard]] bool is_mouse_grabbed() const;
    
    // event source
    event_source& get_event_source();

    // Pump pending OS messages and emit events; call once per frame
    void poll();

    // Backend handle (HWND, window, etc.)
    [[nodiscard]] void* native_handle() const;
    
    // Current window (most recently focused or created)
    [[nodiscard]] static window* current();
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace alia

#endif /* WINDOW_F58624F0_BF07_420F_8964_19D3B7ACDB84 */
