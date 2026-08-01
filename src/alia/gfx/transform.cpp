#include "transform.hpp"
#include "gfx_device.hpp"

namespace alia {

    transform transform::ortho_ui(float width, float height) {
        const vec2f offset = current_pixel_center_offset();
        return ortho(-offset.x, width - offset.x, height - offset.y, -offset.y);
    }

} // namespace alia
