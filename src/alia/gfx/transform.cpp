#include "transform.hpp"
#include "gfx_device.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace alia {
    namespace {
#ifndef NDEBUG
        bool is_finite(vec3f value) {
            return std::isfinite(value.x) &&
                std::isfinite(value.y) &&
                std::isfinite(value.z);
        }
#endif
    } // namespace

    transform transform::look_at(vec3f eye, vec3f target, vec3f up) {
#ifndef NDEBUG
        if (!is_finite(eye) || !is_finite(target) || !is_finite(up))
            throw std::invalid_argument(
                "transform::look_at: eye, target, and up components must be finite");
        if (eye == target)
            throw std::invalid_argument("transform::look_at: eye and target must differ");
        if (up == vec3f{})
            throw std::invalid_argument("transform::look_at: up vector must be nonzero");
        if ((target - eye).cross(up) == vec3f{})
            throw std::invalid_argument(
                "transform::look_at: up vector must not be collinear with "
                "the viewing direction");
#endif

        const vec3f forward = (target - eye).normalized();
        const vec3f side = forward.cross(up).normalized();
        const vec3f camera_up = side.cross(forward);
        transform result = identity();
        result.m[0][0] = side.x;
        result.m[0][1] = camera_up.x;
        result.m[0][2] = -forward.x;
        result.m[1][0] = side.y;
        result.m[1][1] = camera_up.y;
        result.m[1][2] = -forward.y;
        result.m[2][0] = side.z;
        result.m[2][1] = camera_up.z;
        result.m[2][2] = -forward.z;
        result.m[3][0] = -side.dot(eye);
        result.m[3][1] = -camera_up.dot(eye);
        result.m[3][2] = forward.dot(eye);
        return result;
    }

    transform gfx_device::ortho_ui(float width, float height) const {
        if (!backend_)
            throw std::logic_error("gfx_device::ortho_ui: device is not valid");
        const vec2f offset = backend_->pixel_center_offset;
        return transform::ortho(
            -offset.x, width - offset.x, height - offset.y, -offset.y);
    }

    transform gfx_device::perspective_fov(
        float fov_degrees,
        float aspect,
        float near_plane,
        float far_plane) const {
        if (!backend_)
            throw std::logic_error("gfx_device::perspective_fov: device is not valid");

#ifndef NDEBUG
        if (!std::isfinite(fov_degrees) || !std::isfinite(aspect) ||
            !std::isfinite(near_plane) || !std::isfinite(far_plane))
            throw std::invalid_argument(
                "gfx_device::perspective_fov: field of view, aspect ratio, "
                "and clipping planes must be finite");
        if (fov_degrees <= 0.0f || fov_degrees >= 180.0f)
            throw std::invalid_argument(
                "gfx_device::perspective_fov: field of view must be between 0 and 180 degrees");
        if (aspect <= 0.0f)
            throw std::invalid_argument(
                "gfx_device::perspective_fov: aspect ratio must be positive");
        if (near_plane <= 0.0f || far_plane <= near_plane)
            throw std::invalid_argument(
                "gfx_device::perspective_fov: planes must satisfy 0 < near_plane < far_plane");
#endif

        const float radians =
            fov_degrees * std::numbers::pi_v<float> / 180.0f;
        const float y_scale = 1.0f / std::tan(radians * 0.5f);
        transform result{};
        result.m[0][0] = y_scale / aspect;
        result.m[1][1] = y_scale;
        result.m[2][3] = -1.0f;
        if (backend_->clip_depth == clip_depth_range::zero_to_one) {
            result.m[2][2] = far_plane / (near_plane - far_plane);
            result.m[3][2] =
                near_plane * far_plane / (near_plane - far_plane);
        } else {
            result.m[2][2] =
                (far_plane + near_plane) / (near_plane - far_plane);
            result.m[3][2] =
                2.0f * near_plane * far_plane / (near_plane - far_plane);
        }
        return result;
    }

} // namespace alia
