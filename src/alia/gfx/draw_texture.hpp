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
        // Level-zero pixels, fractions kept; crops may extend beyond the
        // texture's bounds, leaving addressing to the sampler's wrap mode.
        std::variant<detail::full_rect_t, rect_f> source_crop_rect = full_rect;
        // Used only for vec2i destinations; vectors are pixels from the crop's
        // top-left and may lie outside the crop.
        std::variant<draw_anchor, vec2f> source_anchor = draw_anchor::top_left;
        // vec2i draws the crop at native size with source_anchor placed there.
        // rect_f stretches the crop; rotated_rect_f also rotates it.
        std::variant<vec2i, rect_f, rotated_rect_f> destination;
        color tint = white;
    };

    // Submit one full_vertex quad using the current frame render state and
    // viewport. Fixed-function drawing uses slot 0;
    // shaders must sample the explicit slot (stored shader samplers still apply).
    // Bind the texture with its stored sampler; the binding persists after drawing.
    // Zero-width or zero-height geometry leaves bindings untouched. Inverted
    // rectangles, negative rotated sizes, and non-finite geometry throw
    // std::invalid_argument before binding.
    void draw_texture(const draw_texture_params &);
} // namespace alia

#endif
