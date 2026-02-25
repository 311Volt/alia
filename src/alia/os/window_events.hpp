#ifndef WINDOW_EVENTS_E75D05C9_A37B_4004_8843_279A20162018
#define WINDOW_EVENTS_E75D05C9_A37B_4004_8843_279A20162018

#include "../events/event.hpp"
#include "../core/vec.hpp"
#include "../io/keycodes.hpp"

namespace alia {

// ── Window Event Types ────────────────────────────────────────────────
// Compile-time IDs in range 0x0100–0x01FF (window subsystem)

struct window_close_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0100;
};

struct window_resize_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0101;
    vec2i new_size;
};

struct window_key_down_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0102;
    key key;
};

struct window_key_up_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0103;
    key key;
};

struct window_key_char_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0107;
    key      key;       // physical key that generated this character
    char32_t codepoint; // full Unicode codepoint (never a surrogate)
    key_mod  modifiers; // shift/ctrl/alt/super/caps/numlock state
    bool     is_repeat; // true if key was already held (auto-repeat)
};

struct window_mouse_move_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0104;
    vec2i position;  // client-area coordinates
};

struct window_mouse_button_down_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0105;
    mouse_button button;
    vec2i position;
};

struct window_mouse_button_up_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0106;
    mouse_button button;
    vec2i position;
};

} // namespace alia

#endif /* WINDOW_EVENTS_E75D05C9_A37B_4004_8843_279A20162018 */
