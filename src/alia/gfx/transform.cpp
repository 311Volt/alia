#include "transform.hpp"
#include "gfx_device.hpp"
#include <span>

namespace alia {

transform get_current_transform() {
    transform t;
    current_swapchain().get_transform(std::span<float, 16>(&t.m[0][0], 16));
    return t;
}

void set_current_transform(const transform& t) {
    current_swapchain().set_transform(std::span<const float, 16>(&t.m[0][0], 16));
}

transform get_current_projection() {
    transform t;
    current_swapchain().get_projection(std::span<float, 16>(&t.m[0][0], 16));
    return t;
}

void set_current_projection(const transform& t) {
    current_swapchain().set_projection(std::span<const float, 16>(&t.m[0][0], 16));
}

} // namespace alia
