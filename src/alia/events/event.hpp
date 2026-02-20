#ifndef EVENT_D20B5DB9_6979_4D79_B3F7_E50D954510D6
#define EVENT_D20B5DB9_6979_4D79_B3F7_E50D954510D6

#include <concepts>
#include <cstdint>

namespace alia {

class event_source;

using event_type_id = uint32_t;

struct base_event {
    event_type_id type;
    double timestamp;
    event_source* source;
};

template <typename T>
concept event = std::is_aggregate_v<T> && std::derived_from<T, base_event>;

} // namespace alia

#endif /* EVENT_D20B5DB9_6979_4D79_B3F7_E50D954510D6 */
