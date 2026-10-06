#include "draw_common.hpp"

#include <cmath>
#include <stdexcept>

namespace alia::detail {
    vec2f anchor_offset(
        const std::variant<draw_anchor, vec2f> &anchor,
        vec2f size,
        const char *non_finite_message
    ) {
        if (const auto *offset = std::get_if<vec2f>(&anchor)) {
            if (!std::isfinite(offset->x) || !std::isfinite(offset->y))
                throw std::invalid_argument(non_finite_message);
            return *offset;
        }

        switch (std::get<draw_anchor>(anchor)) {
        case draw_anchor::top_left:      return {0.0f, 0.0f};
        case draw_anchor::top_center:    return {size.x * 0.5f, 0.0f};
        case draw_anchor::top_right:     return {size.x, 0.0f};
        case draw_anchor::center_left:   return {0.0f, size.y * 0.5f};
        case draw_anchor::center:        return size * 0.5f;
        case draw_anchor::center_right:  return {size.x, size.y * 0.5f};
        case draw_anchor::bottom_left:   return {0.0f, size.y};
        case draw_anchor::bottom_center: return {size.x * 0.5f, size.y};
        case draw_anchor::bottom_right:  return size;
        }
        throw std::invalid_argument("invalid draw_anchor");
    }
} // namespace alia::detail
