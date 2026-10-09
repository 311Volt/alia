#include "draw_texture.hpp"
#include "frame.hpp"
#include "texture.hpp"
#include "transform.hpp"
#include "vertex.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace alia {
    namespace {
        bool is_finite(vec2f value) {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        void validate_rect(rect_f value, const char *message) {
            if (!is_finite(value.p1) || !is_finite(value.p2) ||
                value.right() < value.left() || value.bottom() < value.top() ||
                !is_finite(value.size()))
                throw std::invalid_argument(message);
        }

        bool is_empty(vec2f size) {
            return size.x == 0.0f || size.y == 0.0f;
        }

        std::array<vec2f, 4> corners(rect_f value) {
            return {value.tl(), value.tr(), value.br(), value.bl()};
        }
    } // namespace

    void draw_texture(const draw_texture_params &params) {
        const vec2f texture_size(params.texture.size());
        const auto *explicit_crop = std::get_if<rect_f>(&params.source_crop_rect);
        const rect_f crop = explicit_crop
            ? *explicit_crop : rect_f::pos_size({}, texture_size);
        validate_rect(crop, "draw_texture: crop must be finite and not inverted");
        const vec2f crop_size = crop.size();

        std::array<vec2f, 4> positions;
        vec2f destination_size;
        if (const auto *position = std::get_if<vec2i>(&params.destination)) {
            const vec2f offset = detail::anchor_offset(
                params.source_anchor, crop_size, "draw_texture: source anchor must be finite");
            const rect_f destination = rect_f::pos_size(vec2f(*position) - offset, crop_size);
            validate_rect(destination, "draw_texture: destination geometry must be finite");
            positions = corners(destination);
            destination_size = destination.size();
        } else if (const auto *destination = std::get_if<rect_f>(&params.destination)) {
            validate_rect(*destination, "draw_texture: destination must be finite and not inverted");
            positions = corners(*destination);
            destination_size = destination->size();
        } else {
            const auto &rotated = std::get<rotated_rect_f>(params.destination);
            if (!is_finite(rotated.center) || !is_finite(rotated.size) ||
                !std::isfinite(rotated.angle_rad) ||
                rotated.size.x < 0.0f || rotated.size.y < 0.0f)
                throw std::invalid_argument(
                    "draw_texture: rotated destination must be finite with non-negative size");

            const vec2f half_size = rotated.size * 0.5f;
            positions = corners({{-half_size.x, -half_size.y}, half_size});
            const transform rotation = transform::rotate(rotated.angle_rad);
            for (auto &position : positions)
                position = rotated.center + rotation.apply(position);
            destination_size = rotated.size;
        }

        for (const auto &position : positions) {
            if (!is_finite(position))
                throw std::invalid_argument("draw_texture: destination geometry must be finite");
        }
        if (is_empty(crop_size) || is_empty(destination_size))
            return;

        const rect_f uv{
            {crop.left() / texture_size.x, crop.top() / texture_size.y},
            {crop.right() / texture_size.x, crop.bottom() / texture_size.y}
        };
        if (!is_finite(uv.p1) || !is_finite(uv.p2))
            throw std::invalid_argument("draw_texture: texture coordinates must be finite");

        const full_vertex vertices[]{
            {positions[0], params.tint, uv.tl()},
            {positions[1], params.tint, uv.tr()},
            {positions[2], params.tint, uv.br()},
            {positions[3], params.tint, uv.bl()},
        };
        constexpr uint16_t indices[]{0, 1, 2, 0, 2, 3};

        params.target.set_texture(params.texture_slot.value, params.texture);
        params.target.draw_indexed<full_vertex>(vertices, indices);
    }
} // namespace alia
