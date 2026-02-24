#ifndef EVENT_EDF071D1_00E2_44D5_ABDC_8B19C4250DCB
#define EVENT_EDF071D1_00E2_44D5_ABDC_8B19C4250DCB

#include <cstdint>
#include <type_traits>
#include <atomic>
#include <concepts>
#include "../util/type_erasure.hpp"

namespace alia {

// ── Event Type ID ─────────────────────────────────────────────────────

using event_type_id_t = std::uint32_t;

// Compile-time event IDs must be less than this threshold
constexpr event_type_id_t RUNTIME_EVENT_ID_START = 0x40000000;

// ── Event Concept ─────────────────────────────────────────────────────

template<typename T>
concept event = std::is_aggregate_v<T>;

// ── Event Source (forward declaration) ────────────────────────────────

class event_source;

// ── Event Metadata ────────────────────────────────────────────────────

struct event_metadata {
    double          timestamp;
    event_source*   source;
    event_type_id_t event_type_id;
};

// ── Runtime Event Type ID Generation ──────────────────────────────────

namespace detail {
    inline std::atomic<event_type_id_t> next_runtime_event_id{RUNTIME_EVENT_ID_START};
}

// Get event type ID for events with compile-time IDs
template<event TEvent>
    requires requires { TEvent::alia_event_type_id; }
constexpr event_type_id_t get_event_type_id() noexcept {
    static_assert(TEvent::alia_event_type_id < RUNTIME_EVENT_ID_START,
                  "Compile-time event type IDs must be less than RUNTIME_EVENT_ID_START");
    return TEvent::alia_event_type_id;
}

// Get event type ID for events without compile-time IDs (runtime generation)
template<event TEvent>
    requires (!requires { TEvent::alia_event_type_id; })
event_type_id_t get_event_type_id() noexcept {
    static const event_type_id_t id = detail::next_runtime_event_id.fetch_add(1, std::memory_order_relaxed);
    return id;
}

// ── Event Slot ────────────────────────────────────────────────────────
//
// event_slot<TEvent> is the storage unit placed in the event queue's any_deque.
// The base class is layout-compatible with all specialisations, allowing the
// queue to read metadata and dispatch without knowing the concrete event type.

struct event_slot_base; // forward – needed by event_vtable_t

/// Per-event-type vtable used by the event queue for type-erased dispatch.
struct event_vtable_t {
    /// Type-erasure vtable for the event payload (TEvent, not the whole slot).
    /// Used by pop() to move-construct the payload into an soo_any.
    const type_erasure_vtable_t* event_type_erasure_vt;

    /// Returns a pointer to the TEvent payload inside the given slot.
    void* (*get_event_ptr)(event_slot_base* slot) noexcept;
};

struct event_slot_base {
    event_metadata        meta;
    const event_vtable_t* event_vt;
};

template<event TEvent>
struct event_slot : event_slot_base {
    TEvent ev;

    static const event_vtable_t* get_event_vtable() noexcept {
        static const event_vtable_t vt = {
            type_erasure_vtable_for<TEvent>(),
            [](event_slot_base* slot) noexcept -> void* {
                return &static_cast<event_slot<TEvent>*>(slot)->ev;
            },
        };
        return &vt;
    }
};

} // namespace alia

#endif /* EVENT_EDF071D1_00E2_44D5_ABDC_8B19C4250DCB */
