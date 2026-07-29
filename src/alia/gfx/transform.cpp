#include "transform.hpp"
#include "gfx_device.hpp"
#include <algorithm>
#include <span>
#include <array>

namespace alia {

    static constexpr std::array<float, 16> identity_matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    static thread_local std::array<float, 16> current_transform_matrix = identity_matrix;

    static thread_local std::array<float, 16> current_projection_matrix = identity_matrix;

    transform transform::ortho_ui(float width, float height) {
        const vec2f offset = current_pixel_center_offset();
        return ortho(-offset.x, width - offset.x, height - offset.y, -offset.y);
    }

    transform get_current_transform() {
        transform t;
        get_current_transform_matrix(std::span<float, 16>(&t.m[0][0], 16));
        return t;
    }

    void set_current_transform(const transform &t) {
        set_current_transform_matrix(std::span<const float, 16>(&t.m[0][0], 16));
    }

    transform get_current_projection() {
        transform t;
        get_current_projection_matrix(std::span<float, 16>(&t.m[0][0], 16));
        return t;
    }

    void set_current_projection(const transform &t) {
        set_current_projection_matrix(std::span<const float, 16>(&t.m[0][0], 16));
    }

    void set_current_transform_matrix(std::span<const float, 16> in_matrix) {
        std::copy(in_matrix.begin(), in_matrix.end(), current_transform_matrix.begin());
    }

    void get_current_transform_matrix(std::span<float, 16> out_matrix) {
        std::copy(current_transform_matrix.begin(), current_transform_matrix.end(), out_matrix.begin());
    }

    void set_current_projection_matrix(std::span<const float, 16> in_matrix) {
        std::copy(in_matrix.begin(), in_matrix.end(), current_projection_matrix.begin());
    }

    void get_current_projection_matrix(std::span<float, 16> out_matrix) {
        std::copy(current_projection_matrix.begin(), current_projection_matrix.end(), out_matrix.begin());
    }

} // namespace alia
