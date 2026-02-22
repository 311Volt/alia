#ifndef EVENT_D20B5DB9_6979_4D79_B3F7_E50D954510D6
#define EVENT_D20B5DB9_6979_4D79_B3F7_E50D954510D6

#include <cstdint>
#include <type_traits>
#include <atomic>
#include <concepts>

namespace alia {

// ── Event Type ID ─────────────────────────────────────────────────────

using event_type_id_t = std::uint32_t;

// Compile-time event IDs must be less than this threshold
constexpr event_type_id_t RUNTIME_EVENT_ID_START = 0x40000000;

// ── Event Concept ─────────────────────────────────────────────────────

template<typename T>
concept event = std::is_aggregate_v<T>;

// ── Event Metadata ────────────────────────────────────────────────────

// Forward declaration
class event_source;

struct event_metadata {
    double timestamp;
    event_source* source;
};

template<event TEvent>
struct event_with_metadata {
    event_metadata meta;
    TEvent ev;
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

} // namespace alia

#endif /* EVENT_D20B5DB9_6979_4D79_B3F7_E50D954510D6 */
