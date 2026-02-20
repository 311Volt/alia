#ifndef HASHMAP_ROUTER_D23B1936_61BA_4652_B173_1226BC0255B7
#define HASHMAP_ROUTER_D23B1936_61BA_4652_B173_1226BC0255B7

#include <unordered_map>
#include <vector>
#include <ranges>

namespace alia {

template <typename Key, typename ID>
class hashmap_router {
    std::unordered_map<Key, std::vector<ID>> map_;

public:
    using key_type = Key;
    using id_type = ID;

    void insert(const Key& key, ID id) {
        map_[key].push_back(id);
    }

    void remove(const Key& key, ID id) {
        if (auto it = map_.find(key); it != map_.end()) {
            std::erase(it->second, id);
        }
    }

    auto query(const Key& key) const {
        auto it = map_.find(key);
        if (it == map_.end()) {
            return std::views::empty<ID>;
        }
        return it->second | std::views::all;
    }
};

} // namespace alia

#endif /* HASHMAP_ROUTER_D23B1936_61BA_4652_B173_1226BC0255B7 */
