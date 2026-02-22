#ifndef ALIA_OS_WINDOW_EVENTS_HPP
#define ALIA_OS_WINDOW_EVENTS_HPP

#include "../events/event.hpp"
#include "../core/vec.hpp"

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

// Platform virtual key code; exact value is OS-defined (e.g. Win32 VK_*)
struct window_key_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0102;
    int vk_code;
    bool pressed;
};

struct window_mouse_move_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0103;
    vec2i position;  // client-area coordinates
};

// button: 0=left, 1=right, 2=middle
struct window_mouse_button_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0104;
    int button;
    bool pressed;
    vec2i position;
};

} // namespace alia

#endif // ALIA_OS_WINDOW_EVENTS_HPP
