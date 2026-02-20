#ifndef ALIA_IO_KEYBOARD_HPP
#define ALIA_IO_KEYBOARD_HPP

#include "keycodes.hpp"
#include "../events/event_source.hpp"
#include <array>

namespace alia {

// Keyboard state
class keyboard_state {
public:
    keyboard_state();
    
    [[nodiscard]] bool is_key_down(key key) const;
    [[nodiscard]] bool operator[](key key) const { return is_key_down(key); }
    
    [[nodiscard]] key_mod get_modifiers() const;
    
    // Check specific modifiers
    [[nodiscard]] bool shift() const;
    [[nodiscard]] bool ctrl() const;
    [[nodiscard]] bool alt() const;
    [[nodiscard]] bool super() const;
    
private:
    std::array<bool, static_cast<size_t>(key::key_count)> keys_;
    key_mod modifiers_;
};

// Get current keyboard state
[[nodiscard]] keyboard_state get_keyboard_state();

// Check if a specific key is currently down
[[nodiscard]] bool is_key_down(key key);
[[nodiscard]] bool is_key_down(const keyboard_state& state, key key);

// Get event source for keyboard events
event_source& get_keyboard_event_source();

// key name utilities
[[nodiscard]] const char* get_key_name(key key);
[[nodiscard]] key get_key_from_name(const char* name);

} // namespace alia

#endif // ALIA_IO_KEYBOARD_HPP
