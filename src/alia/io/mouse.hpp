#ifndef ALIA_IO_MOUSE_HPP
#define ALIA_IO_MOUSE_HPP

#include "../core/vec.hpp"
#include <cstdint>
#include <memory>
#include <optional>

namespace alia {

class any_bitmap_view;
class window;

// Mouse buttons are one-based so button N maps directly to bit N - 1.
enum class mouse_button : unsigned {
    left = 1,
    right = 2,
    middle = 3,
    x1 = 4,
    x2 = 5,
};

enum class mouse_mode {
    normal,
    confined,
    relative,
};

enum class system_mouse_cursor {
    default_cursor,
    arrow,
    busy,
    question,
    edit,
    move,
    resize_north,
    resize_south,
    resize_east,
    resize_west,
    resize_northeast,
    resize_northwest,
    resize_southeast,
    resize_southwest,
    progress,
    precision,
    link,
    alternate_selection,
    unavailable,
};

namespace detail {
class mouse_cursor_impl;
}

// A native cursor made from a copied bitmap. Windows retain the underlying
// native cursor while it is selected, even if this wrapper is destroyed.
class mouse_cursor {
public:
    mouse_cursor(const any_bitmap_view &bitmap, vec2i hotspot);
    ~mouse_cursor();

    mouse_cursor(mouse_cursor &&) noexcept;
    mouse_cursor &operator=(mouse_cursor &&) noexcept;

    mouse_cursor(const mouse_cursor &) = delete;
    mouse_cursor &operator=(const mouse_cursor &) = delete;

private:
    friend class window;
    std::shared_ptr<const detail::mouse_cursor_impl> impl_;
};

// Snapshot of state already processed by window::poll(). Reading a snapshot
// never consumes input. associated_window() is borrowed; a saved association
// requires that window to remain alive and at the same object address.
class mouse_state {
public:
    mouse_state();

    [[nodiscard]] vec2i position() const;
    [[nodiscard]] vec2f position_f() const;
    [[nodiscard]] int z() const;
    [[nodiscard]] int w() const;
    [[nodiscard]] int axis(unsigned index) const;
    [[nodiscard]] std::uint32_t button_mask() const;
    // Ordinary mouse input reports 0.0f; native pen/tablet pressure is not
    // part of this API implementation.
    [[nodiscard]] float pressure() const;
    [[nodiscard]] alia::window *associated_window() const;

    [[nodiscard]] bool is_button_down(mouse_button button) const;
    [[nodiscard]] bool is_button_down(unsigned button) const;
    [[nodiscard]] bool operator[](mouse_button button) const {
        return is_button_down(button);
    }
    [[nodiscard]] bool operator[](unsigned button) const {
        return is_button_down(button);
    }

private:
    friend struct detail_mouse_state_access;

    vec2i position_{};
    vec2f position_f_{};
    int z_ = 0;
    int w_ = 0;
    std::uint32_t buttons_ = 0;
    float pressure_ = 0.0f;
    alia::window *window_ = nullptr;
};

[[nodiscard]] mouse_state get_mouse_state();
[[nodiscard]] bool is_mouse_button_down(mouse_button button);
[[nodiscard]] bool is_mouse_button_down(unsigned button);
[[nodiscard]] bool is_mouse_button_down(
    const mouse_state &state, mouse_button button);
[[nodiscard]] bool is_mouse_button_down(
    const mouse_state &state, unsigned button);

[[nodiscard]] unsigned get_mouse_num_axes();
[[nodiscard]] unsigned get_mouse_num_buttons();

[[nodiscard]] bool can_get_mouse_cursor_position();
[[nodiscard]] std::optional<vec2i> get_mouse_cursor_position();

bool set_mouse_z(int value);
bool set_mouse_w(int value);
bool set_mouse_axis(unsigned axis, int value);

[[nodiscard]] int get_mouse_wheel_precision();
void set_mouse_wheel_precision(int precision);

// Releases whichever alia window currently owns confined or relative input.
bool ungrab_mouse();

// Axis 0/1 are x/y and cannot be set. Axis 2 is z and axis 3 is w.
struct mouse_axis_set_event {
    static constexpr std::uint32_t alia_event_type_id = 0x0200;
    unsigned axis;
    int value;
    int delta;
};

} // namespace alia

#endif // ALIA_IO_MOUSE_HPP
