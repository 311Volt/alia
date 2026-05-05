#include "transform.hpp"
#include "gfx_device.hpp"
#include <algorithm>
#include <span>

namespace alia {

static thread_local float current_transform_matrix[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

static thread_local float current_projection_matrix[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

transform get_current_transform() {
    transform t;
    get_current_transform_matrix(std::span<float, 16>(&t.m[0][0], 16));
    return t;
}

void set_current_transform(const transform& t) {
    set_current_transform_matrix(std::span<const float, 16>(&t.m[0][0], 16));
}

transform get_current_projection() {
    transform t;
    get_current_projection_matrix(std::span<float, 16>(&t.m[0][0], 16));
    return t;
}

void set_current_projection(const transform& t) {
    set_current_projection_matrix(std::span<const float, 16>(&t.m[0][0], 16));
}

void set_current_transform_matrix(std::span<const float, 16> m) {
    std::copy(m.begin(), m.end(), current_transform_matrix);
}

void get_current_transform_matrix(std::span<float, 16> m) {
    std::copy(current_transform_matrix, current_transform_matrix + 16, m.begin());
}

void set_current_projection_matrix(std::span<const float, 16> m) {
    std::copy(m.begin(), m.end(), current_projection_matrix);
}

void get_current_projection_matrix(std::span<float, 16> m) {
    std::copy(current_projection_matrix, current_projection_matrix + 16, m.begin());
}

} // namespace alia
