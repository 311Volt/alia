#ifndef ALIA_GFX_DRAW_TEXTURE_HPP
#define ALIA_GFX_DRAW_TEXTURE_HPP

#include "../core/color.hpp"
#include "../core/rect.hpp"
#include "draw_common.hpp"

#include <variant>

namespace alia {
    class frame;
    class texture;

    namespace detail {
        struct full_rect_t {};
    } // namespace detail

    inline constexpr auto full_rect = detail::full_rect_t{};

    struct draw_texture_params {
        frame &target;
        alia::texture &texture;
        detail::required_texture_slot texture_slot;
        // Level-zero pixels; crops may extend beyond the texture's bounds.
        std::variant<detail::full_rect_t, rect_f> source_crop_rect = full_rect;
        // Used only for vec2i destinations; vectors are pixels from the crop's top-left.
        std::variant<draw_anchor, vec2f> source_anchor = draw_anchor::top_left;
        std::variant<vec2i, rect_f, rotated_rect_f> destination;
        color tint = white;
    };

    // Submit one full_vertex quad using the caller's pipeline, transforms,
    // viewport, and blending. Fixed-function drawing requires modulate and slot 0;
    // shaders must sample the explicit slot (stored shader samplers still apply).
    // Bind the texture with its stored sampler; the binding persists after drawing.
    // Empty geometry leaves bindings untouched. Invalid geometry throws
    // std::invalid_argument before binding.
    void draw_texture(const draw_texture_params &);
} // namespace alia

#endif
