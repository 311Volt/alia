#ifndef QUADTREE_ROUTER_F50D16FE_3716_4579_8F23_88FC7EE73134
#define QUADTREE_ROUTER_F50D16FE_3716_4579_8F23_88FC7EE73134

#include <vector>
#include <ranges>
#include "../../core/rect.hpp"
#include "../../core/vec.hpp"

namespace alia {

template <typename T, typename ID>
class rect_quadtree_router {
    struct entry {
        rect<T> bounds;
        ID id;
    };
    std::vector<entry> entries_;

public:
    using key_type = rect<T>;
    using id_type = ID;

    void insert(const rect<T>& bounds, ID id) {
        entries_.push_back({bounds, id});
    }

    void remove(const rect<T>& bounds, ID id) {
        std::erase_if(entries_, [&](const entry& e) { return e.id == id && e.bounds == bounds; });
    }

    auto query(const vec2<T>& p) const {
        return entries_
            | std::views::filter([p](const entry& e) {
                return e.bounds.contains(p);
            })
            | std::views::transform([](const entry& e) { return e.id; });
    }
};

} // namespace alia

#endif /* QUADTREE_ROUTER_F50D16FE_3716_4579_8F23_88FC7EE73134 */
