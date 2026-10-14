#ifndef TRANSFORM_D0BF54D2_38DF_4A32_87FE_50E22A30C9FA
#define TRANSFORM_D0BF54D2_38DF_4A32_87FE_50E22A30C9FA

#include "../core/vec.hpp"
#include <cmath>

namespace alia {

// Row-vector matrix: translation is in m[3], and a * b applies a, then b.
// Rotation and shear angles are in radians.
struct transform {
    float m[4][4];

    static transform identity() {
        transform t = {};
        t.m[0][0] = 1; t.m[1][1] = 1; t.m[2][2] = 1; t.m[3][3] = 1;
        return t;
    }

    // Translates x and y, leaving z unchanged.
    static transform translate(vec2f t) {
        transform r = identity();
        r.m[3][0] = t.x;
        r.m[3][1] = t.y;
        return r;
    }

    // Translates all three spatial coordinates.
    static transform translate(vec3f t) {
        transform r = identity();
        r.m[3][0] = t.x;
        r.m[3][1] = t.y;
        r.m[3][2] = t.z;
        return r;
    }

    // Scales x and y, leaving z unchanged.
    static transform scale(vec2f s) {
        transform r = identity();
        r.m[0][0] = s.x;
        r.m[1][1] = s.y;
        return r;
    }

    // Scales all three spatial coordinates independently.
    static transform scale(vec3f s) {
        transform r = identity();
        r.m[0][0] = s.x;
        r.m[1][1] = s.y;
        r.m[2][2] = s.z;
        return r;
    }

    // Uniform 2D scale; z remains unchanged.
    static transform scale(float s) {
        return scale(vec2f{s, s});
    }

    // Equivalent to rotate_z(angle_rad).
    static transform rotate(float angle_rad) {
        return rotate_z(angle_rad);
    }

    // Right-handed rotation about the positive x axis.
    static transform rotate_x(float angle_rad) {
        transform r = identity();
        float c = std::cos(angle_rad);
        float s = std::sin(angle_rad);
        r.m[1][1] = c;  r.m[1][2] = s;
        r.m[2][1] = -s; r.m[2][2] = c;
        return r;
    }

    // Right-handed rotation about the positive y axis.
    static transform rotate_y(float angle_rad) {
        transform r = identity();
        float c = std::cos(angle_rad);
        float s = std::sin(angle_rad);
        r.m[0][0] = c; r.m[0][2] = -s;
        r.m[2][0] = s; r.m[2][2] = c;
        return r;
    }

    // Right-handed rotation about the positive z axis.
    static transform rotate_z(float angle_rad) {
        transform r = identity();
        float c = std::cos(angle_rad);
        float s = std::sin(angle_rad);
        r.m[0][0] = c;  r.m[0][1] = s;
        r.m[1][0] = -s; r.m[1][1] = c;
        return r;
    }

    // Right-handed rotation about a normalized copy of axis.
    // Debug builds throw std::invalid_argument for a zero or non-finite axis.
    static transform rotate(vec3f axis, float angle_rad);

    // Adds tan(angle_rad) * y to x.
    static transform horizontal_shear(float angle_rad) {
        transform r = identity();
        r.m[1][0] = std::tan(angle_rad);
        return r;
    }

    // Adds tan(angle_rad) * x to y.
    static transform vertical_shear(float angle_rad) {
        transform r = identity();
        r.m[0][1] = std::tan(angle_rad);
        return r;
    }

    // Builds a 2D transform by scaling, rotating, then translating.
    static transform build(float angle_rad, vec2f scaling, vec2f translation) {
        return scale(scaling) * rotate(angle_rad) * translate(translation);
    }

    // 2D orthographic projection; use gfx_device::orthographic for 3D clip depth.
    static transform ortho(float l, float r, float b, float t) {
        transform res = {};
        res.m[0][0] = 2.0f / (r - l);
        res.m[1][1] = 2.0f / (t - b);
        res.m[2][2] = -1.0f;
        res.m[3][0] = -(r + l) / (r - l);
        res.m[3][1] = -(t + b) / (t - b);
        res.m[3][3] = 1.0f;
        return res;
    }

    // Right-handed view transform for row-vector multiplication.
    static transform look_at(vec3f eye, vec3f target, vec3f up);

