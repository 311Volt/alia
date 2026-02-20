#ifndef ALIA_IO_JOYSTICK_HPP
#define ALIA_IO_JOYSTICK_HPP

#include "../core/vec.hpp"
#include "../core/event_queue.hpp"
#include "../core/events.hpp" // For hat_direction
#include <string>
#include <vector>

namespace alia {

// Joystick information
struct joystick_info {
    int index;
    std::string name;
    int num_axes;
    int num_buttons;
    int num_hats;       // D-pads
    bool is_gamepad;    // Has standard gamepad layout
};

// Standard gamepad buttons (for is_gamepad == true)
enum class gamepad_button {
    A, B, X, Y,
    left_bumper, right_bumper,
    back, start, guide,
    left_thumb, right_thumb,
    dpad_up, dpad_right, dpad_down, dpad_left,
    
    button_count
};

// Standard gamepad axes
enum class gamepad_axis {
    left_x, left_y,
    right_x, right_y,
    left_trigger, right_trigger,
    
    axis_count
};

// Joystick state
class joystick_state {
public:
    joystick_state();
    joystick_state(int index);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] int index() const;
    [[nodiscard]] const joystick_info& info() const;
    
    // Raw access
    [[nodiscard]] float get_axis(int axis) const;          // -1.0 to 1.0
    [[nodiscard]] bool get_button(int button) const;
    [[nodiscard]] hat_direction get_hat(int hat) const;
    
    // Gamepad access (if is_gamepad)
    [[nodiscard]] float get_axis(gamepad_axis axis) const;
    [[nodiscard]] bool get_button(gamepad_button button) const;
    
private:
    int index_;
    joystick_info info_;
    std::vector<float> axes_;
    std::vector<bool> buttons_;
    std::vector<hat_direction> hats_;
};

// Joystick enumeration
[[nodiscard]] int get_joystick_count();
[[nodiscard]] std::vector<joystick_info> get_joysticks();
[[nodiscard]] joystick_info get_joystick_info(int index);

// Get joystick state
[[nodiscard]] joystick_state get_joystick_state(int index);

// Check if joystick is connected
[[nodiscard]] bool is_joystick_connected(int index);

// Reconfigure joysticks (after connect/disconnect)
void reconfigure_joysticks();

// Get event source for joystick events
event_source& get_joystick_event_source();

// Deadzone utility
[[nodiscard]] float apply_deadzone(float value, float deadzone);
[[nodiscard]] vec2f apply_deadzone(vec2f stick, float deadzone);

} // namespace alia

#endif // ALIA_IO_JOYSTICK_HPP
