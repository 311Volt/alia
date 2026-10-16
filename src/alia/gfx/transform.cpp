#include "transform.hpp"
#include "gfx_device.hpp"
#include "../core/angle.hpp"

#include <cmath>
#include <stdexcept>

namespace alia {
    namespace {
        // Evaluate cofactors in double precision to reduce intermediate rounding.
        double cofactor(const transform &value, int row, int column) {
            double minor[3][3];
            int minor_row = 0;
            for (int i = 0; i < 4; ++i) {
                if (i == row)
                    continue;
                int minor_column = 0;
                for (int j = 0; j < 4; ++j) {
                    if (j != column)
                        minor[minor_row][minor_column++] = value.m[i][j];
                }
                ++minor_row;
            }
            const double determinant = minor[0][0] * (minor[1][1] * minor[2][2] - minor[1][2] * minor[2][1]) -
                                       minor[0][1] * (minor[1][0] * minor[2][2] - minor[1][2] * minor[2][0]) +
                                       minor[0][2] * (minor[1][0] * minor[2][1] - minor[1][1] * minor[2][0]);
            return (row + column) % 2 == 0 ? determinant : -determinant;
        }

        double matrix_determinant(const transform &value) {
            double result = 0.0;
            for (int j = 0; j < 4; ++j)
                result += value.m[0][j] * cofactor(value, 0, j);
            return result;
        }

#ifndef NDEBUG
        bool is_finite(vec3f value) {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }
#endif
    } // namespace

    transform transform::rotate(vec3f axis, float angle_rad) {
#ifndef NDEBUG
        if (!is_finite(axis) || axis == vec3f{})
            throw std::invalid_argument("transform::rotate: axis must be nonzero and finite");
#endif

        const double length =
            std::hypot(static_cast<double>(axis.x), static_cast<double>(axis.y), static_cast<double>(axis.z));
        const float x = static_cast<float>(axis.x / length);
        const float y = static_cast<float>(axis.y / length);
        const float z = static_cast<float>(axis.z / length);
        const float c = std::cos(angle_rad);
        const float s = std::sin(angle_rad);
        const float k = 1.0f - c;
        transform result = identity();
        result.m[0][0] = c + x * x * k;
        result.m[0][1] = x * y * k + z * s;
        result.m[0][2] = x * z * k - y * s;
        result.m[1][0] = x * y * k - z * s;
        result.m[1][1] = c + y * y * k;
        result.m[1][2] = y * z * k + x * s;
        result.m[2][0] = x * z * k + y * s;
        result.m[2][1] = y * z * k - x * s;
        result.m[2][2] = c + z * z * k;
        return result;
    }

    float transform::determinant() const {
        return static_cast<float>(matrix_determinant(*this));
    }

    bool transform::is_invertible(float tolerance) const {
        const double det = matrix_determinant(*this);
        return tolerance >= 0.0f && std::isfinite(det) && std::abs(det) > tolerance;
    }

    transform transform::inverted() const {
        double cofactors[4][4];
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                cofactors[i][j] = cofactor(*this, i, j);
        double det = 0.0;
        for (int j = 0; j < 4; ++j)
            det += m[0][j] * cofactors[0][j];
        if (det == 0.0)
            throw std::invalid_argument("transform::inverted: matrix is singular");

        transform result{};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.m[i][j] = static_cast<float>(cofactors[j][i] / det);
        return result;
    }

