#ifndef INTERVAL_TREE_ROUTER_B797A686_295C_4DB0_9784_B9E66CC1C327
#define INTERVAL_TREE_ROUTER_B797A686_295C_4DB0_9784_B9E66CC1C327

#include <vector>
#include <ranges>
#include <algorithm>

namespace alia {

// Simple Interval Tree router for 1D range queries
template <typename T, typename ID>
class interval_tree_router {
    struct entry {
        T low, high;
        ID id;
    };
    std::vector<entry> entries_;

public:
    using key_type = std::pair<T, T>; // [low, high)
    using id_type = ID;

    void insert(const key_type& key, ID id) {
        entries_.push_back({key.first, key.second, id});
    }

    void remove(const key_type& key, ID id) {
        std::erase_if(entries_, [&](const entry& e) { 
            return e.low == key.first && e.high == key.second && e.id == id; 
        });
    }

    auto query(const T& point) const {
        return entries_
            | std::views::filter([point](const entry& e) {
                return point >= e.low && point < e.high;
            })
            | std::views::transform([](const entry& e) { return e.id; });
    }
};

} // namespace alia

#endif /* INTERVAL_TREE_ROUTER_B797A686_295C_4DB0_9784_B9E66CC1C327 */
