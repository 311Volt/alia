#ifndef ARRAY_ROUTER_B0649D94_A5D3_4878_A21C_18E78DA64AA4
#define ARRAY_ROUTER_B0649D94_A5D3_4878_A21C_18E78DA64AA4

#include <vector>
#include <ranges>
#include <concepts>
#include <optional>

namespace alia {

/**
 * @brief An event router that uses an array for O(1) lookup.
 * 
 * This router assumes that the keys are non-negative integers.
 * It uses a std::vector to store one ID per key.
 */
template <std::integral Key, typename ID>
class array_router {
    std::vector<std::optional<ID>> storage_;

public:
    using key_type = Key;
    using id_type = ID;

    void insert(const Key& key, ID id) {
        if (static_cast<std::size_t>(key) >= storage_.size()) {
            storage_.resize(static_cast<std::size_t>(key) + 1);
        }
        storage_[key] = id;
    }

    void remove(const Key& key, ID id) {
        if (static_cast<std::size_t>(key) < storage_.size()) {
            if (storage_[key] == id) {
                storage_[key] = std::nullopt;
            }
        }
    }

    auto query(const Key& key) const {
        if (static_cast<std::size_t>(key) < storage_.size() && storage_[key].has_value()) {
            return std::ranges::single_view(*storage_[key]);
        } else {
            return std::ranges::empty_view<ID>{};
        }
    }
};

} // namespace alia

#endif /* ARRAY_ROUTER_B0649D94_A5D3_4878_A21C_18E78DA64AA4 */