    transform transform::look_at(vec3f eye, vec3f target, vec3f up) {
#ifndef NDEBUG
        if (!is_finite(eye) || !is_finite(target) || !is_finite(up))
            throw std::invalid_argument("transform::look_at: eye, target, and up components must be finite");
        if (eye == target)
            throw std::invalid_argument("transform::look_at: eye and target must differ");
        if (up == vec3f{})
            throw std::invalid_argument("transform::look_at: up vector must be nonzero");
        if ((target - eye).cross(up) == vec3f{})
            throw std::invalid_argument("transform::look_at: up vector must not be collinear with "
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
        return transform::ortho(-offset.x, width - offset.x, height - offset.y, -offset.y);
    }

    transform gfx_device::orthographic(float l, float r, float b, float t, float near_plane, float far_plane) const {
        if (!backend_)
            throw std::logic_error("gfx_device::orthographic: device is not valid");

#ifndef NDEBUG
        if (!std::isfinite(l) || !std::isfinite(r) || !std::isfinite(b) || !std::isfinite(t) ||
            !std::isfinite(near_plane) || !std::isfinite(far_plane))
            throw std::invalid_argument("gfx_device::orthographic: bounds and clipping planes must be finite");
        if (l == r || b == t || near_plane == far_plane)
            throw std::invalid_argument("gfx_device::orthographic: bounds and clipping planes must differ");
#endif

        transform result{};
        result.m[0][0] = 2.0f / (r - l);
        result.m[1][1] = 2.0f / (t - b);
        result.m[3][0] = -(r + l) / (r - l);
        result.m[3][1] = -(t + b) / (t - b);
        result.m[3][3] = 1.0f;
        if (backend_->clip_depth == clip_depth_range::zero_to_one) {
            result.m[2][2] = -1.0f / (far_plane - near_plane);
            result.m[3][2] = -near_plane / (far_plane - near_plane);
        } else {
            result.m[2][2] = -2.0f / (far_plane - near_plane);
            result.m[3][2] = -(far_plane + near_plane) / (far_plane - near_plane);
        }
        return result;
    }

    transform gfx_device::perspective(float l, float r, float b, float t, float near_plane, float far_plane) const {
        if (!backend_)
            throw std::logic_error("gfx_device::perspective: device is not valid");

#ifndef NDEBUG
        if (!std::isfinite(l) || !std::isfinite(r) || !std::isfinite(b) || !std::isfinite(t) ||
            !std::isfinite(near_plane) || !std::isfinite(far_plane))
            throw std::invalid_argument("gfx_device::perspective: bounds and clipping planes must be finite");
        if (l == r || b == t)
            throw std::invalid_argument("gfx_device::perspective: bounds must differ");
        if (near_plane <= 0.0f || far_plane <= near_plane)
            throw std::invalid_argument("gfx_device::perspective: planes must satisfy 0 < near_plane < far_plane");
#endif

        transform result{};
        result.m[0][0] = 2.0f * near_plane / (r - l);
        result.m[1][1] = 2.0f * near_plane / (t - b);
        result.m[2][0] = (r + l) / (r - l);
        result.m[2][1] = (t + b) / (t - b);
        result.m[2][3] = -1.0f;
        if (backend_->clip_depth == clip_depth_range::zero_to_one) {
            result.m[2][2] = far_plane / (near_plane - far_plane);
            result.m[3][2] = near_plane * far_plane / (near_plane - far_plane);
        } else {
            result.m[2][2] = (far_plane + near_plane) / (near_plane - far_plane);
            result.m[3][2] = 2.0f * near_plane * far_plane / (near_plane - far_plane);
        }
        return result;
    }

    transform gfx_device::perspective_vfov(float vfov_degrees, float aspect, float near_plane, float far_plane) const {
        if (!backend_)
            throw std::logic_error("gfx_device::perspective_vfov: device is not valid");

#ifndef NDEBUG
        if (!std::isfinite(vfov_degrees) || !std::isfinite(aspect) || !std::isfinite(near_plane) ||
            !std::isfinite(far_plane))
            throw std::invalid_argument("gfx_device::perspective_vfov: field of view, aspect ratio, "
                                        "and clipping planes must be finite");
        if (vfov_degrees <= 0.0f || vfov_degrees >= 180.0f)
            throw std::invalid_argument("gfx_device::perspective_vfov: field of view must be between 0 and 180 degrees"
            );
        if (aspect <= 0.0f)
            throw std::invalid_argument("gfx_device::perspective_vfov: aspect ratio must be positive");
        if (near_plane <= 0.0f || far_plane <= near_plane)
            throw std::invalid_argument("gfx_device::perspective_vfov: planes must satisfy 0 < near_plane < far_plane");
#endif

        const float h = near_plane * std::tan(deg_to_rad(vfov_degrees) * 0.5f);
        const float w = h * aspect;
        return perspective(-w, w, -h, h, near_plane, far_plane);
    }

    transform gfx_device::perspective_hfov(float hfov_degrees, float aspect, float near_plane, float far_plane) const {
        if (!backend_)
            throw std::logic_error("gfx_device::perspective_hfov: device is not valid");

#ifndef NDEBUG
        if (!std::isfinite(hfov_degrees) || !std::isfinite(aspect) || !std::isfinite(near_plane) ||
            !std::isfinite(far_plane))
            throw std::invalid_argument("gfx_device::perspective_hfov: field of view, aspect ratio, "
                                        "and clipping planes must be finite");
        if (hfov_degrees <= 0.0f || hfov_degrees >= 180.0f)
            throw std::invalid_argument("gfx_device::perspective_hfov: field of view must be between 0 and 180 degrees"
            );
        if (aspect <= 0.0f)
            throw std::invalid_argument("gfx_device::perspective_hfov: aspect ratio must be positive");
        if (near_plane <= 0.0f || far_plane <= near_plane)
            throw std::invalid_argument("gfx_device::perspective_hfov: planes must satisfy 0 < near_plane < far_plane");
#endif

        const float vfov = 2.0f * std::atan(std::tan(deg_to_rad(hfov_degrees) * 0.5f) / aspect);
        return perspective_vfov(rad_to_deg(vfov), aspect, near_plane, far_plane);
    }

} // namespace alia