    transform operator*(const transform& o) const {
        transform res = {};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                res.m[i][j] = m[i][0] * o.m[0][j] +
                              m[i][1] * o.m[1][j] +
                              m[i][2] * o.m[2][j] +
                              m[i][3] * o.m[3][j];
            }
        }
        return res;
    }

    transform& operator*=(const transform& o) {
        *this = *this * o;
        return *this;
    }

    // Appends a transform; all then_* members apply after the current transform.
    transform& then(const transform& o) {
        return *this *= o;
    }

    // Appends a translation in 2D or 3D.
    transform& then_translate(vec2f t) { return then(translate(t)); }
    transform& then_translate(vec3f t) { return then(translate(t)); }

    // Appends scaling; the scalar overload scales x and y only.
    transform& then_scale(float s) { return then(scale(s)); }
    transform& then_scale(vec2f s) { return then(scale(s)); }
    transform& then_scale(vec3f s) { return then(scale(s)); }

    // Appends a rotation about z or the given axis.
    transform& then_rotate(float angle_rad) { return then(rotate(angle_rad)); }
    transform& then_rotate(vec3f axis, float angle_rad) {
        return then(rotate(axis, angle_rad));
    }

    // Appends a right-handed rotation about the named axis.
    transform& then_rotate_x(float angle_rad) { return then(rotate_x(angle_rad)); }
    transform& then_rotate_y(float angle_rad) { return then(rotate_y(angle_rad)); }
    transform& then_rotate_z(float angle_rad) { return then(rotate_z(angle_rad)); }

    // Appends a shear in the named direction.
    transform& then_horizontal_shear(float angle_rad) {
        return then(horizontal_shear(angle_rad));
    }
    transform& then_vertical_shear(float angle_rad) {
        return then(vertical_shear(angle_rad));
    }

    // Returns a copy with rows and columns exchanged.
    [[nodiscard]] transform transposed() const {
        transform result{};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.m[i][j] = m[j][i];
        return result;
    }

    // Exchanges rows and columns in place.
    transform& transpose() {
        *this = transposed();
        return *this;
    }

    // Determinant of the full 4x4 matrix.
    [[nodiscard]] float determinant() const;

    // Whether the determinant is finite and its magnitude exceeds tolerance.
    // tolerance must be nonnegative; this uses an absolute determinant threshold.
    [[nodiscard]] bool is_invertible(float tolerance = 1e-7f) const;

    // General 4x4 inverse; throws std::invalid_argument for an exactly zero determinant.
    [[nodiscard]] transform inverted() const;

    // Replaces this matrix with its inverse; leaves it unchanged if inversion throws.
    transform& invert() {
        *this = inverted();
        return *this;
    }

    // Applies to (x, y, 0, 1), returning xy without a perspective divide.
    [[nodiscard]] vec2f apply(vec2f p) const {
        return {
            p.x * m[0][0] + p.y * m[1][0] + m[3][0],
            p.x * m[0][1] + p.y * m[1][1] + m[3][1]
        };
    }

    // Applies to (x, y, z, 1), returning xyz without a perspective divide.
    [[nodiscard]] vec3f apply(vec3f p) const {
        return {
            p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0],
            p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1],
            p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2]
        };
    }

    // Full homogeneous row-vector multiplication, including the supplied w.
    [[nodiscard]] vec4f apply(vec4f p) const {
        return {
            p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + p.w * m[3][0],
            p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + p.w * m[3][1],
            p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + p.w * m[3][2],
            p.x * m[0][3] + p.y * m[1][3] + p.z * m[2][3] + p.w * m[3][3]
        };
    }

    // Applies to (x, y, z, 1) and divides xyz by the resulting w, which must be nonzero.
    [[nodiscard]] vec3f apply_projective(vec3f p) const {
        const vec4f result = apply(vec4f{p.x, p.y, p.z, 1.0f});
        return {result.x / result.w, result.y / result.w, result.z / result.w};
    }

    // Callable aliases for apply; these do not perform a perspective divide.
    [[nodiscard]] vec2f operator()(vec2f p) const { return apply(p); }
    [[nodiscard]] vec3f operator()(vec3f p) const { return apply(p); }
    [[nodiscard]] vec4f operator()(vec4f p) const { return apply(p); }
};

} // namespace alia

#endif /* TRANSFORM_D0BF54D2_38DF_4A32_87FE_50E22A30C9FA */
