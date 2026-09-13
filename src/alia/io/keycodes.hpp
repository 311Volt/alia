#ifndef ALIA_IO_KEYCODES_HPP
#define ALIA_IO_KEYCODES_HPP

#include <cstdint>

namespace alia {

// key codes (platform-independent)
enum class key : int {
#define ALIA_KEYCODE(identifier, display_name) identifier,
#include "keycodes.inc"
#undef ALIA_KEYCODE
    key_count  // Number of keys
};

static_assert(static_cast<int>(key::unknown) == 0);

// key modifiers (bitmask)
enum class key_mod : uint32_t {
    none = 0,
    shift = 1 << 0,
    ctrl = 1 << 1,
    alt = 1 << 2,
    super = 1 << 3,  // Windows/Command key
    caps_lock = 1 << 4,
    num_lock = 1 << 5,
    alt_gr = 1 << 6,
    scroll_lock = 1 << 7,
};

constexpr key_mod operator|(key_mod a, key_mod b) {
    return static_cast<key_mod>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr key_mod operator&(key_mod a, key_mod b) {
    return static_cast<key_mod>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

constexpr key_mod& operator|=(key_mod& a, key_mod b) {
    a = a | b;
    return a;
}

constexpr key_mod& operator&=(key_mod& a, key_mod b) {
    a = a & b;
    return a;
}

// mouse buttons
enum class mouse_button : int {
    left   = 0,
    right  = 1,
    middle = 2,
    x1     = 3,  // extra button 1
    x2     = 4,  // extra button 2
};

} // namespace alia

#endif // ALIA_IO_KEYCODES_HPP
