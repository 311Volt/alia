#ifndef ALIA_IO_KEYCODES_HPP
#define ALIA_IO_KEYCODES_HPP

#include <cstdint>

namespace alia {

// key codes (platform-independent)
enum class key : int {
    unknown = 0,
    
    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    
    // Numbers
    num0, num1, num2, num3, num4, num5, num6, num7, num8, num9,
    
    // Function keys
    f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12,
    
    // Modifiers
    lshift, rshift, lctrl, rctrl, lalt, ralt, lsuper, rsuper,
    
    // Navigation
    up, down, left, right,
    home, end, page_up, page_down,
    insert, del,
    
    // Editing
    backspace, tab, enter, escape, space,
    
    // Punctuation
    minus, equals, left_bracket, right_bracket, backslash,
    semicolon, apostrophe, grave, comma, period, slash,
    
    // Numpad
    numpad0, numpad1, numpad2, numpad3, numpad4,
    numpad5, numpad6, numpad7, numpad8, numpad9,
    numpad_add, numpad_subtract, numpad_multiply, numpad_divide,
    numpad_enter, numpad_decimal,
    
    // Lock keys
    caps_lock, num_lock, scroll_lock,
    
    // Other
    print_screen, pause, menu,
    
    key_count  // Number of keys
};

// key modifiers (bitmask)
enum class key_mod : uint32_t {
    none = 0,
    shift = 1 << 0,
    ctrl = 1 << 1,
    alt = 1 << 2,
    super = 1 << 3,  // Windows/Command key
    caps_lock = 1 << 4,
    num_lock = 1 << 5,
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

} // namespace alia

#endif // ALIA_IO_KEYCODES_HPP
