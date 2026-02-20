#ifndef ALIA_CORE_VEC_HPP
#define ALIA_CORE_VEC_HPP

#include <cmath>
#include <concepts>
#include <type_traits>
#include "arithmetic.hpp"

namespace alia {

template <typename T>
struct vec2 {
    T x, y;

    constexpr vec2() : x(0), y(0) {}
    constexpr vec2(T x, T y) : x(x), y(y) {}
    template <typename U>
    explicit constexpr vec2(const vec2<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)) {}

    template <typename U = T, typename ResultT = typename detail::square_type_trait<U>::type>
    [[nodiscard]] constexpr ResultT length_squared() const {
        return static_cast<ResultT>(x) * static_cast<ResultT>(x) + static_cast<ResultT>(y) * static_cast<ResultT>(y);
    }

    template <typename U = T, typename ResultT = std::conditional_t<std::is_integral_v<U>, double, U>>
    [[nodiscard]] ResultT length() const {
        return std::sqrt(static_cast<ResultT>(length_squared()));
    }

    template <typename U = T, std::enable_if_t<std::is_floating_point_v<U>, int> = 0>
    [[nodiscard]] vec2 normalized() const {
        using LenT = std::conditional_t<std::is_integral_v<T>, double, T>;
        LenT len = length<T, LenT>();
        if (len == 0) return {0, 0};
        return {x / len, y / len};
    }

    constexpr vec2& operator+=(const vec2& v) { x += v.x; y += v.y; return *this; }
    constexpr vec2& operator-=(const vec2& v) { x -= v.x; y -= v.y; return *this; }
    constexpr vec2& operator*=(T s) { x *= s; y *= s; return *this; }
    constexpr vec2& operator/=(T s) { x /= s; y /= s; return *this; }

    friend constexpr vec2 operator+(vec2 a, const vec2& b) { return a += b; }
    friend constexpr vec2 operator-(vec2 a, const vec2& b) { return a -= b; }
    friend constexpr vec2 operator*(vec2 a, T s) { return a *= s; }
    friend constexpr vec2 operator*(T s, vec2 a) { return a *= s; }
    friend constexpr vec2 operator/(vec2 a, T s) { return a /= s; }

    constexpr bool operator==(const vec2& other) const = default;
};

template <typename T>
struct vec3 {
    T x, y, z;

    constexpr vec3() : x(0), y(0), z(0) {}
    constexpr vec3(T x, T y, T z) : x(x), y(y), z(z) {}
    template <typename U>
    explicit constexpr vec3(const vec3<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)) {}

    template <typename U = T, typename ResultT = typename detail::square_type_trait<U>::type>
    [[nodiscard]] constexpr ResultT length_squared() const {
        return static_cast<ResultT>(x) * static_cast<ResultT>(x) +
               static_cast<ResultT>(y) * static_cast<ResultT>(y) +
               static_cast<ResultT>(z) * static_cast<ResultT>(z);
    }

    template <typename U = T, typename ResultT = std::conditional_t<std::is_integral_v<U>, double, U>>
    [[nodiscard]] ResultT length() const {
        return std::sqrt(static_cast<ResultT>(length_squared()));
    }

    template <typename U = T, std::enable_if_t<std::is_floating_point_v<U>, int> = 0>
    [[nodiscard]] vec3 normalized() const {
        using LenT = std::conditional_t<std::is_integral_v<T>, double, T>;
        LenT len = length<T, LenT>();
        if (len == 0) return {0, 0, 0};
        return {x / len, y / len, z / len};
    }

    constexpr vec3& operator+=(const vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    constexpr vec3& operator-=(const vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    constexpr vec3& operator*=(T s) { x *= s; y *= s; z *= s; return *this; }
    constexpr vec3& operator/=(T s) { x /= s; y /= s; z /= s; return *this; }

    friend constexpr vec3 operator+(vec3 a, const vec3& b) { return a += b; }
    friend constexpr vec3 operator-(vec3 a, const vec3& b) { return a -= b; }
    friend constexpr vec3 operator*(vec3 a, T s) { return a *= s; }
    friend constexpr vec3 operator*(T s, vec3 a) { return a *= s; }
    friend constexpr vec3 operator/(vec3 a, T s) { return a /= s; }

    constexpr bool operator==(const vec3& other) const = default;
};

template <typename T>
struct vec4 {
    T x, y, z, w;

    constexpr vec4() : x(0), y(0), z(0), w(0) {}
    constexpr vec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
    template <typename U>
    explicit constexpr vec4(const vec4<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)), w(static_cast<T>(v.w)) {}

    template <typename U = T, typename ResultT = typename detail::square_type_trait<U>::type>
    [[nodiscard]] constexpr ResultT length_squared() const {
        return static_cast<ResultT>(x) * static_cast<ResultT>(x) +
               static_cast<ResultT>(y) * static_cast<ResultT>(y) +
               static_cast<ResultT>(z) * static_cast<ResultT>(z) +
               static_cast<ResultT>(w) * static_cast<ResultT>(w);
    }

    template <typename U = T, typename ResultT = std::conditional_t<std::is_integral_v<U>, double, U>>
    [[nodiscard]] ResultT length() const {
        return std::sqrt(static_cast<ResultT>(length_squared()));
    }

    template <typename U = T, std::enable_if_t<std::is_floating_point_v<U>, int> = 0>
    [[nodiscard]] vec4 normalized() const {
        using LenT = std::conditional_t<std::is_integral_v<T>, double, T>;
        LenT len = length<T, LenT>();
        if (len == 0) return {0, 0, 0, 0};
        return {x / len, y / len, z / len, w / len};
    }

    constexpr vec4& operator+=(const vec4& v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
    constexpr vec4& operator-=(const vec4& v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
    constexpr vec4& operator*=(T s) { x *= s; y *= s; z *= s; w *= s; return *this; }
    constexpr vec4& operator/=(T s) { x /= s; y /= s; z /= s; w /= s; return *this; }

    friend constexpr vec4 operator+(vec4 a, const vec4& b) { return a += b; }
    friend constexpr vec4 operator-(vec4 a, const vec4& b) { return a -= b; }
    friend constexpr vec4 operator*(vec4 a, T s) { return a *= s; }
    friend constexpr vec4 operator*(T s, vec4 a) { return a *= s; }
    friend constexpr vec4 operator/(vec4 a, T s) { return a /= s; }

    constexpr bool operator==(const vec4& other) const = default;
};

using vec2i = vec2<int>;
using vec2f = vec2<float>;
using vec2d = vec2<double>;

using vec3i = vec3<int>;
using vec3f = vec3<float>;
using vec3d = vec3<double>;

using vec4i = vec4<int>;
using vec4f = vec4<float>;
using vec4d = vec4<double>;

} // namespace alia

#endif // ALIA_CORE_VEC_HPP
