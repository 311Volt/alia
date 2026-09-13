#ifndef ALIA_IO_KEYBOARD_HPP
#define ALIA_IO_KEYBOARD_HPP

#include "keycodes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace alia {

class keyboard_state;

namespace detail {
    void update_keyboard_state(key value, bool down, key_mod modifiers);
    void refresh_keyboard_modifiers(key_mod modifiers);
    void clear_keyboard_state(key_mod modifiers);
}

// Snapshot of the keyboard state tracked from window messages.
class keyboard_state {
public:
    keyboard_state();

    [[nodiscard]] bool is_key_down(key value) const;
    [[nodiscard]] bool operator[](key value) const {
        return is_key_down(value);
    }

    [[nodiscard]] key_mod get_modifiers() const;

    [[nodiscard]] bool shift() const;
    [[nodiscard]] bool ctrl() const;
    [[nodiscard]] bool alt() const;
    [[nodiscard]] bool alt_gr() const;
    [[nodiscard]] bool super() const;
    [[nodiscard]] bool caps_lock() const;
    [[nodiscard]] bool num_lock() const;
    [[nodiscard]] bool scroll_lock() const;

private:
    friend void detail::update_keyboard_state(key, bool, key_mod);
    friend void detail::refresh_keyboard_modifiers(key_mod);
    friend void detail::clear_keyboard_state(key_mod);

    void refresh_modifiers(key_mod modifiers);

    std::array<bool, static_cast<std::size_t>(key::key_count)> keys_{};
    key_mod modifiers_ = key_mod::none;
};

[[nodiscard]] keyboard_state get_keyboard_state();
[[nodiscard]] bool is_key_down(key value);
[[nodiscard]] bool is_key_down(const keyboard_state &state, key value);

enum class key_led : std::uint32_t {
    none = 0,
    num_lock = 1 << 0,
    caps_lock = 1 << 1,
    scroll_lock = 1 << 2,
};

constexpr key_led operator|(key_led a, key_led b) {
    return static_cast<key_led>(
        static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

constexpr key_led operator&(key_led a, key_led b) {
    return static_cast<key_led>(
        static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

constexpr key_led &operator|=(key_led &a, key_led b) {
    a = a | b;
    return a;
}

constexpr key_led &operator&=(key_led &a, key_led b) {
    a = a & b;
    return a;
}

void set_keyboard_leds(key_led leds);
[[nodiscard]] key_led get_keyboard_leds();

[[nodiscard]] const char *get_key_name(key value);
[[nodiscard]] key get_key_from_name(const char *name);

} // namespace alia

#endif // ALIA_IO_KEYBOARD_HPP
