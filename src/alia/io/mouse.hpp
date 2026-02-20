#ifndef ALIA_IO_MOUSE_HPP
#define ALIA_IO_MOUSE_HPP

#include "../core/vec.hpp"
#include "../events/event_source.hpp"
#include <array>

namespace alia {

class window;

// Mouse state
class mouse_state {
public:
    mouse_state();
    
    [[nodiscard]] vec2i position() const;
    [[nodiscard]] vec2f position_f() const;
    
    [[nodiscard]] bool is_button_down(mouse_button button) const;
    [[nodiscard]] bool operator[](mouse_button button) const { return is_button_down(button); }
    
    [[nodiscard]] int wheel_delta() const;
    [[nodiscard]] int wheel_delta_x() const;  // Horizontal scroll
    
private:
    vec2i position_;
    std::array<bool, static_cast<size_t>(mouse_button::button_count)> buttons_;
    int wheel_delta_;
    int wheel_delta_x_;
};

// Get current mouse state
[[nodiscard]] mouse_state get_mouse_state();

// Get mouse position
[[nodiscard]] vec2i get_mouse_position();
[[nodiscard]] vec2f get_mouse_position_f();

// Set mouse position (relative to window or screen)
void set_mouse_position(vec2i pos);
void set_mouse_position(vec2i pos, const window& window);

// Check if a button is down
[[nodiscard]] bool is_mouse_button_down(mouse_button button);

// Get event source for mouse events
event_source& get_mouse_event_source();

} // namespace alia

#endif // ALIA_IO_MOUSE_HPP
