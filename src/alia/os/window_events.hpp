#ifndef WINDOW_EVENTS_E75D05C9_A37B_4004_8843_279A20162018
#define WINDOW_EVENTS_E75D05C9_A37B_4004_8843_279A20162018

#include "../events/event.hpp"
#include "../core/vec.hpp"
#include "../core/rect.hpp"
#include "../io/keycodes.hpp"
#include "../io/mouse.hpp"

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
    key_mod  modifiers; // held modifiers and lock-key toggle state
    bool     is_repeat; // true if key was already held (auto-repeat)
};

struct window_expose_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0108;
    rect_i area;
};

// The retired 0x0104-0x0106 mouse IDs are deliberately not reused.
struct mouse_axes_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0109;
    vec2i position;
    vec2i delta;
    int z;
    int w;
    int dz;
    int dw;
    float pressure;
};

struct mouse_button_down_event {
    static constexpr event_type_id_t alia_event_type_id = 0x010a;
    mouse_button button;
    vec2i position;
    int z;
    int w;
    float pressure;
};

struct mouse_button_up_event {
    static constexpr event_type_id_t alia_event_type_id = 0x010b;
    mouse_button button;
    vec2i position;
    int z;
    int w;
    float pressure;
};

struct mouse_warped_event {
    static constexpr event_type_id_t alia_event_type_id = 0x010c;
    vec2i position;
    vec2i delta;
    int z;
    int w;
    int dz;
    int dw;
    float pressure;
};

struct mouse_enter_window_event {
    static constexpr event_type_id_t alia_event_type_id = 0x010d;
    vec2i position;
    int z;
    int w;
};

struct mouse_leave_window_event {
    static constexpr event_type_id_t alia_event_type_id = 0x010e;
    vec2i position;
    int z;
    int w;
};

struct mouse_relative_event {
    static constexpr event_type_id_t alia_event_type_id = 0x010f;
    vec2i delta;
};

struct mouse_mode_changed_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0110;
    mouse_mode requested;
    mouse_mode active;
};

} // namespace alia

#endif /* WINDOW_EVENTS_E75D05C9_A37B_4004_8843_279A20162018 */
