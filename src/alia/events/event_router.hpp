#ifndef EVENT_ROUTER_B3BC0B91_8C7E_4D64_AF05_C37FD500CF30
#define EVENT_ROUTER_B3BC0B91_8C7E_4D64_AF05_C37FD500CF30

#include <concepts>
#include <ranges>
#include "event.hpp"

namespace alia {

/**
 * @brief The event_router concept.
 * Associates Keys with IDs and allows querying IDs based on a Query.
 */
template <typename T, typename Key, typename Query, typename ID>
concept event_router = requires(T t, const Key& key, const Query& query, ID id) {
    { t.insert(key, id) } -> std::same_as<void>;
    { t.remove(key, id) } -> std::same_as<void>;
    { t.query(query) } -> std::ranges::range;
    requires std::convertible_to<std::ranges::range_value_t<decltype(t.query(query))>, ID>;
};

/**
 * @brief Maps an Event to a Query for a Partitioner.
 */
template <typename RouterT, typename EventT, typename MapperT>
class mapped_router {
    RouterT& router_;
    MapperT mapper_;

public:
    using KeyType = typename RouterT::key_type;
    using IDType = typename RouterT::id_type;

    mapped_router(RouterT& router, MapperT mapper) : router_(router), mapper_(mapper) {}

    void insert(const KeyType& key, IDType id) {
        router_.insert(key, id);
    }

    void remove(const KeyType& key, IDType id) {
        router_.remove(key, id);
    }

    auto query_from_event(const EventT& ev) const {
        return router_.query(mapper_(ev));
    }
};

} // namespace alia

#endif /* EVENT_ROUTER_B3BC0B91_8C7E_4D64_AF05_C37FD500CF30 */
