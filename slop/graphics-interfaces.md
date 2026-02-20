# ALIA Graphics Interfaces

This document defines the C++ concepts, types, and functions for the ALIA graphics subsystem.

---

## Table of Contents

- [ALIA Graphics Interfaces](#alia-graphics-interfaces)
  - [Table of Contents](#table-of-contents)
  - [Core Types](#core-types)
    - [Vec2, Vec3, Vec4](#vec2-vec3-vec4)
    - [Rect](#rect)
    - [Color](#color)
  - [Concepts](#concepts)
    - [Vertex Concepts](#vertex-concepts)
    - [Drawable Concepts](#drawable-concepts)
  - [Pixel Formats](#pixel-formats)
    - [PixelFormat Enum and Utilities](#pixelformat-enum-and-utilities)
  - [Bitmap and Texture](#bitmap-and-texture)
    - [BitmapView (Non-owning)](#bitmapview-non-owning)
    - [Bitmap (Owning)](#bitmap-owning)
    - [Texture (GPU)](#texture-gpu)
  - [Vertex System](#vertex-system)
    - [Vertex Semantics](#vertex-semantics)
    - [Built-in Vertex Types](#built-in-vertex-types)
  - [Primitive Drawing](#primitive-drawing)
    - [Drawing Functions](#drawing-functions)
  - [Transforms](#transforms)
    - [Transform Class](#transform-class)
  - [Shaders](#shaders)
    - [Shader System](#shader-system)
  - [Render Targets](#render-targets)
    - [Framebuffer and Surface](#framebuffer-and-surface)
  - [GPU Buffers](#gpu-buffers)
    - [Vertex and Index Buffers](#vertex-and-index-buffers)
  - [Backend Interface](#backend-interface)
    - [Graphics Backend Interface (detail namespace)](#graphics-backend-interface-detail-namespace)
  - [Capabilities](#capabilities)
    - [Capability Queries](#capability-queries)
  - [Blend Modes](#blend-modes)
    - [Blend State](#blend-state)
  - [Global State](#global-state)
    - [Target Bitmap and Current Display](#target-bitmap-and-current-display)

---

## Core Types

### Vec2, Vec3, Vec4

```cpp
namespace alia {

template<typename T>
struct vec2 {
    T x{}, y{};
    
    // Constructors
    constexpr vec2() = default;
    constexpr vec2(T x, T y) : x(x), y(y) {}
    constexpr explicit vec2(T scalar) : x(scalar), y(scalar) {}
    
    // Type conversion
    template<typename U>
    constexpr explicit vec2(const vec2<U>& other) 
        : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)) {}
    
    template<typename U>
    [[nodiscard]] constexpr vec2<U> as() const {
        return vec2<U>(static_cast<U>(x), static_cast<U>(y));
    }
    
    // Convenience casts
    [[nodiscard]] constexpr vec2<float> f32() const { return as<float>(); }
    [[nodiscard]] constexpr vec2<double> f64() const { return as<double>(); }
    [[nodiscard]] constexpr vec2<int> i32() const { return as<int>(); }
    
    // Arithmetic operators
    constexpr vec2 operator+(const vec2& rhs) const { return {x + rhs.x, y + rhs.y}; }
    constexpr vec2 operator-(const vec2& rhs) const { return {x - rhs.x, y - rhs.y}; }
    constexpr vec2 operator*(T scalar) const { return {x * scalar, y * scalar}; }
    constexpr vec2 operator/(T scalar) const { return {x / scalar, y / scalar}; }
    constexpr vec2 operator-() const { return {-x, -y}; }
    
    constexpr vec2& operator+=(const vec2& rhs) { x += rhs.x; y += rhs.y; return *this; }
    constexpr vec2& operator-=(const vec2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
    constexpr vec2& operator*=(T scalar) { x *= scalar; y *= scalar; return *this; }
    constexpr vec2& operator/=(T scalar) { x /= scalar; y /= scalar; return *this; }
    
    // Comparison
    constexpr bool operator==(const vec2& rhs) const = default;
    
    // Vector operations
    [[nodiscard]] constexpr T dot(const vec2& rhs) const { 
        return x * rhs.x + y * rhs.y; 
    }
    
    [[nodiscard]] constexpr T cross(const vec2& rhs) const {
        return x * rhs.y - y * rhs.x;  // 2D "cross" = z-component of 3D cross
    }
    
    [[nodiscard]] constexpr T length_squared() const { return dot(*this); }
    
    [[nodiscard]] T length() const requires std::floating_point<T> {
        return std::sqrt(length_squared());
    }
    
    [[nodiscard]] vec2 normalized() const requires std::floating_point<T> {
        T len = length();
        return len > T(0) ? *this / len : vec2{};
    }
    
    // Component-wise operations
    [[nodiscard]] constexpr vec2 hadamard(const vec2& rhs) const {
        return {x * rhs.x, y * rhs.y};
    }
    
    [[nodiscard]] constexpr vec2 abs() const {
        return {std::abs(x), std::abs(y)};
    }
    
    [[nodiscard]] constexpr vec2 min(const vec2& rhs) const {
        return {std::min(x, rhs.x), std::min(y, rhs.y)};
    }
    
    [[nodiscard]] constexpr vec2 max(const vec2& rhs) const {
        return {std::max(x, rhs.x), std::max(y, rhs.y)};
    }
    
    // Swizzle (returns new vec2)
    [[nodiscard]] constexpr vec2 yx() const { return {y, x}; }
    [[nodiscard]] constexpr vec2 xx() const { return {x, x}; }
    [[nodiscard]] constexpr vec2 yy() const { return {y, y}; }
    
    // Reduce
    template<typename F>
    [[nodiscard]] constexpr T fold(T init, F&& f) const {
        return f(f(init, x), y);
    }
};

template<typename T>
struct vec3 {
    T x{}, y{}, z{};
    
    constexpr vec3() = default;
    constexpr vec3(T x, T y, T z) : x(x), y(y), z(z) {}
    constexpr explicit vec3(T scalar) : x(scalar), y(scalar), z(scalar) {}
    constexpr vec3(const vec2<T>& xy, T z) : x(xy.x), y(xy.y), z(z) {}
    
    template<typename U>
    constexpr explicit vec3(const vec3<U>& other)
        : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(other.z)) {}
    
    template<typename U>
    [[nodiscard]] constexpr vec3<U> as() const {
        return vec3<U>(static_cast<U>(x), static_cast<U>(y), static_cast<U>(z));
    }
    
    [[nodiscard]] constexpr vec3<float> f32() const { return as<float>(); }
    [[nodiscard]] constexpr vec3<double> f64() const { return as<double>(); }
    
    // Extract vec2
    [[nodiscard]] constexpr vec2<T> xy() const { return {x, y}; }
    [[nodiscard]] constexpr vec2<T> xz() const { return {x, z}; }
    [[nodiscard]] constexpr vec2<T> yz() const { return {y, z}; }
    
    // Arithmetic
    constexpr vec3 operator+(const vec3& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    constexpr vec3 operator-(const vec3& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    constexpr vec3 operator*(T scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    constexpr vec3 operator/(T scalar) const { return {x / scalar, y / scalar, z / scalar}; }
    constexpr vec3 operator-() const { return {-x, -y, -z}; }
    
    constexpr vec3& operator+=(const vec3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
    constexpr vec3& operator-=(const vec3& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
    constexpr vec3& operator*=(T scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
    constexpr vec3& operator/=(T scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }
    
    constexpr bool operator==(const vec3& rhs) const = default;
    
    [[nodiscard]] constexpr T dot(const vec3& rhs) const {
        return x * rhs.x + y * rhs.y + z * rhs.z;
    }
    
    [[nodiscard]] constexpr vec3 cross(const vec3& rhs) const {
        return {
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x
        };
    }
    
    [[nodiscard]] constexpr T length_squared() const { return dot(*this); }
    
    [[nodiscard]] T length() const requires std::floating_point<T> {
        return std::sqrt(length_squared());
    }
    
    [[nodiscard]] vec3 normalized() const requires std::floating_point<T> {
        T len = length();
        return len > T(0) ? *this / len : vec3{};
    }
    
    [[nodiscard]] constexpr vec3 hadamard(const vec3& rhs) const {
        return {x * rhs.x, y * rhs.y, z * rhs.z};
    }
};

template<typename T>
struct vec4 {
    T x{}, y{}, z{}, w{};
    
    constexpr vec4() = default;
    constexpr vec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
    constexpr explicit vec4(T scalar) : x(scalar), y(scalar), z(scalar), w(scalar) {}
    constexpr vec4(const vec3<T>& xyz, T w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
    constexpr vec4(const vec2<T>& xy, const vec2<T>& zw) : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
    
    template<typename U>
    constexpr explicit vec4(const vec4<U>& other)
        : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), 
          z(static_cast<T>(other.z)), w(static_cast<T>(other.w)) {}
    
    // Extract sub-vectors
    [[nodiscard]] constexpr vec2<T> xy() const { return {x, y}; }
    [[nodiscard]] constexpr vec3<T> xyz() const { return {x, y, z}; }
    [[nodiscard]] constexpr vec3<T> rgb() const { return {x, y, z}; }
    
    // Arithmetic (similar to vec3)
    constexpr vec4 operator+(const vec4& rhs) const { 
        return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w}; 
    }
    constexpr vec4 operator-(const vec4& rhs) const { 
        return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w}; 
    }
    constexpr vec4 operator*(T scalar) const { 
        return {x * scalar, y * scalar, z * scalar, w * scalar}; 
    }
    constexpr vec4 operator/(T scalar) const { 
        return {x / scalar, y / scalar, z / scalar, w / scalar}; 
    }
    
    constexpr bool operator==(const vec4& rhs) const = default;
    
    [[nodiscard]] constexpr T dot(const vec4& rhs) const {
        return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
    }
    
    [[nodiscard]] constexpr vec4 hadamard(const vec4& rhs) const {
        return {x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w};
    }
};

// Type aliases
using vec2f = vec2<float>;
using vec2d = vec2<double>;
using vec2i = vec2<int>;
using vec2u = vec2<unsigned>;

using vec3f = vec3<float>;
using vec3d = vec3<double>;
using vec3i = vec3<int>;

using vec4f = vec4<float>;
using vec4d = vec4<double>;
using vec4i = vec4<int>;

} // namespace alia
```

### Rect

```cpp
namespace alia {

template<typename T>
struct rect {
    vec2<T> a{}, b{};  // Top-left and bottom-right corners
    
    constexpr rect() = default;
    constexpr rect(const vec2<T>& a, const vec2<T>& b) : a(a), b(b) {}
    constexpr rect(T x1, T y1, T x2, T y2) : a(x1, y1), b(x2, y2) {}
    
    // Factory methods
    [[nodiscard]] static constexpr rect from_pos_size(const vec2<T>& pos, const vec2<T>& size) {
        return {pos, pos + size};
    }
    
    [[nodiscard]] static constexpr rect xywh(T x, T y, T w, T h) {
        return from_pos_size({x, y}, {w, h});
    }
    
    [[nodiscard]] static constexpr rect centered(const vec2<T>& center, const vec2<T>& size) {
        vec2<T> half = size / T(2);
        return {center - half, center + half};
    }
    
    // Type conversion
    template<typename U>
    constexpr explicit rect(const rect<U>& other)
        : a(other.a.template as<T>()), b(other.b.template as<T>()) {}
    
    // Properties
    [[nodiscard]] constexpr T width() const { return b.x - a.x; }
    [[nodiscard]] constexpr T height() const { return b.y - a.y; }
    [[nodiscard]] constexpr vec2<T> size() const { return {width(), height()}; }
    [[nodiscard]] constexpr vec2<T> center() const { return a + size() / T(2); }
    [[nodiscard]] constexpr T area() const { return width() * height(); }
    
    // Corner accessors
    [[nodiscard]] constexpr vec2<T> top_left() const { return a; }
    [[nodiscard]] constexpr vec2<T> top_right() const { return {b.x, a.y}; }
    [[nodiscard]] constexpr vec2<T> bottom_left() const { return {a.x, b.y}; }
    [[nodiscard]] constexpr vec2<T> bottom_right() const { return b; }
    
    // Translation
    constexpr rect& operator+=(const vec2<T>& v) { a += v; b += v; return *this; }
    constexpr rect& operator-=(const vec2<T>& v) { a -= v; b -= v; return *this; }
    
    constexpr rect operator+(const vec2<T>& v) const { return {a + v, b + v}; }
    constexpr rect operator-(const vec2<T>& v) const { return {a - v, b - v}; }
    
    // Scaling (from position)
    constexpr rect& operator*=(T s) { b = a + size() * s; return *this; }
    constexpr rect& operator/=(T s) { b = a + size() / s; return *this; }
    
    constexpr rect operator*(T s) const { rect r = *this; r *= s; return r; }
    constexpr rect operator/(T s) const { rect r = *this; r /= s; return r; }
    
    constexpr bool operator==(const rect& rhs) const = default;
    
    // Containment tests
    [[nodiscard]] constexpr bool contains(const vec2<T>& p) const {
        return p.x >= a.x && p.x <= b.x && p.y >= a.y && p.y <= b.y;
    }
    
    [[nodiscard]] constexpr bool contains(const rect& r) const {
        return contains(r.a) && contains(r.b);
    }
    
    // Test flags for boundary violations (uses alia::bounds namespace)
    [[nodiscard]] constexpr uint8_t test(const rect& inner) const {
        return (
            (uint8_t(inner.a.x < a.x) << 0) |
            (uint8_t(inner.b.x > b.x) << 1) |
            (uint8_t(inner.a.y < a.y) << 2) |
            (uint8_t(inner.b.y > b.y) << 3)
        );
    }
    
    // Clip a point to rectangle bounds
    [[nodiscard]] constexpr vec2<T> clip(const vec2<T>& p) const {
        return {
            std::clamp(p.x, a.x, b.x),
            std::clamp(p.y, a.y, b.y)
        };
    }
    
    // Intersection (may produce empty rect)
    [[nodiscard]] constexpr rect intersection(const rect& other) const {
        return {clip(other.a), clip(other.b)};
    }
    
    // Union (smallest rect containing both)
    [[nodiscard]] constexpr rect make_union(const rect& other) const {
        return {
            {std::min(a.x, other.a.x), std::min(a.y, other.a.y)},
            {std::max(b.x, other.b.x), std::max(b.y, other.b.y)}
        };
    }
    
    // Clamp inner rect to stay within bounds
    [[nodiscard]] constexpr rect clamp(const rect& inner) const {
        rect result = inner;
        uint8_t t = test(inner);
        if (t & bounds::x_low)  result += {a.x - inner.a.x, T(0)};
        if (t & bounds::x_high) result -= {inner.b.x - b.x, T(0)};
        if (t & bounds::y_low)  result += {T(0), a.y - inner.a.y};
        if (t & bounds::y_high) result -= {T(0), inner.b.y - b.y};
        return result;
    }
    
    // Scale around a center point
    [[nodiscard]] constexpr rect scaled(vec2<double> factor, 
                                         vec2<double> center = {NAN, NAN}) const {
        if (std::isnan(center.x) || std::isnan(center.y)) {
            center = vec2<double>(this->center());
        }
        vec2<double> diff_a = center - vec2<double>(a);
        vec2<double> diff_b = center - vec2<double>(b);
        return rect<double>(
            center - factor.hadamard(diff_a),
            center - factor.hadamard(diff_b)
        );
    }
    
    // Expand/shrink by margin
    [[nodiscard]] constexpr rect expanded(T margin) const {
        return {a - vec2<T>(margin, margin), b + vec2<T>(margin, margin)};
    }
    
    [[nodiscard]] constexpr rect shrunk(T margin) const {
        return expanded(-margin);
    }
};

using rect_f = rect<float>;
using rect_d = rect<double>;
using rect_i = rect<int>;
using rect_u = rect<unsigned>;

// Boundary test flags (for rect::test())
namespace bounds {
    inline constexpr uint8_t x_low  = 0x01;
    inline constexpr uint8_t x_high = 0x02;
    inline constexpr uint8_t x_out  = 0x03;
    inline constexpr uint8_t y_low  = 0x04;
    inline constexpr uint8_t y_high = 0x08;
    inline constexpr uint8_t y_out  = 0x0C;
}

} // namespace alia
```

### Color

```cpp
namespace alia {

struct color {
    float r{0}, g{0}, b{0}, a{1};
    
    constexpr color() = default;
    constexpr color(float r, float g, float b, float a = 1.0f) 
        : r(r), g(g), b(b), a(a) {}
    
    // From integer components [0-255]
    [[nodiscard]] static constexpr color from_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }
    
    // From packed ARGB (common in Windows APIs)
    [[nodiscard]] static constexpr color from_argb32(uint32_t argb) {
        return from_rgba8(
            (argb >> 16) & 0xFF,
            (argb >> 8) & 0xFF,
            argb & 0xFF,
            (argb >> 24) & 0xFF
        );
    }
    
    // From packed RGBA
    [[nodiscard]] static constexpr color from_rgba32(uint32_t rgba) {
        return from_rgba8(
            (rgba >> 24) & 0xFF,
            (rgba >> 16) & 0xFF,
            (rgba >> 8) & 0xFF,
            rgba & 0xFF
        );
    }
    
    // From hex string (e.g., "#FF00FF" or "FF00FF")
    [[nodiscard]] static color from_hex(std::string_view hex);
    
    // HSV/HSL conversions
    [[nodiscard]] static color from_hsv(float h, float s, float v, float a = 1.0f);
    [[nodiscard]] static color from_hsl(float h, float s, float l, float a = 1.0f);
    
    // To packed formats
    [[nodiscard]] constexpr uint32_t to_argb32() const {
        return (uint32_t(a * 255) << 24) | (uint32_t(r * 255) << 16) |
               (uint32_t(g * 255) << 8) | uint32_t(b * 255);
    }
    
    [[nodiscard]] constexpr uint32_t to_rgba32() const {
        return (uint32_t(r * 255) << 24) | (uint32_t(g * 255) << 16) |
               (uint32_t(b * 255) << 8) | uint32_t(a * 255);
    }
    
    // Arithmetic (for blending calculations)
    constexpr color operator+(const color& rhs) const {
        return {r + rhs.r, g + rhs.g, b + rhs.b, a + rhs.a};
    }
    
    constexpr color operator*(float scalar) const {
        return {r * scalar, g * scalar, b * scalar, a * scalar};
    }
    
    constexpr color operator*(const color& rhs) const {
        return {r * rhs.r, g * rhs.g, b * rhs.b, a * rhs.a};
    }
    
    // Interpolation
    [[nodiscard]] constexpr color lerp(const color& other, float t) const {
        return *this * (1.0f - t) + other * t;
    }
    
    // With modified alpha
    [[nodiscard]] constexpr color with_alpha(float new_alpha) const {
        return {r, g, b, new_alpha};
    }
    
    // Premultiply alpha
    [[nodiscard]] constexpr color premultiplied() const {
        return {r * a, g * a, b * a, a};
    }
    
    // Common colors
    static const color White;
    static const color Black;
    static const color Red;
    static const color Green;
    static const color Blue;
    static const color Yellow;
    static const color Cyan;
    static const color Magenta;
    static const color Transparent;
    static const color CornflowerBlue;
};

// Inline color constants
inline constexpr color color::White{1, 1, 1, 1};
inline constexpr color color::Black{0, 0, 0, 1};
inline constexpr color color::Red{1, 0, 0, 1};
inline constexpr color color::Green{0, 1, 0, 1};
inline constexpr color color::Blue{0, 0, 1, 1};
inline constexpr color color::Yellow{1, 1, 0, 1};
inline constexpr color color::Cyan{0, 1, 1, 1};
inline constexpr color color::Magenta{1, 0, 1, 1};
inline constexpr color color::Transparent{0, 0, 0, 0};
inline constexpr color color::CornflowerBlue{0.392f, 0.584f, 0.929f, 1};

// color literal (usage: 0xFF00FF_rgb)
constexpr color operator""_rgb(unsigned long long hex) {
    return color::from_rgba8(
        (hex >> 16) & 0xFF,
        (hex >> 8) & 0xFF,
        hex & 0xFF
    );
}

constexpr color operator""_rgba(unsigned long long hex) {
    return color::from_rgba32(static_cast<uint32_t>(hex));
}

} // namespace alia
```

---

## Concepts

### Vertex Concepts

```cpp
namespace alia {

// Concept for types that can be used as vertex positions
template<typename T>
concept position_type = 
    std::same_as<T, vec2f> || 
    std::same_as<T, vec3f> || 
    std::same_as<T, vec4f>;

// Concept for types that can be used as texture coordinates
template<typename T>
concept tex_coord_type = 
    std::same_as<T, vec2f> || 
    std::same_as<T, vec3f>;

// Concept for types that can be used as vertex colors
template<typename T>
concept color_type = 
    std::same_as<T, color> ||
    std::same_as<T, vec4f> ||
    std::same_as<T, uint32_t>;

// Check if a type has a member named 'pos' that is a position type
template<typename T>
concept has_position = requires(T v) {
    { v.pos } -> position_type;
};

// Check if a type has texture coordinates
template<typename T>
concept has_tex_coord = requires(T v) {
    { v.uv } -> tex_coord_type;
} || requires(T v) {
    { v.uv_px } -> tex_coord_type;
} || requires(T v) {
    { v.uv_norm } -> tex_coord_type;
};

// Check if a type has a color
template<typename T>
concept has_color = requires(T v) {
    { v.color } -> color_type;
};

// A valid vertex type must at least have a position
template<typename T>
concept vertex_type = 
    std::is_trivially_copyable_v<T> &&
    has_position<T>;

// A range of vertices
template<typename R>
concept vertex_range = 
    std::ranges::contiguous_range<R> &&
    vertex_type<std::ranges::range_value_t<R>>;

// A range of indices
template<typename R>
concept index_range = 
    std::ranges::contiguous_range<R> &&
    std::integral<std::ranges::range_value_t<R>>;

} // namespace alia
```

### Drawable Concepts

```cpp
namespace alia {

// Something that can be drawn at a position
template<typename T>
concept drawable = requires(const T& obj, vec2f pos) {
    obj.draw(pos);
};

// Something that can be drawn scaled
template<typename T>
concept scalable_drawable = requires(const T& obj, rect_f src, rect_f dst) {
    obj.draw_scaled(src, dst);
};

// Something that has dimensions
template<typename T>
concept has_size = requires(const T& obj) {
    { obj.size() } -> std::convertible_to<vec2i>;
    { obj.width() } -> std::convertible_to<int>;
    { obj.height() } -> std::convertible_to<int>;
};

// A texture-like object
template<typename T>
concept texture_like = has_size<T> && requires(const T& obj) {
    { obj.native_handle() } -> std::convertible_to<void*>;
};

} // namespace alia
```

---

## Pixel Formats

### PixelFormat Enum and Utilities

```cpp
namespace alia {

// pixel format enumeration
enum class pixel_format : uint32_t {
    // 32-bit formats
    argb8888,       // 8-bit alpha, red, green, blue (default)
    rgba8888,       // 8-bit red, green, blue, alpha
    abgr8888,       // 8-bit alpha, blue, green, red
    xrgb8888,       // 8-bit padding, red, green, blue (no alpha)
    
    // 24-bit formats
    rgb888,         // 8-bit red, green, blue (packed)
    bgr888,         // 8-bit blue, green, red (packed)
    
    // 16-bit formats
    rgb565,         // 5-bit red, 6-bit green, 5-bit blue
    argb4444,       // 4-bit alpha, red, green, blue
    argb1555,       // 1-bit alpha, 5-bit red, green, blue
    
    // 8-bit formats
    l8,             // 8-bit luminance (grayscale)
    a8,             // 8-bit alpha only
    
    // Floating point formats
    rgbaf32,        // 32-bit float per channel (128-bit total)
    rgbf32,         // 32-bit float per channel, no alpha (96-bit)
    rf32,           // Single 32-bit float (grayscale)
};

// pixel format traits
template<pixel_format F>
struct pixel_format_traits;

template<>
struct pixel_format_traits<pixel_format::argb8888> {
    using storage_type = uint32_t;
    static constexpr int bits_per_pixel = 32;
    static constexpr int bytes_per_pixel = 4;
    static constexpr bool has_alpha = true;
};

template<>
struct pixel_format_traits<pixel_format::rgb565> {
    using storage_type = uint16_t;
    static constexpr int bits_per_pixel = 16;
    static constexpr int bytes_per_pixel = 2;
    static constexpr bool has_alpha = false;
};

// ... (other format traits)

// pixel format utilities
namespace pixel {
    // Get bytes per pixel for a format
    [[nodiscard]] constexpr int bytes_per_pixel(pixel_format format);
    
    // Check if format has alpha
    [[nodiscard]] constexpr bool has_alpha(pixel_format format);
    
    // Convert a single pixel between formats
    [[nodiscard]] uint32_t convert(uint32_t pixel, pixel_format from, pixel_format to);
    
    // Convert pixel to color
    [[nodiscard]] color to_color(uint32_t pixel, pixel_format format);
    
    // Convert color to pixel
    [[nodiscard]] uint32_t from_color(color color, pixel_format format);
    
    // Extract color components (0.0 - 1.0)
    [[nodiscard]] float get_red(uint32_t pixel, pixel_format format);
    [[nodiscard]] float get_green(uint32_t pixel, pixel_format format);
    [[nodiscard]] float get_blue(uint32_t pixel, pixel_format format);
    [[nodiscard]] float get_alpha(uint32_t pixel, pixel_format format);
    
    // Pack color components into pixel
    [[nodiscard]] uint32_t pack(float r, float g, float b, float a, pixel_format format);
    
    // Bulk conversion
    void convert_buffer(std::span<const std::byte> src, pixel_format src_format,
                        std::span<std::byte> dst, pixel_format dst_format,
                        size_t pixel_count);
}

} // namespace alia
```

---

## Bitmap and Texture

### BitmapView (Non-owning)

```cpp
namespace alia {

// bitmap_view - non-owning view into pixel data (similar to std::mdspan)
// Supports arbitrary layouts via template parameter
template<typename PixelT = uint32_t, typename layout_policy = layout_stride>
class bitmap_view {
public:
    using pixel_type = PixelT;
    using layout_type = layout_policy;
    
    // Construct from raw data
    bitmap_view(PixelT* data, vec2i size, int pitch_bytes, pixel_format format);
    
    // Construct subview
    bitmap_view(const bitmap_view& parent, rect_i region);
    
    // default/copy/move
    bitmap_view() = default;
    bitmap_view(const bitmap_view&) = default;
    bitmap_view& operator=(const bitmap_view&) = default;
    bitmap_view(bitmap_view&&) = default;
    bitmap_view& operator=(bitmap_view&&) = default;
    
    // Properties
    [[nodiscard]] int width() const { return size_.x; }
    [[nodiscard]] int height() const { return size_.y; }
    [[nodiscard]] vec2i size() const { return size_; }
    [[nodiscard]] rect_i rect() const { return rect_i{{0, 0}, size_}; }
    [[nodiscard]] int pitch() const { return pitch_bytes_; }
    [[nodiscard]] pixel_format format() const { return format_; }
    [[nodiscard]] bool empty() const { return data_ == nullptr; }
    
    // Multi-argument operator[] (C++23)
    [[nodiscard]] PixelT& operator[](int x, int y);
    [[nodiscard]] const PixelT& operator[](int x, int y) const;
    
    // Alternative accessor
    [[nodiscard]] PixelT& at(int x, int y);
    [[nodiscard]] const PixelT& at(int x, int y) const;
    
    // Row access
    [[nodiscard]] std::span<PixelT> row(int y);
    [[nodiscard]] std::span<const PixelT> row(int y) const;
    
    // Raw data access
    [[nodiscard]] PixelT* data() { return data_; }
    [[nodiscard]] const PixelT* data() const { return data_; }
    
    // Create subview (uses deducing this for const correctness)
    template<typename Self>
    [[nodiscard]] auto subview(this Self&& self, rect_i region);
    
    // Typed access (throws if format mismatch)
    template<typename OtherPixelT>
    [[nodiscard]] bitmap_view<OtherPixelT, layout_policy> as() const;
    
private:
    PixelT* data_ = nullptr;
    vec2i size_{0, 0};
    int pitch_bytes_ = 0;
    pixel_format format_ = pixel_format::argb8888;
};

// Common instantiations
using bitmap_view32 = bitmap_view<uint32_t>;
using bitmap_view16 = bitmap_view<uint16_t>;
using bitmap_view_f = bitmap_view<float>;

} // namespace alia
```

### Bitmap (Owning)

```cpp
namespace alia {

// bitmap - owning CPU-side pixel buffer
class bitmap {
public:
    // Construction
    bitmap() = default;
    bitmap(int width, int height, pixel_format format = pixel_format::argb8888);
    bitmap(vec2i size, pixel_format format = pixel_format::argb8888);
    
    // Construct from raw pixel data (explicit)
    explicit bitmap(std::span<const std::byte> raw_data, vec2i size, 
                    pixel_format format = pixel_format::argb8888);
    
    // Construct from bitmap_view (copies data)
    explicit bitmap(const bitmap_view<>& view);
    
    // Move only (no copy - explicit clone() if needed)
    bitmap(bitmap&&) noexcept;
    bitmap& operator=(bitmap&&) noexcept;
    ~bitmap();
    
    bitmap(const bitmap&) = delete;
    bitmap& operator=(const bitmap&) = delete;
    
    // Explicit copy
    [[nodiscard]] bitmap clone() const;
    
    // Properties (Bitmaps are always valid after construction)
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] vec2i size() const;
    [[nodiscard]] rect_i rect() const { return rect_i{{0, 0}, size()}; }
    [[nodiscard]] int pitch() const;  // Bytes per row
    [[nodiscard]] pixel_format format() const;
    [[nodiscard]] size_t data_size() const;  // Total bytes
    
    // Get view for direct pixel access
    [[nodiscard]] bitmap_view<> view();
    [[nodiscard]] bitmap_view<const uint32_t> view() const;
    
    // Create subview (uses deducing this)
    template<typename Self>
    [[nodiscard]] auto subview(this Self&& self, rect_i region);
    
    // Typed access (throws if format mismatch)  
    template<typename PixelT>
    [[nodiscard]] bitmap_view<PixelT> view_as();
    
    template<typename PixelT>
    [[nodiscard]] bitmap_view<const PixelT> view_as() const;
    
    // Raw data access
    [[nodiscard]] std::byte* data();
    [[nodiscard]] const std::byte* data() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Free function to load bitmap from file (handles image format registry)
[[nodiscard]] bitmap load_bitmap(std::string_view path);
[[nodiscard]] bitmap load_bitmap(std::span<const std::byte> file_data);

} // namespace alia
```

### Texture (GPU)

```cpp
namespace alia {

// texture creation flags
enum class texture_flags : uint32_t {
    none = 0,
    
    // filter flags
    min_linear = 1 << 0,          // Linear minification filter
    mag_linear = 1 << 1,          // Linear magnification filter
    mipmap = 1 << 2,             // Generate mipmaps
    
    // Feature flags
    no_premultiplied_alpha = 1 << 4,
    render_target = 1 << 5,       // Can be used as render target
};

constexpr texture_flags operator|(texture_flags a, texture_flags b) {
    return static_cast<texture_flags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

// Lock mode as template parameter for const-correctness
enum class lock_mode {
    read_only,
    write_only,
    read_write
};

// RAII texture lock - provides bitmap_view access to texture data
template<lock_mode Mode>
class texture_lock {
public:
    texture_lock(texture_lock&&) noexcept;
    texture_lock& operator=(texture_lock&&) noexcept;
    ~texture_lock();
    
    // Non-copyable, non-movable during lock
    texture_lock(const texture_lock&) = delete;
    texture_lock& operator=(const texture_lock&) = delete;
    
    // Get view to locked data
    [[nodiscard]] auto view() requires (Mode != lock_mode::write_only) {
        return bitmap_view<const uint32_t>(data_, size_, pitch_, format_);
    }
    
    [[nodiscard]] auto view() requires (Mode == lock_mode::write_only || Mode == lock_mode::read_write) {
        return bitmap_view<uint32_t>(data_, size_, pitch_, format_);
    }
    
    // Properties
    [[nodiscard]] int width() const { return size_.x; }
    [[nodiscard]] int height() const { return size_.y; }
    [[nodiscard]] vec2i size() const { return size_; }
    [[nodiscard]] int pitch() const { return pitch_; }
    
private:
    friend class texture;
    texture_lock(/* internal */);
    
    uint32_t* data_;
    vec2i size_;
    int pitch_;
    pixel_format format_;
    void* texture_handle_;
};

// texture - GPU-side image
class texture {
public:
    // Construction
    texture() = default;
    texture(int width, int height, texture_flags flags = texture_flags::none);
    texture(vec2i size, texture_flags flags = texture_flags::none);
    
    // Create from bitmap (uploads to GPU)
    explicit texture(const bitmap& bitmap, texture_flags flags = texture_flags::none);
    explicit texture(const bitmap_view<>& view, texture_flags flags = texture_flags::none);
    
    // Move only
    texture(texture&&) noexcept;
    texture& operator=(texture&&) noexcept;
    ~texture();
    
    texture(const texture&) = delete;
    texture& operator=(const texture&) = delete;
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] vec2i size() const;
    [[nodiscard]] rect_i rect() const { return rect_i{{0, 0}, size()}; }
    [[nodiscard]] texture_flags flags() const;
    [[nodiscard]] pixel_format format() const;
    
    // Lock for CPU access (downloads from GPU)
    template<lock_mode Mode = lock_mode::read_write>
    [[nodiscard]] texture_lock<Mode> lock();
    
    template<lock_mode Mode = lock_mode::read_write>
    [[nodiscard]] texture_lock<Mode> lock(rect_i region);
    
    // Update from bitmap (partial or full)
    void update(const bitmap& bitmap);
    void update(const bitmap& bitmap, vec2i dest_pos);
    void update(const bitmap_view<>& view, vec2i dest_pos);
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
    // Global settings for new textures
    static void set_new_texture_flags(texture_flags flags);
    static texture_flags get_new_texture_flags();
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Free function to load texture directly from file (loads bitmap, uploads, discards)
[[nodiscard]] texture load_texture(std::string_view path, texture_flags flags = texture_flags::none);
[[nodiscard]] texture load_texture(std::span<const std::byte> file_data, texture_flags flags = texture_flags::none);

// Drawing free functions for texture (fast, GPU-accelerated)
void draw(const texture& texture, vec2f pos, int flags = 0);
void draw_tinted(const texture& texture, vec2f pos, color tint, int flags = 0);
void draw_scaled(const texture& texture, rect_f src, rect_f dst, int flags = 0);
void draw_rotated(const texture& texture, vec2f center, vec2f dst, float angle, int flags = 0);
void draw_region(const texture& texture, rect_f src, vec2f dst, int flags = 0);

// Drawing free functions for bitmap (slow, software rendering - name reflects this)
void draw_software(const bitmap& bitmap, vec2f pos);
void draw_software_tinted(const bitmap& bitmap, vec2f pos, color tint);
void draw_software_scaled(const bitmap& bitmap, rect_f src, rect_f dst);

} // namespace alia
```

---

## Vertex System

### Vertex Semantics

```cpp
namespace alia {

// Semantic meaning of vertex attributes
enum class vertex_semantic : uint32_t {
    position,       // Vertex position (2D, 3D, or 4D)
    tex_coord,       // texture coordinates (pixel or normalized)
    tex_coord_norm,   // Normalized texture coordinates [0,1]
    normal,         // surface normal
    tangent,        // surface tangent
    binormal,       // surface binormal
    color,          // Vertex color
    bone_indices,    // Skeletal animation bone indices
    bone_weights,    // Skeletal animation bone weights
    user_attr0,      // User-defined attribute 0
    user_attr1,      // User-defined attribute 1
    user_attr2,      // User-defined attribute 2
    user_attr3,      // User-defined attribute 3
};

// Data types for vertex attributes
enum class vertex_attr_type : uint32_t {
    float1,
    float2,
    float3,
    float4,
    int1,
    int2,
    int3,
    int4,
    u_byte4,         // 4 unsigned bytes
    u_byte4_norm,     // 4 unsigned bytes, normalized to [0,1]
    short2,
    short2_norm,
    short4,
    short4_norm,
};

// Single vertex attribute descriptor
struct vertex_attribute {
    vertex_semantic semantic;
    vertex_attr_type type;
    uint32_t offset;
};

// Note: Use offsetof() directly to specify attribute offsets.
// Pointer-to-member cannot be converted to offset without undefined behavior.
// Vertex types should be standard layout (static_assert recommended).

// Helper to define vertex attributes
template<vertex_attribute... Attrs>
struct vertex_attribute_list {
    static constexpr std::array<vertex_attribute, sizeof...(Attrs)> attributes = {Attrs...};
};

// Trait to get vertex attributes for a type
template<typename T>
struct vertex_traits {
    // default: try to auto-detect from common member names
    static constexpr auto attributes = detail::auto_detect_vertex_attrs<T>();
};

// Helper function to create attribute list
template<vertex_attribute... Attrs>
consteval auto vertex_attrs() {
    return std::array<vertex_attribute, sizeof...(Attrs)>{Attrs...};
}

// Compile-time vertex declaration
template<typename vertex_t>
class vertex_declaration {
public:
    static constexpr auto& attributes() {
        return vertex_traits<vertex_t>::attributes;
    }
    
    static constexpr size_t stride() {
        return sizeof(vertex_t);
    }
    
    // Get backend-specific vertex declaration handle
    static void* native_handle();
    
private:
    static inline void* cached_handle_ = nullptr;
};

} // namespace alia
```

### Built-in Vertex Types

```cpp
namespace alia {

// Simple 2D vertex with color
struct vertex2_d {
    vec2f pos;
    color color = color::White;
};

// 2D vertex with texture coordinates
struct vertex2_d_tex {
    vec2f pos;
    vec2f uv;       // texture coordinates in pixels
    color color = color::White;
};

// Full 3D vertex
struct vertex3_d {
    vec3f pos;
    vec2f uv;
    vec3f normal;
    color color = color::White;
};

// Skinned vertex for skeletal animation
struct vertex_skinned {
    vec3f pos;
    vec2f uv;
    vec3f normal;
    vec4i bone_indices;
    vec4f bone_weights;
};

// Specializations for built-in types
template<> struct vertex_traits<vertex2_d> {
    static constexpr auto attributes = vertex_attrs<
        vertex_attribute{vertex_semantic::position, vertex_attr_type::float2, 0},
        vertex_attribute{vertex_semantic::color, vertex_attr_type::float4, offsetof(vertex2_d, color)}
    >();
};

template<> struct vertex_traits<vertex2_d_tex> {
    static constexpr auto attributes = vertex_attrs<
        vertex_attribute{vertex_semantic::position, vertex_attr_type::float2, 0},
        vertex_attribute{vertex_semantic::tex_coord, vertex_attr_type::float2, offsetof(vertex2_d_tex, uv)},
        vertex_attribute{vertex_semantic::color, vertex_attr_type::float4, offsetof(vertex2_d_tex, color)}
    >();
};

template<> struct vertex_traits<vertex3_d> {
    static constexpr auto attributes = vertex_attrs<
        vertex_attribute{vertex_semantic::position, vertex_attr_type::float3, 0},
        vertex_attribute{vertex_semantic::tex_coord, vertex_attr_type::float2, offsetof(vertex3_d, uv)},
        vertex_attribute{vertex_semantic::normal, vertex_attr_type::float3, offsetof(vertex3_d, normal)},
        vertex_attribute{vertex_semantic::color, vertex_attr_type::float4, offsetof(vertex3_d, color)}
    >();
};

} // namespace alia
```

---

## Primitive Drawing

### Drawing Functions

```cpp
namespace alia {

// Primitive topology
enum class primitive_type {
    point_list,
    line_list,
    line_strip,
    triangle_list,
    triangle_strip,
    triangle_fan,
};

// Draw primitives from vertex array
template<vertex_range R>
void draw_prim(const R& vertices, 
               primitive_type type = primitive_type::triangle_list,
               const bitmap* texture = nullptr);

// Draw primitives with index buffer
template<vertex_range vr, index_range ir>
void draw_indexed_prim(const vr& vertices, 
                       const ir& indices,
                       primitive_type type = primitive_type::triangle_list,
                       const bitmap* texture = nullptr);

// Convenience wrappers
template<vertex_range R>
void draw_triangles(const R& vertices, const bitmap* texture = nullptr) {
    draw_prim(vertices, primitive_type::triangle_list, texture);
}

template<vertex_range R>
void draw_triangle_strip(const R& vertices, const bitmap* texture = nullptr) {
    draw_prim(vertices, primitive_type::triangle_strip, texture);
}

template<vertex_range R>
void draw_lines(const R& vertices) {
    draw_prim(vertices, primitive_type::line_list, nullptr);
}

// Simple shape drawing (99% use case)
void draw_line(vec2f p1, vec2f p2, color color, float thickness = 1.0f);
void draw_triangle(vec2f p1, vec2f p2, vec2f p3, color color, float thickness = 1.0f);
void draw_rectangle(rect_f rect, color color, float thickness = 1.0f);
void draw_rounded_rectangle(rect_f rect, float rx, float ry, color color, float thickness = 1.0f);
void draw_circle(vec2f center, float radius, color color, float thickness = 1.0f);
void draw_ellipse(vec2f center, float rx, float ry, color color, float thickness = 1.0f);
void draw_arc(vec2f center, float radius, float start_angle, float delta_angle, color color, float thickness = 1.0f);
void draw_spline(std::span<const vec2f> points, color color, float thickness = 1.0f);
void draw_polygon(std::span<const vec2f> points, color color, float thickness = 1.0f);

// Filled shapes
void draw_filled_triangle(vec2f p1, vec2f p2, vec2f p3, color color);
void draw_filled_rectangle(rect_f rect, color color);
void draw_filled_rounded_rectangle(rect_f rect, float rx, float ry, color color);
void draw_filled_circle(vec2f center, float radius, color color);
void draw_filled_ellipse(vec2f center, float rx, float ry, color color);
void draw_filled_pie_slice(vec2f center, float radius, float start_angle, float delta_angle, color color);
void draw_filled_polygon(std::span<const vec2f> points, color color);

// Gradient shapes
void draw_filled_rectangle_gradient(rect_f rect, color tl, color tr, color br, color bl);

} // namespace alia
```

---

## Transforms

### Transform Class

```cpp
namespace alia {

// 4x4 transformation matrix
class transform {
public:
    // Identity transform
    constexpr transform() : m_{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    } {}
    
    // From raw matrix data (column-major)
    explicit transform(std::span<const float, 16> data);
    
    // Access matrix elements (C++23 multi-arg operator[])
    float& operator[](int row, int col) { return m_[col * 4 + row]; }
    float operator[](int row, int col) const { return m_[col * 4 + row]; }
    
    // Raw data access
    [[nodiscard]] std::span<const float, 16> data() const { return m_; }
    [[nodiscard]] std::span<float, 16> data() { return m_; }
    
    // Factory methods
    [[nodiscard]] static transform identity();
    [[nodiscard]] static transform translation(vec3f offset);
    [[nodiscard]] static transform translation(vec2f offset);
    [[nodiscard]] static transform scaling(vec3f factors);
    [[nodiscard]] static transform scaling(vec2f factors);
    [[nodiscard]] static transform scaling(float uniform);
    [[nodiscard]] static transform rotation_x(float radians);
    [[nodiscard]] static transform rotation_y(float radians);
    [[nodiscard]] static transform rotation_z(float radians);
    [[nodiscard]] static transform rotation(vec3f axis, float radians);
    
    // 2D rotation (around Z axis)
    [[nodiscard]] static transform rotation_2d(float radians);
    [[nodiscard]] static transform rotation_2d(vec2f center, float radians);
    
    // Projection matrices
    [[nodiscard]] static transform ortho(float left, float right, float bottom, float top, float near, float far);
    [[nodiscard]] static transform perspective(float fov_y, float aspect, float near, float far);
    [[nodiscard]] static transform perspective_fov(float fov_degrees, float near, float far, float aspect);
    
    // View matrix (camera)
    [[nodiscard]] static transform look_at(vec3f eye, vec3f target, vec3f up);
    [[nodiscard]] static transform camera(vec3f pos, vec3f look_at, vec3f up);
    
    // Chainable transformations (modify and return *this)
    transform& translate(vec3f offset);
    transform& translate(vec2f offset);
    transform& scale(vec3f factors);
    transform& scale(vec2f factors);
    transform& scale(float uniform);
    transform& rotate_x(float radians);
    transform& rotate_y(float radians);
    transform& rotate_z(float radians);
    transform& rotate(vec3f axis, float radians);
    transform& rotate_2d(float radians);
    transform& rotate_2d(vec2f center, float radians);
    
    // Composition
    [[nodiscard]] transform operator*(const transform& rhs) const;
    transform& operator*=(const transform& rhs);
    
    // transform points
    [[nodiscard]] vec2f transform_point(vec2f p) const;
    [[nodiscard]] vec3f transform_point(vec3f p) const;
    [[nodiscard]] vec4f transform_point(vec4f p) const;
    
    // Inverse
    [[nodiscard]] transform inverted() const;
    bool invert();  // In-place, returns false if singular
    
    // Apply to current rendering state
    void use() const;                    // Set as current transform
    void use_projection() const;         // Set as projection matrix
    
    // Get current transforms
    [[nodiscard]] static transform current();
    [[nodiscard]] static transform current_projection();
    
private:
    std::array<float, 16> m_;
};

// RAII scoped transform
class scoped_transform {
public:
    explicit scoped_transform(const transform& t);
    ~scoped_transform();
    
    scoped_transform(const scoped_transform&) = delete;
    scoped_transform& operator=(const scoped_transform&) = delete;
    
private:
    transform saved_;
};

// Global transform functions
void reset_transform();
void reset_projection();

} // namespace alia
```

---

## Shaders

### Shader System

```cpp
namespace alia {

// shader stage
enum class shader_stage {
    Vertex,
    pixel,      // fragment
    geometry,
    compute,
};

// shader source language
enum class shader_language {
    GLSL,       // open_gl Shading Language
    HLSL,       // high-Level Shading Language
    spirv,      // SPIR-V binary
    auto,       // Detect from backend
};

// Compiled shader stage
class shader_module {
public:
    shader_module() = default;
    shader_module(shader_module&&) noexcept;
    shader_module& operator=(shader_module&&) noexcept;
    ~shader_module();
    
    // Factory
    [[nodiscard]] static shader_module compile(shader_stage stage, 
                                               std::string_view source,
                                               shader_language lang = shader_language::auto);
    
    [[nodiscard]] static shader_module load_spirv(shader_stage stage,
                                                  std::span<const uint32_t> spirv);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] shader_stage stage() const;
    
    [[nodiscard]] void* native_handle() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// shader uniform types
using uniform_value = std::variant<
    float, vec2f, vec3f, vec4f,
    int, vec2i, vec3i, vec4i,
    transform,  // mat4
    std::array<float, 9>  // mat3
>;

// Complete shader program
class shader {
public:
    shader() = default;
    shader(shader&&) noexcept;
    shader& operator=(shader&&) noexcept;
    ~shader();
    
    // Build from modules
    class builder {
    public:
        builder& vertex(shader_module module);
        builder& pixel(shader_module module);
        builder& geometry(shader_module module);
        
        // Convenience: compile from source
        builder& vertex_source(std::string_view source, shader_language lang = shader_language::auto);
        builder& pixel_source(std::string_view source, shader_language lang = shader_language::auto);
        
        // Use default shaders
        builder& default_vertex();
        builder& default_pixel();
        
        [[nodiscard]] shader build();
        
    private:
        friend class shader;
        struct BuilderImpl;
        std::unique_ptr<BuilderImpl> impl_;
    };
    
    [[nodiscard]] static builder builder();
    
    // Quick creation for common cases
    [[nodiscard]] static shader from_source(std::string_view vertex_src,
                                             std::string_view pixel_src,
                                             shader_language lang = shader_language::auto);
    
    [[nodiscard]] bool valid() const;
    
    // Activate shader
    void use() const;
    static void use_default();  // Reset to fixed-function / default
    
    // Set uniforms
    void set_uniform(std::string_view name, const uniform_value& value);
    void set_uniform(int location, const uniform_value& value);
    
    // Set texture samplers
    void set_sampler(std::string_view name, const texture& texture, int unit);
    void set_sampler(int location, const texture& texture, int unit);
    
    // Query uniform locations (for caching)
    [[nodiscard]] int get_uniform_location(std::string_view name) const;
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace alia
```

---

## Render Targets

### Framebuffer and Surface

```cpp
namespace alia {

// surface format
enum class surface_format {
    rgba8,
    rgba16_f,
    rgba32_f,
    r8,
    r16_f,
    r32_f,
    depth16,
    depth24,
    depth32_f,
    depth24_stencil8,
    depth32_f_stencil8,
};

// Render target / framebuffer
class surface {
public:
    surface() = default;
    surface(int width, int height, surface_format format = surface_format::rgba8);
    surface(vec2i size, surface_format format = surface_format::rgba8);
    
    surface(surface&&) noexcept;
    surface& operator=(surface&&) noexcept;
    ~surface();
    
    [[nodiscard]] bool valid() const;
    
    // Properties
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] vec2i size() const;
    [[nodiscard]] surface_format format() const;
    
    // Get as bitmap (for reading pixels or using as texture)
    [[nodiscard]] bitmap& as_bitmap();
    [[nodiscard]] const bitmap& as_bitmap() const;
    
    // Set as render target
    void set_as_target();
    
    // Clear operations
    void clear(color color = color::Black);
    void clear_depth(float depth = 1.0f);
    void clear_stencil(int value = 0);
    
    // Depth/stencil configuration
    void set_depth_test(bool enable);
    void set_depth_write(bool enable);
    void set_stencil_test(bool enable);
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
    // Current target
    [[nodiscard]] static surface* current();
    static void reset_target();  // Reset to window backbuffer
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// RAII scoped render target
class scoped_target {
public:
    explicit scoped_target(surface& surface);
    ~scoped_target();
    
    scoped_target(const scoped_target&) = delete;
    scoped_target& operator=(const scoped_target&) = delete;
    
private:
    surface* saved_;
};

} // namespace alia
```

---

## GPU Buffers

### Vertex and Index Buffers

```cpp
namespace alia {

// Buffer usage hints
enum class buffer_usage {
    static,     // Set once, used many times
    dynamic,    // Modified occasionally
    stream,     // Modified every frame
};

// Buffer lock for direct access
template<typename T>
class buffer_lock {
public:
    buffer_lock(buffer_lock&&) noexcept;
    buffer_lock& operator=(buffer_lock&&) noexcept;
    ~buffer_lock();
    
    buffer_lock(const buffer_lock&) = delete;
    buffer_lock& operator=(const buffer_lock&) = delete;
    
    [[nodiscard]] std::span<T> data();
    [[nodiscard]] std::span<const T> data() const;
    
    T& operator[](size_t index) { return data()[index]; }
    const T& operator[](size_t index) const { return data()[index]; }
    
private:
    friend class vertex_buffer;
    friend class index_buffer;
    buffer_lock(/* internal */);
    
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Vertex buffer
class vertex_buffer {
public:
    vertex_buffer() = default;
    
    // Create with initial data
    template<vertex_range R>
    explicit vertex_buffer(const R& vertices, buffer_usage usage = buffer_usage::static);
    
    // Create empty with size
    vertex_buffer(size_t vertex_count, size_t vertex_size, buffer_usage usage = buffer_usage::dynamic);
    
    vertex_buffer(vertex_buffer&&) noexcept;
    vertex_buffer& operator=(vertex_buffer&&) noexcept;
    ~vertex_buffer();
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] size_t size() const;       // Number of vertices
    [[nodiscard]] size_t stride() const;     // Bytes per vertex
    [[nodiscard]] size_t bytes_per_vertex() const; // same as stride()
    [[nodiscard]] buffer_usage usage() const;
    
    // Update data
    template<vertex_range R>
    void update(const R& vertices, size_t offset = 0);
    
    // Lock for direct access
    template<typename vertex_t>
    [[nodiscard]] buffer_lock<vertex_t> lock(size_t offset = 0, size_t count = 0);
    
    template<typename vertex_t>
    [[nodiscard]] buffer_lock<const vertex_t> lock(size_t offset = 0, size_t count = 0) const;
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Index buffer
class index_buffer {
public:
    index_buffer() = default;
    
    // Create with initial data
    template<index_range R>
    explicit index_buffer(const R& indices, buffer_usage usage = buffer_usage::static);
    
    // Create empty with size
    index_buffer(size_t index_count, buffer_usage usage = buffer_usage::dynamic);
    
    index_buffer(index_buffer&&) noexcept;
    index_buffer& operator=(index_buffer&&) noexcept;
    ~index_buffer();
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] size_t size() const;  // Number of indices
    [[nodiscard]] buffer_usage usage() const;
    
    // Update data
    template<index_range R>
    void update(const R& indices, size_t offset = 0);
    
    // Lock for direct access
    [[nodiscard]] buffer_lock<int> lock(size_t offset = 0, size_t count = 0);
    [[nodiscard]] buffer_lock<const int> lock(size_t offset = 0, size_t count = 0) const;
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Draw from GPU buffers
void draw_buffer(const vertex_buffer& vb, 
                 primitive_type type = primitive_type::triangle_list,
                 const bitmap* texture = nullptr);

void draw_indexed_buffer(const vertex_buffer& vb,
                         const index_buffer& ib,
                         primitive_type type = primitive_type::triangle_list,
                         const bitmap* texture = nullptr);

} // namespace alia
```

---

## Backend Interface

### Graphics Backend Interface (detail namespace)

```cpp
namespace alia::detail {

// Resource types for backend tracking
enum class gfx_resource_type : uint32_t {
    bitmap,
    vertex_buffer,
    index_buffer,
    shader,
    surface,
    vertex_declaration,
};

// Resource data for hot-swap
struct gfx_resource_data {
    gfx_resource_type type;
    vec2i size;                         // For bitmaps/surfaces
    pixel_format format;                 // For bitmaps
    std::vector<std::byte> cpu_data;    // CPU-side copy
    void* extra_data;                   // Type-specific extra info
};

// Abstract graphics backend interface
// Implementations live in separate .cpp files
class i_gfx_backend {
public:
    virtual ~i_gfx_backend() = default;
    
    // Lifecycle
    virtual bool initialize(void* window_handle) = 0;
    virtual void shutdown() = 0;
    virtual bool is_valid() const = 0;
    
    // Capabilities
    virtual uint32_t get_capabilities() const = 0;
    virtual limits get_limits() const = 0;
    virtual const char* get_name() const = 0;
    
    // bitmap operations
    virtual void* create_bitmap(int w, int h, uint32_t flags, const void* data) = 0;
    virtual void destroy_bitmap(void* handle) = 0;
    virtual void* lock_bitmap(void* handle, int x, int y, int w, int h, int mode, int* pitch) = 0;
    virtual void unlock_bitmap(void* handle) = 0;
    
    // Buffer operations
    virtual void* create_vertex_buffer(size_t size, uint32_t usage, const void* data) = 0;
    virtual void destroy_vertex_buffer(void* handle) = 0;
    virtual void* lock_vertex_buffer(void* handle, size_t offset, size_t size) = 0;
    virtual void unlock_vertex_buffer(void* handle) = 0;
    
    virtual void* create_index_buffer(size_t size, uint32_t usage, const void* data) = 0;
    virtual void destroy_index_buffer(void* handle) = 0;
    virtual void* lock_index_buffer(void* handle, size_t offset, size_t size) = 0;
    virtual void unlock_index_buffer(void* handle) = 0;
    
    // shader operations
    virtual void* compile_shader(int stage, const char* source, int lang) = 0;
    virtual void destroy_shader_module(void* handle) = 0;
    virtual void* create_shader_program(void* vertex, void* pixel, void* geometry) = 0;
    virtual void destroy_shader_program(void* handle) = 0;
    virtual void use_shader(void* handle) = 0;
    virtual int get_uniform_location(void* handle, const char* name) = 0;
    virtual void set_uniform(void* handle, int location, int type, const void* data) = 0;
    
    // surface operations
    virtual void* create_surface(int w, int h, int format) = 0;
    virtual void destroy_surface(void* handle) = 0;
    virtual void set_render_target(void* handle) = 0;
    
    // Drawing operations
    virtual void clear(float r, float g, float b, float a) = 0;
    virtual void clear_depth(float depth) = 0;
    virtual void draw_primitives(int type, const void* vertices, int vertex_count, 
                                  int vertex_stride, void* vertex_decl, void* texture) = 0;
    virtual void draw_indexed_primitives(int type, const void* vertices, int vertex_count,
                                          int vertex_stride, const int* indices, int index_count,
                                          void* vertex_decl, void* texture) = 0;
    virtual void draw_buffer(void* vb, void* ib, int type, void* vertex_decl, void* texture) = 0;
    
    // State
    virtual void set_transform(const float* matrix) = 0;
    virtual void set_projection(const float* matrix) = 0;
    virtual void set_blend_mode(int src, int dst, int op) = 0;
    virtual void set_depth_test(bool enable) = 0;
    virtual void set_depth_write(bool enable) = 0;
    
    // Frame
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;
    virtual void present() = 0;
};

// Backend registration
struct gfx_backend_info {
    const char* name;
    uint32_t id;
    bool (*is_available)();
    i_gfx_backend* (*create)();
    void (*destroy)(i_gfx_backend*);
};

void register_gfx_backend(const gfx_backend_info& info);
std::span<const gfx_backend_info> get_gfx_backends();
i_gfx_backend* get_current_gfx_backend();
bool switch_gfx_backend(uint32_t backend_id);

} // namespace alia::detail
```

---

## Capabilities

### Capability Queries

```cpp
namespace alia::gfx {

// capability flags (bitfield)
enum class capability : uint32_t {
    // texture capabilities
    texture_npot          = 1 << 0,   // Non-power-of-two textures
    texture_float         = 1 << 1,   // Floating-point textures
    texture_depth         = 1 << 2,   // Depth textures
    texture_array         = 1 << 3,   // texture arrays
    texture3_d            = 1 << 4,   // 3D textures
    texture_cube          = 1 << 5,   // Cubemap textures
    TextureCompressed    = 1 << 6,   // Compressed texture formats (DXT, etc.)
    
    // shader capabilities
    shader_vertex         = 1 << 8,   // Programmable vertex shaders
    shader_pixel          = 1 << 9,   // Programmable pixel/fragment shaders
    shader_geometry       = 1 << 10,  // geometry shaders
    shader_compute        = 1 << 11,  // compute shaders
    ShaderTessellation   = 1 << 12,  // Tessellation shaders
    
    // Rendering features
    framebuffer          = 1 << 16,  // Render-to-texture
    multiple_render_targets= 1 << 17,  // MRT support
    instancing           = 1 << 18,  // Hardware instancing
    indirect_draw         = 1 << 19,  // Indirect drawing
    OcclusionQuery       = 1 << 20,  // Occlusion queries
    
    // Blending
    blend_separate        = 1 << 24,  // Separate RGB/alpha blending
    blend_dual_source      = 1 << 25,  // Dual-source blending
    BlendMinMax          = 1 << 26,  // min/max blend equations
    
    // Other
    anisotropic_filtering = 1 << 28,  // Anisotropic texture filtering
    srgb_textures         = 1 << 29,  // sRGB texture support
    depth_clamp           = 1 << 30,  // Depth clamping
};

constexpr capability operator|(capability a, capability b) {
    return static_cast<capability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr capability operator&(capability a, capability b) {
    return static_cast<capability>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

// Query individual capability
[[nodiscard]] bool has_capability(capability cap);

// Get all capabilities as bitmask
[[nodiscard]] uint32_t get_capabilities();

// Hardware limits
struct limits {
    int max_texture_size;           // Maximum texture dimension
    int max_texture_units;          // Maximum bound textures
    int max_render_targets;         // Maximum simultaneous render targets
    int max_vertex_attributes;      // Maximum vertex attributes
    int max_uniform_block_size;     // Maximum uniform buffer size
    int max_anisotropy;             // Maximum anisotropic filtering level
    int max_viewport_dims[2];       // Maximum viewport dimensions
    int max_clip_planes;            // Maximum user clip planes
    float max_point_size;           // Maximum point sprite size
    float line_width_range[2];      // min/max line width
    float line_width_granularity;   // Line width step
};

[[nodiscard]] const limits& get_limits();

// Backend info
struct backend_info {
    const char* name;               // e.g., "Direct3D 9", "open_gl 4.5"
    const char* vendor;             // GPU vendor
    const char* renderer;           // GPU model
    int version_major;
    int version_minor;
    int shader_model;               // D3D shader model or GLSL version
};

[[nodiscard]] const backend_info& get_backend_info();

// Feature detection helpers
[[nodiscard]] inline bool supports_shaders() {
    return has_capability(capability::shader_vertex) && has_capability(capability::shader_pixel);
}

[[nodiscard]] inline bool supports_render_to_texture() {
    return has_capability(capability::framebuffer);
}

[[nodiscard]] inline bool supports_npot_textures() {
    return has_capability(capability::texture_npot);
}

} // namespace alia::gfx
```

---

## Blend Modes

### Blend State

```cpp
namespace alia::gfx {

// Blend factors
enum class blend_factor {
    zero,
    one,
    src_color,
    inv_src_color,
    src_alpha,
    inv_src_alpha,
    dst_color,
    inv_dst_color,
    dst_alpha,
    inv_dst_alpha,
    src_alpha_sat,
    const_color,
    inv_const_color,
    const_alpha,
    inv_const_alpha,
};

// Blend operations
enum class blend_op {
    add,
    subtract,
    rev_subtract,
    min,
    max,
};

// Complete blend state
struct blend_state {
    bool enabled = true;
    
    blend_factor src_color = blend_factor::src_alpha;
    blend_factor dst_color = blend_factor::inv_src_alpha;
    blend_op color_op = blend_op::add;
    
    blend_factor src_alpha = blend_factor::one;
    blend_factor dst_alpha = blend_factor::inv_src_alpha;
    blend_op alpha_op = blend_op::add;
    
    // Common presets
    static const blend_state opaque;
    static const blend_state alpha;
    static const blend_state additive;
    static const blend_state multiply;
    static const blend_state premultiplied_alpha;
};

inline const blend_state blend_state::opaque = {.enabled = false};
inline const blend_state blend_state::alpha = {};  // default is alpha blend
inline const blend_state blend_state::additive = {
    .src_color = blend_factor::src_alpha,
    .dst_color = blend_factor::one,
    .src_alpha = blend_factor::one,
    .dst_alpha = blend_factor::one,
};
inline const blend_state blend_state::multiply = {
    .src_color = blend_factor::dst_color,
    .dst_color = blend_factor::zero,
};
inline const blend_state blend_state::premultiplied_alpha = {
    .src_color = blend_factor::one,
    .dst_color = blend_factor::inv_src_alpha,
    .src_alpha = blend_factor::one,
    .dst_alpha = blend_factor::inv_src_alpha,
};

// Set current blend state
void set_blend_state(const blend_state& state);

// RAII scoped blend state
class scoped_blend_state {
public:
    explicit scoped_blend_state(const blend_state& state);
    ~scoped_blend_state();
    
private:
    blend_state saved_;
};

} // namespace alia::gfx
```

---

## Global State

### Target Bitmap and Current Display

```cpp
namespace alia {

// Current render target (conceptually similar to axxegro's TargetBitmap)
namespace target {
    // Clear operations
    void clear();
    void clear(color color);
    void clear_depth(float depth = 1.0f);
    void clear_stencil(int value = 0);
    
    // Reset transforms
    void reset_transform();
    void reset_projection();
    
    // Properties of current target
    [[nodiscard]] int width();
    [[nodiscard]] int height();
    [[nodiscard]] vec2i size();
    [[nodiscard]] rect_i rect();
    [[nodiscard]] float aspect_ratio();
    
    // Depth/stencil state
    void set_depth_test(bool enable);
    void set_depth_write(bool enable);
    void set_stencil_test(bool enable);
}

// Current window/display utilities (see os-interfaces.md for window class)
namespace display {
    [[nodiscard]] int width();
    [[nodiscard]] int height();
    [[nodiscard]] vec2i size();
    [[nodiscard]] rect_i rect();
    [[nodiscard]] float aspect_ratio();
    
    void flip();  // Present backbuffer
    void set_vsync(bool enable);
}

} // namespace alia
```
