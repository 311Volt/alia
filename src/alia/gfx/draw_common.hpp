#ifndef ALIA_GFX_DRAW_COMMON_HPP
#define ALIA_GFX_DRAW_COMMON_HPP

#include "../core/vec.hpp"

#include <variant>

namespace alia {
    enum class draw_anchor {
        top_left, top_center, top_right,
        center_left, center, center_right,
        bottom_left, bottom_center, bottom_right
    };

    namespace detail {
        // Texture slot member of draw-helper params. It has no default, so
        // aggregate initialization requires an explicit slot at every callsite.
        struct required_texture_slot {
            int value;
            required_texture_slot() = delete;
            constexpr required_texture_slot(int value) noexcept
                : value(value) {}
        };

        // Offset of the anchor from the top-left of a box of the given size.
        // Custom vectors are returned as-is; non-finite ones throw
        // std::invalid_argument with the given message.
        [[nodiscard]] vec2f anchor_offset(
            const std::variant<draw_anchor, vec2f> &anchor,
            vec2f size,
            const char *non_finite_message
        );
    } // namespace detail
} // namespace alia

#endif
