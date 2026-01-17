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
struct Vec2 {
    T x{}, y{};
    
    // Constructors
    constexpr Vec2() = default;
    constexpr Vec2(T x, T y) : x(x), y(y) {}
    constexpr explicit Vec2(T scalar) : x(scalar), y(scalar) {}
    
    // Type conversion
    template<typename U>
    constexpr explicit Vec2(const Vec2<U>& other) 
        : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)) {}
    
    template<typename U>
    [[nodiscard]] constexpr Vec2<U> as() const {
        return Vec2<U>(static_cast<U>(x), static_cast<U>(y));
    }
    
    // Convenience casts
    [[nodiscard]] constexpr Vec2<float> f32() const { return as<float>(); }
    [[nodiscard]] constexpr Vec2<double> f64() const { return as<double>(); }
    [[nodiscard]] constexpr Vec2<int> i32() const { return as<int>(); }
    
    // Arithmetic operators
    constexpr Vec2 operator+(const Vec2& rhs) const { return {x + rhs.x, y + rhs.y}; }
    constexpr Vec2 operator-(const Vec2& rhs) const { return {x - rhs.x, y - rhs.y}; }
    constexpr Vec2 operator*(T scalar) const { return {x * scalar, y * scalar}; }
    constexpr Vec2 operator/(T scalar) const { return {x / scalar, y / scalar}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }
    
    constexpr Vec2& operator+=(const Vec2& rhs) { x += rhs.x; y += rhs.y; return *this; }
    constexpr Vec2& operator-=(const Vec2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
    constexpr Vec2& operator*=(T scalar) { x *= scalar; y *= scalar; return *this; }
    constexpr Vec2& operator/=(T scalar) { x /= scalar; y /= scalar; return *this; }
    
    // Comparison
    constexpr bool operator==(const Vec2& rhs) const = default;
    
    // Vector operations
    [[nodiscard]] constexpr T dot(const Vec2& rhs) const { 
        return x * rhs.x + y * rhs.y; 
    }
    
    [[nodiscard]] constexpr T cross(const Vec2& rhs) const {
        return x * rhs.y - y * rhs.x;  // 2D "cross" = z-component of 3D cross
    }
    
    [[nodiscard]] constexpr T length_squared() const { return dot(*this); }
    
    [[nodiscard]] T length() const requires std::floating_point<T> {
        return std::sqrt(length_squared());
    }
    
    [[nodiscard]] Vec2 normalized() const requires std::floating_point<T> {
        T len = length();
        return len > T(0) ? *this / len : Vec2{};
    }
    
    // Component-wise operations
    [[nodiscard]] constexpr Vec2 hadamard(const Vec2& rhs) const {
        return {x * rhs.x, y * rhs.y};
    }
    
    [[nodiscard]] constexpr Vec2 abs() const {
        return {std::abs(x), std::abs(y)};
    }
    
    [[nodiscard]] constexpr Vec2 min(const Vec2& rhs) const {
        return {std::min(x, rhs.x), std::min(y, rhs.y)};
    }
    
    [[nodiscard]] constexpr Vec2 max(const Vec2& rhs) const {
        return {std::max(x, rhs.x), std::max(y, rhs.y)};
    }
    
    // Swizzle (returns new Vec2)
    [[nodiscard]] constexpr Vec2 yx() const { return {y, x}; }
    [[nodiscard]] constexpr Vec2 xx() const { return {x, x}; }
    [[nodiscard]] constexpr Vec2 yy() const { return {y, y}; }
    
    // Reduce
    template<typename F>
    [[nodiscard]] constexpr T fold(T init, F&& f) const {
        return f(f(init, x), y);
    }
};

template<typename T>
struct Vec3 {
    T x{}, y{}, z{};
    
    constexpr Vec3() = default;
    constexpr Vec3(T x, T y, T z) : x(x), y(y), z(z) {}
    constexpr explicit Vec3(T scalar) : x(scalar), y(scalar), z(scalar) {}
    constexpr Vec3(const Vec2<T>& xy, T z) : x(xy.x), y(xy.y), z(z) {}
    
    template<typename U>
    constexpr explicit Vec3(const Vec3<U>& other)
        : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(other.z)) {}
    
    template<typename U>
    [[nodiscard]] constexpr Vec3<U> as() const {
        return Vec3<U>(static_cast<U>(x), static_cast<U>(y), static_cast<U>(z));
    }
    
    [[nodiscard]] constexpr Vec3<float> f32() const { return as<float>(); }
    [[nodiscard]] constexpr Vec3<double> f64() const { return as<double>(); }
    
    // Extract Vec2
    [[nodiscard]] constexpr Vec2<T> xy() const { return {x, y}; }
    [[nodiscard]] constexpr Vec2<T> xz() const { return {x, z}; }
    [[nodiscard]] constexpr Vec2<T> yz() const { return {y, z}; }
    
    // Arithmetic
    constexpr Vec3 operator+(const Vec3& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    constexpr Vec3 operator-(const Vec3& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    constexpr Vec3 operator*(T scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    constexpr Vec3 operator/(T scalar) const { return {x / scalar, y / scalar, z / scalar}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    
    constexpr Vec3& operator+=(const Vec3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
    constexpr Vec3& operator-=(const Vec3& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
    constexpr Vec3& operator*=(T scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
    constexpr Vec3& operator/=(T scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }
    
    constexpr bool operator==(const Vec3& rhs) const = default;
    
    [[nodiscard]] constexpr T dot(const Vec3& rhs) const {
        return x * rhs.x + y * rhs.y + z * rhs.z;
    }
    
    [[nodiscard]] constexpr Vec3 cross(const Vec3& rhs) const {
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
    
    [[nodiscard]] Vec3 normalized() const requires std::floating_point<T> {
        T len = length();
        return len > T(0) ? *this / len : Vec3{};
    }
    
    [[nodiscard]] constexpr Vec3 hadamard(const Vec3& rhs) const {
        return {x * rhs.x, y * rhs.y, z * rhs.z};
    }
};

template<typename T>
struct Vec4 {
    T x{}, y{}, z{}, w{};
    
    constexpr Vec4() = default;
    constexpr Vec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
    constexpr explicit Vec4(T scalar) : x(scalar), y(scalar), z(scalar), w(scalar) {}
    constexpr Vec4(const Vec3<T>& xyz, T w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
    constexpr Vec4(const Vec2<T>& xy, const Vec2<T>& zw) : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
    
    template<typename U>
    constexpr explicit Vec4(const Vec4<U>& other)
        : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), 
          z(static_cast<T>(other.z)), w(static_cast<T>(other.w)) {}
    
    // Extract sub-vectors
    [[nodiscard]] constexpr Vec2<T> xy() const { return {x, y}; }
    [[nodiscard]] constexpr Vec3<T> xyz() const { return {x, y, z}; }
    [[nodiscard]] constexpr Vec3<T> rgb() const { return {x, y, z}; }
    
    // Arithmetic (similar to Vec3)
    constexpr Vec4 operator+(const Vec4& rhs) const { 
        return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w}; 
    }
    constexpr Vec4 operator-(const Vec4& rhs) const { 
        return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w}; 
    }
    constexpr Vec4 operator*(T scalar) const { 
        return {x * scalar, y * scalar, z * scalar, w * scalar}; 
    }
    constexpr Vec4 operator/(T scalar) const { 
        return {x / scalar, y / scalar, z / scalar, w / scalar}; 
    }
    
    constexpr bool operator==(const Vec4& rhs) const = default;
    
    [[nodiscard]] constexpr T dot(const Vec4& rhs) const {
        return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
    }
    
    [[nodiscard]] constexpr Vec4 hadamard(const Vec4& rhs) const {
        return {x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w};
    }
};

// Type aliases
using Vec2f = Vec2<float>;
using Vec2d = Vec2<double>;
using Vec2i = Vec2<int>;
using Vec2u = Vec2<unsigned>;

using Vec3f = Vec3<float>;
using Vec3d = Vec3<double>;
using Vec3i = Vec3<int>;

using Vec4f = Vec4<float>;
using Vec4d = Vec4<double>;
using Vec4i = Vec4<int>;

} // namespace alia
```

### Rect

```cpp
namespace alia {

template<typename T>
struct Rect {
    Vec2<T> a{}, b{};  // Top-left and bottom-right corners
    
    constexpr Rect() = default;
    constexpr Rect(const Vec2<T>& a, const Vec2<T>& b) : a(a), b(b) {}
    constexpr Rect(T x1, T y1, T x2, T y2) : a(x1, y1), b(x2, y2) {}
    
    // Factory methods
    [[nodiscard]] static constexpr Rect from_pos_size(const Vec2<T>& pos, const Vec2<T>& size) {
        return {pos, pos + size};
    }
    
    [[nodiscard]] static constexpr Rect xywh(T x, T y, T w, T h) {
        return from_pos_size({x, y}, {w, h});
    }
    
    [[nodiscard]] static constexpr Rect centered(const Vec2<T>& center, const Vec2<T>& size) {
        Vec2<T> half = size / T(2);
        return {center - half, center + half};
    }
    
    // Type conversion
    template<typename U>
    constexpr explicit Rect(const Rect<U>& other)
        : a(other.a.template as<T>()), b(other.b.template as<T>()) {}
    
    // Properties
    [[nodiscard]] constexpr T width() const { return b.x - a.x; }
    [[nodiscard]] constexpr T height() const { return b.y - a.y; }
    [[nodiscard]] constexpr Vec2<T> size() const { return {width(), height()}; }
    [[nodiscard]] constexpr Vec2<T> center() const { return a + size() / T(2); }
    [[nodiscard]] constexpr T area() const { return width() * height(); }
    
    // Corner accessors
    [[nodiscard]] constexpr Vec2<T> top_left() const { return a; }
    [[nodiscard]] constexpr Vec2<T> top_right() const { return {b.x, a.y}; }
    [[nodiscard]] constexpr Vec2<T> bottom_left() const { return {a.x, b.y}; }
    [[nodiscard]] constexpr Vec2<T> bottom_right() const { return b; }
    
    // Translation
    constexpr Rect& operator+=(const Vec2<T>& v) { a += v; b += v; return *this; }
    constexpr Rect& operator-=(const Vec2<T>& v) { a -= v; b -= v; return *this; }
    
    constexpr Rect operator+(const Vec2<T>& v) const { return {a + v, b + v}; }
    constexpr Rect operator-(const Vec2<T>& v) const { return {a - v, b - v}; }
    
    // Scaling (from position)
    constexpr Rect& operator*=(T s) { b = a + size() * s; return *this; }
    constexpr Rect& operator/=(T s) { b = a + size() / s; return *this; }
    
    constexpr Rect operator*(T s) const { Rect r = *this; r *= s; return r; }
    constexpr Rect operator/(T s) const { Rect r = *this; r /= s; return r; }
    
    constexpr bool operator==(const Rect& rhs) const = default;
    
    // Containment tests
    [[nodiscard]] constexpr bool contains(const Vec2<T>& p) const {
        return p.x >= a.x && p.x <= b.x && p.y >= a.y && p.y <= b.y;
    }
    
    [[nodiscard]] constexpr bool contains(const Rect& r) const {
        return contains(r.a) && contains(r.b);
    }
    
    // Test flags for boundary violations (uses alia::bounds namespace)
    [[nodiscard]] constexpr uint8_t test(const Rect& inner) const {
        return (
            (uint8_t(inner.a.x < a.x) << 0) |
            (uint8_t(inner.b.x > b.x) << 1) |
            (uint8_t(inner.a.y < a.y) << 2) |
            (uint8_t(inner.b.y > b.y) << 3)
        );
    }
    
    // Clip a point to rectangle bounds
    [[nodiscard]] constexpr Vec2<T> clip(const Vec2<T>& p) const {
        return {
            std::clamp(p.x, a.x, b.x),
            std::clamp(p.y, a.y, b.y)
        };
    }
    
    // Intersection (may produce empty rect)
    [[nodiscard]] constexpr Rect intersection(const Rect& other) const {
        return {clip(other.a), clip(other.b)};
    }
    
    // Union (smallest rect containing both)
    [[nodiscard]] constexpr Rect make_union(const Rect& other) const {
        return {
            {std::min(a.x, other.a.x), std::min(a.y, other.a.y)},
            {std::max(b.x, other.b.x), std::max(b.y, other.b.y)}
        };
    }
    
    // Clamp inner rect to stay within bounds
    [[nodiscard]] constexpr Rect clamp(const Rect& inner) const {
        Rect result = inner;
        uint8_t t = test(inner);
        if (t & bounds::x_low)  result += {a.x - inner.a.x, T(0)};
        if (t & bounds::x_high) result -= {inner.b.x - b.x, T(0)};
        if (t & bounds::y_low)  result += {T(0), a.y - inner.a.y};
        if (t & bounds::y_high) result -= {T(0), inner.b.y - b.y};
        return result;
    }
    
    // Scale around a center point
    [[nodiscard]] constexpr Rect scaled(Vec2<double> factor, 
                                         Vec2<double> center = {NAN, NAN}) const {
        if (std::isnan(center.x) || std::isnan(center.y)) {
            center = Vec2<double>(this->center());
        }
        Vec2<double> diff_a = center - Vec2<double>(a);
        Vec2<double> diff_b = center - Vec2<double>(b);
        return Rect<double>(
            center - factor.hadamard(diff_a),
            center - factor.hadamard(diff_b)
        );
    }
    
    // Expand/shrink by margin
    [[nodiscard]] constexpr Rect expanded(T margin) const {
        return {a - Vec2<T>(margin, margin), b + Vec2<T>(margin, margin)};
    }
    
    [[nodiscard]] constexpr Rect shrunk(T margin) const {
        return expanded(-margin);
    }
};

using RectF = Rect<float>;
using RectD = Rect<double>;
using RectI = Rect<int>;
using RectU = Rect<unsigned>;

// Boundary test flags (for Rect::test())
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

struct Color {
    float r{0}, g{0}, b{0}, a{1};
    
    constexpr Color() = default;
    constexpr Color(float r, float g, float b, float a = 1.0f) 
        : r(r), g(g), b(b), a(a) {}
    
    // From integer components [0-255]
    [[nodiscard]] static constexpr Color from_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }
    
    // From packed ARGB (common in Windows APIs)
    [[nodiscard]] static constexpr Color from_argb32(uint32_t argb) {
        return from_rgba8(
            (argb >> 16) & 0xFF,
            (argb >> 8) & 0xFF,
            argb & 0xFF,
            (argb >> 24) & 0xFF
        );
    }
    
    // From packed RGBA
    [[nodiscard]] static constexpr Color from_rgba32(uint32_t rgba) {
        return from_rgba8(
            (rgba >> 24) & 0xFF,
            (rgba >> 16) & 0xFF,
            (rgba >> 8) & 0xFF,
            rgba & 0xFF
        );
    }
    
    // From hex string (e.g., "#FF00FF" or "FF00FF")
    [[nodiscard]] static Color from_hex(std::string_view hex);
    
    // HSV/HSL conversions
    [[nodiscard]] static Color from_hsv(float h, float s, float v, float a = 1.0f);
    [[nodiscard]] static Color from_hsl(float h, float s, float l, float a = 1.0f);
    
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
    constexpr Color operator+(const Color& rhs) const {
        return {r + rhs.r, g + rhs.g, b + rhs.b, a + rhs.a};
    }
    
    constexpr Color operator*(float scalar) const {
        return {r * scalar, g * scalar, b * scalar, a * scalar};
    }
    
    constexpr Color operator*(const Color& rhs) const {
        return {r * rhs.r, g * rhs.g, b * rhs.b, a * rhs.a};
    }
    
    // Interpolation
    [[nodiscard]] constexpr Color lerp(const Color& other, float t) const {
        return *this * (1.0f - t) + other * t;
    }
    
    // With modified alpha
    [[nodiscard]] constexpr Color with_alpha(float new_alpha) const {
        return {r, g, b, new_alpha};
    }
    
    // Premultiply alpha
    [[nodiscard]] constexpr Color premultiplied() const {
        return {r * a, g * a, b * a, a};
    }
    
    // Common colors
    static const Color White;
    static const Color Black;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Yellow;
    static const Color Cyan;
    static const Color Magenta;
    static const Color Transparent;
    static const Color CornflowerBlue;
};

// Inline color constants
inline constexpr Color Color::White{1, 1, 1, 1};
inline constexpr Color Color::Black{0, 0, 0, 1};
inline constexpr Color Color::Red{1, 0, 0, 1};
inline constexpr Color Color::Green{0, 1, 0, 1};
inline constexpr Color Color::Blue{0, 0, 1, 1};
inline constexpr Color Color::Yellow{1, 1, 0, 1};
inline constexpr Color Color::Cyan{0, 1, 1, 1};
inline constexpr Color Color::Magenta{1, 0, 1, 1};
inline constexpr Color Color::Transparent{0, 0, 0, 0};
inline constexpr Color Color::CornflowerBlue{0.392f, 0.584f, 0.929f, 1};

// Color literal (usage: 0xFF00FF_rgb)
constexpr Color operator""_rgb(unsigned long long hex) {
    return Color::from_rgba8(
        (hex >> 16) & 0xFF,
        (hex >> 8) & 0xFF,
        hex & 0xFF
    );
}

constexpr Color operator""_rgba(unsigned long long hex) {
    return Color::from_rgba32(static_cast<uint32_t>(hex));
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
concept PositionType = 
    std::same_as<T, Vec2f> || 
    std::same_as<T, Vec3f> || 
    std::same_as<T, Vec4f>;

// Concept for types that can be used as texture coordinates
template<typename T>
concept TexCoordType = 
    std::same_as<T, Vec2f> || 
    std::same_as<T, Vec3f>;

// Concept for types that can be used as vertex colors
template<typename T>
concept ColorType = 
    std::same_as<T, Color> ||
    std::same_as<T, Vec4f> ||
    std::same_as<T, uint32_t>;

// Check if a type has a member named 'pos' that is a position type
template<typename T>
concept HasPosition = requires(T v) {
    { v.pos } -> PositionType;
};

// Check if a type has texture coordinates
template<typename T>
concept HasTexCoord = requires(T v) {
    { v.uv } -> TexCoordType;
} || requires(T v) {
    { v.uv_px } -> TexCoordType;
} || requires(T v) {
    { v.uv_norm } -> TexCoordType;
};

// Check if a type has a color
template<typename T>
concept HasColor = requires(T v) {
    { v.color } -> ColorType;
};

// A valid vertex type must at least have a position
template<typename T>
concept VertexType = 
    std::is_trivially_copyable_v<T> &&
    HasPosition<T>;

// A range of vertices
template<typename R>
concept VertexRange = 
    std::ranges::contiguous_range<R> &&
    VertexType<std::ranges::range_value_t<R>>;

// A range of indices
template<typename R>
concept IndexRange = 
    std::ranges::contiguous_range<R> &&
    std::integral<std::ranges::range_value_t<R>>;

} // namespace alia
```

### Drawable Concepts

```cpp
namespace alia {

// Something that can be drawn at a position
template<typename T>
concept Drawable = requires(const T& obj, Vec2f pos) {
    obj.draw(pos);
};

// Something that can be drawn scaled
template<typename T>
concept ScalableDrawable = requires(const T& obj, RectF src, RectF dst) {
    obj.draw_scaled(src, dst);
};

// Something that has dimensions
template<typename T>
concept HasSize = requires(const T& obj) {
    { obj.size() } -> std::convertible_to<Vec2i>;
    { obj.width() } -> std::convertible_to<int>;
    { obj.height() } -> std::convertible_to<int>;
};

// A texture-like object
template<typename T>
concept TextureLike = HasSize<T> && requires(const T& obj) {
    { obj.native_handle() } -> std::convertible_to<void*>;
};

} // namespace alia
```

---

## Pixel Formats

### PixelFormat Enum and Utilities

```cpp
namespace alia {

// Pixel format enumeration
enum class PixelFormat : uint32_t {
    // 32-bit formats
    ARGB8888,       // 8-bit alpha, red, green, blue (default)
    RGBA8888,       // 8-bit red, green, blue, alpha
    ABGR8888,       // 8-bit alpha, blue, green, red
    XRGB8888,       // 8-bit padding, red, green, blue (no alpha)
    
    // 24-bit formats
    RGB888,         // 8-bit red, green, blue (packed)
    BGR888,         // 8-bit blue, green, red (packed)
    
    // 16-bit formats
    RGB565,         // 5-bit red, 6-bit green, 5-bit blue
    ARGB4444,       // 4-bit alpha, red, green, blue
    ARGB1555,       // 1-bit alpha, 5-bit red, green, blue
    
    // 8-bit formats
    L8,             // 8-bit luminance (grayscale)
    A8,             // 8-bit alpha only
    
    // Floating point formats
    RGBAF32,        // 32-bit float per channel (128-bit total)
    RGBF32,         // 32-bit float per channel, no alpha (96-bit)
    RF32,           // Single 32-bit float (grayscale)
};

// Pixel format traits
template<PixelFormat F>
struct PixelFormatTraits;

template<>
struct PixelFormatTraits<PixelFormat::ARGB8888> {
    using storage_type = uint32_t;
    static constexpr int bits_per_pixel = 32;
    static constexpr int bytes_per_pixel = 4;
    static constexpr bool has_alpha = true;
};

template<>
struct PixelFormatTraits<PixelFormat::RGB565> {
    using storage_type = uint16_t;
    static constexpr int bits_per_pixel = 16;
    static constexpr int bytes_per_pixel = 2;
    static constexpr bool has_alpha = false;
};

// ... (other format traits)

// Pixel format utilities
namespace pixel {
    // Get bytes per pixel for a format
    [[nodiscard]] constexpr int bytes_per_pixel(PixelFormat format);
    
    // Check if format has alpha
    [[nodiscard]] constexpr bool has_alpha(PixelFormat format);
    
    // Convert a single pixel between formats
    [[nodiscard]] uint32_t convert(uint32_t pixel, PixelFormat from, PixelFormat to);
    
    // Convert pixel to Color
    [[nodiscard]] Color to_color(uint32_t pixel, PixelFormat format);
    
    // Convert Color to pixel
    [[nodiscard]] uint32_t from_color(Color color, PixelFormat format);
    
    // Extract color components (0.0 - 1.0)
    [[nodiscard]] float get_red(uint32_t pixel, PixelFormat format);
    [[nodiscard]] float get_green(uint32_t pixel, PixelFormat format);
    [[nodiscard]] float get_blue(uint32_t pixel, PixelFormat format);
    [[nodiscard]] float get_alpha(uint32_t pixel, PixelFormat format);
    
    // Pack color components into pixel
    [[nodiscard]] uint32_t pack(float r, float g, float b, float a, PixelFormat format);
    
    // Bulk conversion
    void convert_buffer(std::span<const std::byte> src, PixelFormat src_format,
                        std::span<std::byte> dst, PixelFormat dst_format,
                        size_t pixel_count);
}

} // namespace alia
```

---

## Bitmap and Texture

### BitmapView (Non-owning)

```cpp
namespace alia {

// BitmapView - non-owning view into pixel data (similar to std::mdspan)
// Supports arbitrary layouts via template parameter
template<typename PixelT = uint32_t, typename LayoutPolicy = layout_stride>
class BitmapView {
public:
    using pixel_type = PixelT;
    using layout_type = LayoutPolicy;
    
    // Construct from raw data
    BitmapView(PixelT* data, Vec2i size, int pitch_bytes, PixelFormat format);
    
    // Construct subview
    BitmapView(const BitmapView& parent, RectI region);
    
    // Default/copy/move
    BitmapView() = default;
    BitmapView(const BitmapView&) = default;
    BitmapView& operator=(const BitmapView&) = default;
    BitmapView(BitmapView&&) = default;
    BitmapView& operator=(BitmapView&&) = default;
    
    // Properties
    [[nodiscard]] int width() const { return size_.x; }
    [[nodiscard]] int height() const { return size_.y; }
    [[nodiscard]] Vec2i size() const { return size_; }
    [[nodiscard]] RectI rect() const { return RectI{{0, 0}, size_}; }
    [[nodiscard]] int pitch() const { return pitch_bytes_; }
    [[nodiscard]] PixelFormat format() const { return format_; }
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
    [[nodiscard]] auto subview(this Self&& self, RectI region);
    
    // Typed access (throws if format mismatch)
    template<typename OtherPixelT>
    [[nodiscard]] BitmapView<OtherPixelT, LayoutPolicy> as() const;
    
private:
    PixelT* data_ = nullptr;
    Vec2i size_{0, 0};
    int pitch_bytes_ = 0;
    PixelFormat format_ = PixelFormat::ARGB8888;
};

// Common instantiations
using BitmapView32 = BitmapView<uint32_t>;
using BitmapView16 = BitmapView<uint16_t>;
using BitmapViewF = BitmapView<float>;

} // namespace alia
```

### Bitmap (Owning)

```cpp
namespace alia {

// Bitmap - owning CPU-side pixel buffer
class Bitmap {
public:
    // Construction
    Bitmap() = default;
    Bitmap(int width, int height, PixelFormat format = PixelFormat::ARGB8888);
    Bitmap(Vec2i size, PixelFormat format = PixelFormat::ARGB8888);
    
    // Construct from raw pixel data (explicit)
    explicit Bitmap(std::span<const std::byte> raw_data, Vec2i size, 
                    PixelFormat format = PixelFormat::ARGB8888);
    
    // Construct from BitmapView (copies data)
    explicit Bitmap(const BitmapView<>& view);
    
    // Move only (no copy - explicit clone() if needed)
    Bitmap(Bitmap&&) noexcept;
    Bitmap& operator=(Bitmap&&) noexcept;
    ~Bitmap();
    
    Bitmap(const Bitmap&) = delete;
    Bitmap& operator=(const Bitmap&) = delete;
    
    // Explicit copy
    [[nodiscard]] Bitmap clone() const;
    
    // Properties (Bitmaps are always valid after construction)
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] Vec2i size() const;
    [[nodiscard]] RectI rect() const { return RectI{{0, 0}, size()}; }
    [[nodiscard]] int pitch() const;  // Bytes per row
    [[nodiscard]] PixelFormat format() const;
    [[nodiscard]] size_t data_size() const;  // Total bytes
    
    // Get view for direct pixel access
    [[nodiscard]] BitmapView<> view();
    [[nodiscard]] BitmapView<const uint32_t> view() const;
    
    // Create subview (uses deducing this)
    template<typename Self>
    [[nodiscard]] auto subview(this Self&& self, RectI region);
    
    // Typed access (throws if format mismatch)  
    template<typename PixelT>
    [[nodiscard]] BitmapView<PixelT> view_as();
    
    template<typename PixelT>
    [[nodiscard]] BitmapView<const PixelT> view_as() const;
    
    // Raw data access
    [[nodiscard]] std::byte* data();
    [[nodiscard]] const std::byte* data() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Free function to load bitmap from file (handles image format registry)
[[nodiscard]] Bitmap load_bitmap(std::string_view path);
[[nodiscard]] Bitmap load_bitmap(std::span<const std::byte> file_data);

} // namespace alia
```

### Texture (GPU)

```cpp
namespace alia {

// Texture creation flags
enum class TextureFlags : uint32_t {
    None = 0,
    
    // Filter flags
    MinLinear = 1 << 0,          // Linear minification filter
    MagLinear = 1 << 1,          // Linear magnification filter
    Mipmap = 1 << 2,             // Generate mipmaps
    
    // Feature flags
    NoPremultipliedAlpha = 1 << 4,
    RenderTarget = 1 << 5,       // Can be used as render target
};

constexpr TextureFlags operator|(TextureFlags a, TextureFlags b) {
    return static_cast<TextureFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

// Lock mode as template parameter for const-correctness
enum class LockMode {
    ReadOnly,
    WriteOnly,
    ReadWrite
};

// RAII texture lock - provides BitmapView access to texture data
template<LockMode Mode>
class TextureLock {
public:
    TextureLock(TextureLock&&) noexcept;
    TextureLock& operator=(TextureLock&&) noexcept;
    ~TextureLock();
    
    // Non-copyable, non-movable during lock
    TextureLock(const TextureLock&) = delete;
    TextureLock& operator=(const TextureLock&) = delete;
    
    // Get view to locked data
    [[nodiscard]] auto view() requires (Mode != LockMode::WriteOnly) {
        return BitmapView<const uint32_t>(data_, size_, pitch_, format_);
    }
    
    [[nodiscard]] auto view() requires (Mode == LockMode::WriteOnly || Mode == LockMode::ReadWrite) {
        return BitmapView<uint32_t>(data_, size_, pitch_, format_);
    }
    
    // Properties
    [[nodiscard]] int width() const { return size_.x; }
    [[nodiscard]] int height() const { return size_.y; }
    [[nodiscard]] Vec2i size() const { return size_; }
    [[nodiscard]] int pitch() const { return pitch_; }
    
private:
    friend class Texture;
    TextureLock(/* internal */);
    
    uint32_t* data_;
    Vec2i size_;
    int pitch_;
    PixelFormat format_;
    void* texture_handle_;
};

// Texture - GPU-side image
class Texture {
public:
    // Construction
    Texture() = default;
    Texture(int width, int height, TextureFlags flags = TextureFlags::None);
    Texture(Vec2i size, TextureFlags flags = TextureFlags::None);
    
    // Create from Bitmap (uploads to GPU)
    explicit Texture(const Bitmap& bitmap, TextureFlags flags = TextureFlags::None);
    explicit Texture(const BitmapView<>& view, TextureFlags flags = TextureFlags::None);
    
    // Move only
    Texture(Texture&&) noexcept;
    Texture& operator=(Texture&&) noexcept;
    ~Texture();
    
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] Vec2i size() const;
    [[nodiscard]] RectI rect() const { return RectI{{0, 0}, size()}; }
    [[nodiscard]] TextureFlags flags() const;
    [[nodiscard]] PixelFormat format() const;
    
    // Lock for CPU access (downloads from GPU)
    template<LockMode Mode = LockMode::ReadWrite>
    [[nodiscard]] TextureLock<Mode> lock();
    
    template<LockMode Mode = LockMode::ReadWrite>
    [[nodiscard]] TextureLock<Mode> lock(RectI region);
    
    // Update from bitmap (partial or full)
    void update(const Bitmap& bitmap);
    void update(const Bitmap& bitmap, Vec2i dest_pos);
    void update(const BitmapView<>& view, Vec2i dest_pos);
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
    // Global settings for new textures
    static void set_new_texture_flags(TextureFlags flags);
    static TextureFlags get_new_texture_flags();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Free function to load texture directly from file (loads bitmap, uploads, discards)
[[nodiscard]] Texture load_texture(std::string_view path, TextureFlags flags = TextureFlags::None);
[[nodiscard]] Texture load_texture(std::span<const std::byte> file_data, TextureFlags flags = TextureFlags::None);

// Drawing free functions for Texture (fast, GPU-accelerated)
void draw(const Texture& texture, Vec2f pos, int flags = 0);
void draw_tinted(const Texture& texture, Vec2f pos, Color tint, int flags = 0);
void draw_scaled(const Texture& texture, RectF src, RectF dst, int flags = 0);
void draw_rotated(const Texture& texture, Vec2f center, Vec2f dst, float angle, int flags = 0);
void draw_region(const Texture& texture, RectF src, Vec2f dst, int flags = 0);

// Drawing free functions for Bitmap (slow, software rendering - name reflects this)
void draw_software(const Bitmap& bitmap, Vec2f pos);
void draw_software_tinted(const Bitmap& bitmap, Vec2f pos, Color tint);
void draw_software_scaled(const Bitmap& bitmap, RectF src, RectF dst);

} // namespace alia
```

---

## Vertex System

### Vertex Semantics

```cpp
namespace alia {

// Semantic meaning of vertex attributes
enum class VertexSemantic : uint32_t {
    Position,       // Vertex position (2D, 3D, or 4D)
    TexCoord,       // Texture coordinates (pixel or normalized)
    TexCoordNorm,   // Normalized texture coordinates [0,1]
    Normal,         // Surface normal
    Tangent,        // Surface tangent
    Binormal,       // Surface binormal
    Color,          // Vertex color
    BoneIndices,    // Skeletal animation bone indices
    BoneWeights,    // Skeletal animation bone weights
    UserAttr0,      // User-defined attribute 0
    UserAttr1,      // User-defined attribute 1
    UserAttr2,      // User-defined attribute 2
    UserAttr3,      // User-defined attribute 3
};

// Data types for vertex attributes
enum class VertexAttrType : uint32_t {
    Float1,
    Float2,
    Float3,
    Float4,
    Int1,
    Int2,
    Int3,
    Int4,
    UByte4,         // 4 unsigned bytes
    UByte4Norm,     // 4 unsigned bytes, normalized to [0,1]
    Short2,
    Short2Norm,
    Short4,
    Short4Norm,
};

// Single vertex attribute descriptor
struct VertexAttribute {
    VertexSemantic semantic;
    VertexAttrType type;
    uint32_t offset;
};

// Note: Use offsetof() directly to specify attribute offsets.
// Pointer-to-member cannot be converted to offset without undefined behavior.
// Vertex types should be standard layout (static_assert recommended).

// Helper to define vertex attributes
template<VertexAttribute... Attrs>
struct VertexAttributeList {
    static constexpr std::array<VertexAttribute, sizeof...(Attrs)> attributes = {Attrs...};
};

// Trait to get vertex attributes for a type
template<typename T>
struct VertexTraits {
    // Default: try to auto-detect from common member names
    static constexpr auto attributes = detail::auto_detect_vertex_attrs<T>();
};

// Helper function to create attribute list
template<VertexAttribute... Attrs>
consteval auto vertex_attrs() {
    return std::array<VertexAttribute, sizeof...(Attrs)>{Attrs...};
}

// Compile-time vertex declaration
template<typename VertexT>
class VertexDeclaration {
public:
    static constexpr auto& attributes() {
        return VertexTraits<VertexT>::attributes;
    }
    
    static constexpr size_t stride() {
        return sizeof(VertexT);
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
struct Vertex2D {
    Vec2f pos;
    Color color = Color::White;
};

// 2D vertex with texture coordinates
struct Vertex2DTex {
    Vec2f pos;
    Vec2f uv;       // Texture coordinates in pixels
    Color color = Color::White;
};

// Full 3D vertex
struct Vertex3D {
    Vec3f pos;
    Vec2f uv;
    Vec3f normal;
    Color color = Color::White;
};

// Skinned vertex for skeletal animation
struct VertexSkinned {
    Vec3f pos;
    Vec2f uv;
    Vec3f normal;
    Vec4i bone_indices;
    Vec4f bone_weights;
};

// Specializations for built-in types
template<> struct VertexTraits<Vertex2D> {
    static constexpr auto attributes = vertex_attrs<
        VertexAttribute{VertexSemantic::Position, VertexAttrType::Float2, 0},
        VertexAttribute{VertexSemantic::Color, VertexAttrType::Float4, offsetof(Vertex2D, color)}
    >();
};

template<> struct VertexTraits<Vertex2DTex> {
    static constexpr auto attributes = vertex_attrs<
        VertexAttribute{VertexSemantic::Position, VertexAttrType::Float2, 0},
        VertexAttribute{VertexSemantic::TexCoord, VertexAttrType::Float2, offsetof(Vertex2DTex, uv)},
        VertexAttribute{VertexSemantic::Color, VertexAttrType::Float4, offsetof(Vertex2DTex, color)}
    >();
};

template<> struct VertexTraits<Vertex3D> {
    static constexpr auto attributes = vertex_attrs<
        VertexAttribute{VertexSemantic::Position, VertexAttrType::Float3, 0},
        VertexAttribute{VertexSemantic::TexCoord, VertexAttrType::Float2, offsetof(Vertex3D, uv)},
        VertexAttribute{VertexSemantic::Normal, VertexAttrType::Float3, offsetof(Vertex3D, normal)},
        VertexAttribute{VertexSemantic::Color, VertexAttrType::Float4, offsetof(Vertex3D, color)}
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
enum class PrimitiveType {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    TriangleFan,
};

// Draw primitives from vertex array
template<VertexRange R>
void draw_prim(const R& vertices, 
               PrimitiveType type = PrimitiveType::TriangleList,
               const Bitmap* texture = nullptr);

// Draw primitives with index buffer
template<VertexRange VR, IndexRange IR>
void draw_indexed_prim(const VR& vertices, 
                       const IR& indices,
                       PrimitiveType type = PrimitiveType::TriangleList,
                       const Bitmap* texture = nullptr);

// Convenience wrappers
template<VertexRange R>
void draw_triangles(const R& vertices, const Bitmap* texture = nullptr) {
    draw_prim(vertices, PrimitiveType::TriangleList, texture);
}

template<VertexRange R>
void draw_triangle_strip(const R& vertices, const Bitmap* texture = nullptr) {
    draw_prim(vertices, PrimitiveType::TriangleStrip, texture);
}

template<VertexRange R>
void draw_lines(const R& vertices) {
    draw_prim(vertices, PrimitiveType::LineList, nullptr);
}

// Simple shape drawing (99% use case)
void draw_line(Vec2f p1, Vec2f p2, Color color, float thickness = 1.0f);
void draw_triangle(Vec2f p1, Vec2f p2, Vec2f p3, Color color, float thickness = 1.0f);
void draw_rectangle(RectF rect, Color color, float thickness = 1.0f);
void draw_rounded_rectangle(RectF rect, float rx, float ry, Color color, float thickness = 1.0f);
void draw_circle(Vec2f center, float radius, Color color, float thickness = 1.0f);
void draw_ellipse(Vec2f center, float rx, float ry, Color color, float thickness = 1.0f);
void draw_arc(Vec2f center, float radius, float start_angle, float delta_angle, Color color, float thickness = 1.0f);
void draw_spline(std::span<const Vec2f> points, Color color, float thickness = 1.0f);
void draw_polygon(std::span<const Vec2f> points, Color color, float thickness = 1.0f);

// Filled shapes
void draw_filled_triangle(Vec2f p1, Vec2f p2, Vec2f p3, Color color);
void draw_filled_rectangle(RectF rect, Color color);
void draw_filled_rounded_rectangle(RectF rect, float rx, float ry, Color color);
void draw_filled_circle(Vec2f center, float radius, Color color);
void draw_filled_ellipse(Vec2f center, float rx, float ry, Color color);
void draw_filled_pie_slice(Vec2f center, float radius, float start_angle, float delta_angle, Color color);
void draw_filled_polygon(std::span<const Vec2f> points, Color color);

// Gradient shapes
void draw_filled_rectangle_gradient(RectF rect, Color tl, Color tr, Color br, Color bl);

} // namespace alia
```

---

## Transforms

### Transform Class

```cpp
namespace alia {

// 4x4 transformation matrix
class Transform {
public:
    // Identity transform
    constexpr Transform() : m_{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    } {}
    
    // From raw matrix data (column-major)
    explicit Transform(std::span<const float, 16> data);
    
    // Access matrix elements (C++23 multi-arg operator[])
    float& operator[](int row, int col) { return m_[col * 4 + row]; }
    float operator[](int row, int col) const { return m_[col * 4 + row]; }
    
    // Raw data access
    [[nodiscard]] std::span<const float, 16> data() const { return m_; }
    [[nodiscard]] std::span<float, 16> data() { return m_; }
    
    // Factory methods
    [[nodiscard]] static Transform identity();
    [[nodiscard]] static Transform translation(Vec3f offset);
    [[nodiscard]] static Transform translation(Vec2f offset);
    [[nodiscard]] static Transform scaling(Vec3f factors);
    [[nodiscard]] static Transform scaling(Vec2f factors);
    [[nodiscard]] static Transform scaling(float uniform);
    [[nodiscard]] static Transform rotation_x(float radians);
    [[nodiscard]] static Transform rotation_y(float radians);
    [[nodiscard]] static Transform rotation_z(float radians);
    [[nodiscard]] static Transform rotation(Vec3f axis, float radians);
    
    // 2D rotation (around Z axis)
    [[nodiscard]] static Transform rotation_2d(float radians);
    [[nodiscard]] static Transform rotation_2d(Vec2f center, float radians);
    
    // Projection matrices
    [[nodiscard]] static Transform ortho(float left, float right, float bottom, float top, float near, float far);
    [[nodiscard]] static Transform perspective(float fov_y, float aspect, float near, float far);
    [[nodiscard]] static Transform perspective_fov(float fov_degrees, float near, float far, float aspect);
    
    // View matrix (camera)
    [[nodiscard]] static Transform look_at(Vec3f eye, Vec3f target, Vec3f up);
    [[nodiscard]] static Transform camera(Vec3f pos, Vec3f look_at, Vec3f up);
    
    // Chainable transformations (modify and return *this)
    Transform& translate(Vec3f offset);
    Transform& translate(Vec2f offset);
    Transform& scale(Vec3f factors);
    Transform& scale(Vec2f factors);
    Transform& scale(float uniform);
    Transform& rotate_x(float radians);
    Transform& rotate_y(float radians);
    Transform& rotate_z(float radians);
    Transform& rotate(Vec3f axis, float radians);
    Transform& rotate_2d(float radians);
    Transform& rotate_2d(Vec2f center, float radians);
    
    // Composition
    [[nodiscard]] Transform operator*(const Transform& rhs) const;
    Transform& operator*=(const Transform& rhs);
    
    // Transform points
    [[nodiscard]] Vec2f transform_point(Vec2f p) const;
    [[nodiscard]] Vec3f transform_point(Vec3f p) const;
    [[nodiscard]] Vec4f transform_point(Vec4f p) const;
    
    // Inverse
    [[nodiscard]] Transform inverted() const;
    bool invert();  // In-place, returns false if singular
    
    // Apply to current rendering state
    void use() const;                    // Set as current transform
    void use_projection() const;         // Set as projection matrix
    
    // Get current transforms
    [[nodiscard]] static Transform current();
    [[nodiscard]] static Transform current_projection();
    
private:
    std::array<float, 16> m_;
};

// RAII scoped transform
class ScopedTransform {
public:
    explicit ScopedTransform(const Transform& t);
    ~ScopedTransform();
    
    ScopedTransform(const ScopedTransform&) = delete;
    ScopedTransform& operator=(const ScopedTransform&) = delete;
    
private:
    Transform saved_;
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

// Shader stage
enum class ShaderStage {
    Vertex,
    Pixel,      // Fragment
    Geometry,
    Compute,
};

// Shader source language
enum class ShaderLanguage {
    GLSL,       // OpenGL Shading Language
    HLSL,       // High-Level Shading Language
    SPIRV,      // SPIR-V binary
    Auto,       // Detect from backend
};

// Compiled shader stage
class ShaderModule {
public:
    ShaderModule() = default;
    ShaderModule(ShaderModule&&) noexcept;
    ShaderModule& operator=(ShaderModule&&) noexcept;
    ~ShaderModule();
    
    // Factory
    [[nodiscard]] static ShaderModule compile(ShaderStage stage, 
                                               std::string_view source,
                                               ShaderLanguage lang = ShaderLanguage::Auto);
    
    [[nodiscard]] static ShaderModule load_spirv(ShaderStage stage,
                                                  std::span<const uint32_t> spirv);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] ShaderStage stage() const;
    
    [[nodiscard]] void* native_handle() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Shader uniform types
using UniformValue = std::variant<
    float, Vec2f, Vec3f, Vec4f,
    int, Vec2i, Vec3i, Vec4i,
    Transform,  // mat4
    std::array<float, 9>  // mat3
>;

// Complete shader program
class Shader {
public:
    Shader() = default;
    Shader(Shader&&) noexcept;
    Shader& operator=(Shader&&) noexcept;
    ~Shader();
    
    // Build from modules
    class Builder {
    public:
        Builder& vertex(ShaderModule module);
        Builder& pixel(ShaderModule module);
        Builder& geometry(ShaderModule module);
        
        // Convenience: compile from source
        Builder& vertex_source(std::string_view source, ShaderLanguage lang = ShaderLanguage::Auto);
        Builder& pixel_source(std::string_view source, ShaderLanguage lang = ShaderLanguage::Auto);
        
        // Use default shaders
        Builder& default_vertex();
        Builder& default_pixel();
        
        [[nodiscard]] Shader build();
        
    private:
        friend class Shader;
        struct BuilderImpl;
        std::unique_ptr<BuilderImpl> impl_;
    };
    
    [[nodiscard]] static Builder builder();
    
    // Quick creation for common cases
    [[nodiscard]] static Shader from_source(std::string_view vertex_src,
                                             std::string_view pixel_src,
                                             ShaderLanguage lang = ShaderLanguage::Auto);
    
    [[nodiscard]] bool valid() const;
    
    // Activate shader
    void use() const;
    static void use_default();  // Reset to fixed-function / default
    
    // Set uniforms
    void set_uniform(std::string_view name, const UniformValue& value);
    void set_uniform(int location, const UniformValue& value);
    
    // Set texture samplers
    void set_sampler(std::string_view name, const Texture& texture, int unit);
    void set_sampler(int location, const Texture& texture, int unit);
    
    // Query uniform locations (for caching)
    [[nodiscard]] int get_uniform_location(std::string_view name) const;
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace alia
```

---

## Render Targets

### Framebuffer and Surface

```cpp
namespace alia {

// Surface format
enum class SurfaceFormat {
    RGBA8,
    RGBA16F,
    RGBA32F,
    R8,
    R16F,
    R32F,
    Depth16,
    Depth24,
    Depth32F,
    Depth24Stencil8,
    Depth32FStencil8,
};

// Render target / framebuffer
class Surface {
public:
    Surface() = default;
    Surface(int width, int height, SurfaceFormat format = SurfaceFormat::RGBA8);
    Surface(Vec2i size, SurfaceFormat format = SurfaceFormat::RGBA8);
    
    Surface(Surface&&) noexcept;
    Surface& operator=(Surface&&) noexcept;
    ~Surface();
    
    [[nodiscard]] bool valid() const;
    
    // Properties
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] Vec2i size() const;
    [[nodiscard]] SurfaceFormat format() const;
    
    // Get as bitmap (for reading pixels or using as texture)
    [[nodiscard]] Bitmap& as_bitmap();
    [[nodiscard]] const Bitmap& as_bitmap() const;
    
    // Set as render target
    void set_as_target();
    
    // Clear operations
    void clear(Color color = Color::Black);
    void clear_depth(float depth = 1.0f);
    void clear_stencil(int value = 0);
    
    // Depth/stencil configuration
    void set_depth_test(bool enable);
    void set_depth_write(bool enable);
    void set_stencil_test(bool enable);
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
    // Current target
    [[nodiscard]] static Surface* current();
    static void reset_target();  // Reset to window backbuffer
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// RAII scoped render target
class ScopedTarget {
public:
    explicit ScopedTarget(Surface& surface);
    ~ScopedTarget();
    
    ScopedTarget(const ScopedTarget&) = delete;
    ScopedTarget& operator=(const ScopedTarget&) = delete;
    
private:
    Surface* saved_;
};

} // namespace alia
```

---

## GPU Buffers

### Vertex and Index Buffers

```cpp
namespace alia {

// Buffer usage hints
enum class BufferUsage {
    Static,     // Set once, used many times
    Dynamic,    // Modified occasionally
    Stream,     // Modified every frame
};

// Buffer lock for direct access
template<typename T>
class BufferLock {
public:
    BufferLock(BufferLock&&) noexcept;
    BufferLock& operator=(BufferLock&&) noexcept;
    ~BufferLock();
    
    BufferLock(const BufferLock&) = delete;
    BufferLock& operator=(const BufferLock&) = delete;
    
    [[nodiscard]] std::span<T> data();
    [[nodiscard]] std::span<const T> data() const;
    
    T& operator[](size_t index) { return data()[index]; }
    const T& operator[](size_t index) const { return data()[index]; }
    
private:
    friend class VertexBuffer;
    friend class IndexBuffer;
    BufferLock(/* internal */);
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Vertex buffer
class VertexBuffer {
public:
    VertexBuffer() = default;
    
    // Create with initial data
    template<VertexRange R>
    explicit VertexBuffer(const R& vertices, BufferUsage usage = BufferUsage::Static);
    
    // Create empty with size
    VertexBuffer(size_t vertex_count, size_t vertex_size, BufferUsage usage = BufferUsage::Dynamic);
    
    VertexBuffer(VertexBuffer&&) noexcept;
    VertexBuffer& operator=(VertexBuffer&&) noexcept;
    ~VertexBuffer();
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] size_t size() const;       // Number of vertices
    [[nodiscard]] size_t stride() const;     // Bytes per vertex
    [[nodiscard]] size_t bytes_per_vertex() const; // same as stride()
    [[nodiscard]] BufferUsage usage() const;
    
    // Update data
    template<VertexRange R>
    void update(const R& vertices, size_t offset = 0);
    
    // Lock for direct access
    template<typename VertexT>
    [[nodiscard]] BufferLock<VertexT> lock(size_t offset = 0, size_t count = 0);
    
    template<typename VertexT>
    [[nodiscard]] BufferLock<const VertexT> lock(size_t offset = 0, size_t count = 0) const;
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Index buffer
class IndexBuffer {
public:
    IndexBuffer() = default;
    
    // Create with initial data
    template<IndexRange R>
    explicit IndexBuffer(const R& indices, BufferUsage usage = BufferUsage::Static);
    
    // Create empty with size
    IndexBuffer(size_t index_count, BufferUsage usage = BufferUsage::Dynamic);
    
    IndexBuffer(IndexBuffer&&) noexcept;
    IndexBuffer& operator=(IndexBuffer&&) noexcept;
    ~IndexBuffer();
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] size_t size() const;  // Number of indices
    [[nodiscard]] BufferUsage usage() const;
    
    // Update data
    template<IndexRange R>
    void update(const R& indices, size_t offset = 0);
    
    // Lock for direct access
    [[nodiscard]] BufferLock<int> lock(size_t offset = 0, size_t count = 0);
    [[nodiscard]] BufferLock<const int> lock(size_t offset = 0, size_t count = 0) const;
    
    // Backend handle
    [[nodiscard]] void* native_handle() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Draw from GPU buffers
void draw_buffer(const VertexBuffer& vb, 
                 PrimitiveType type = PrimitiveType::TriangleList,
                 const Bitmap* texture = nullptr);

void draw_indexed_buffer(const VertexBuffer& vb,
                         const IndexBuffer& ib,
                         PrimitiveType type = PrimitiveType::TriangleList,
                         const Bitmap* texture = nullptr);

} // namespace alia
```

---

## Backend Interface

### Graphics Backend Interface (detail namespace)

```cpp
namespace alia::detail {

// Resource types for backend tracking
enum class GfxResourceType : uint32_t {
    Bitmap,
    VertexBuffer,
    IndexBuffer,
    Shader,
    Surface,
    VertexDeclaration,
};

// Resource data for hot-swap
struct GfxResourceData {
    GfxResourceType type;
    Vec2i size;                         // For bitmaps/surfaces
    PixelFormat format;                 // For bitmaps
    std::vector<std::byte> cpu_data;    // CPU-side copy
    void* extra_data;                   // Type-specific extra info
};

// Abstract graphics backend interface
// Implementations live in separate .cpp files
class IGfxBackend {
public:
    virtual ~IGfxBackend() = default;
    
    // Lifecycle
    virtual bool initialize(void* window_handle) = 0;
    virtual void shutdown() = 0;
    virtual bool is_valid() const = 0;
    
    // Capabilities
    virtual uint32_t get_capabilities() const = 0;
    virtual Limits get_limits() const = 0;
    virtual const char* get_name() const = 0;
    
    // Bitmap operations
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
    
    // Shader operations
    virtual void* compile_shader(int stage, const char* source, int lang) = 0;
    virtual void destroy_shader_module(void* handle) = 0;
    virtual void* create_shader_program(void* vertex, void* pixel, void* geometry) = 0;
    virtual void destroy_shader_program(void* handle) = 0;
    virtual void use_shader(void* handle) = 0;
    virtual int get_uniform_location(void* handle, const char* name) = 0;
    virtual void set_uniform(void* handle, int location, int type, const void* data) = 0;
    
    // Surface operations
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
struct GfxBackendInfo {
    const char* name;
    uint32_t id;
    bool (*is_available)();
    IGfxBackend* (*create)();
    void (*destroy)(IGfxBackend*);
};

void register_gfx_backend(const GfxBackendInfo& info);
std::span<const GfxBackendInfo> get_gfx_backends();
IGfxBackend* get_current_gfx_backend();
bool switch_gfx_backend(uint32_t backend_id);

} // namespace alia::detail
```

---

## Capabilities

### Capability Queries

```cpp
namespace alia::gfx {

// Capability flags (bitfield)
enum class Capability : uint32_t {
    // Texture capabilities
    TextureNPOT          = 1 << 0,   // Non-power-of-two textures
    TextureFloat         = 1 << 1,   // Floating-point textures
    TextureDepth         = 1 << 2,   // Depth textures
    TextureArray         = 1 << 3,   // Texture arrays
    Texture3D            = 1 << 4,   // 3D textures
    TextureCube          = 1 << 5,   // Cubemap textures
    TextureCompressed    = 1 << 6,   // Compressed texture formats (DXT, etc.)
    
    // Shader capabilities
    ShaderVertex         = 1 << 8,   // Programmable vertex shaders
    ShaderPixel          = 1 << 9,   // Programmable pixel/fragment shaders
    ShaderGeometry       = 1 << 10,  // Geometry shaders
    ShaderCompute        = 1 << 11,  // Compute shaders
    ShaderTessellation   = 1 << 12,  // Tessellation shaders
    
    // Rendering features
    Framebuffer          = 1 << 16,  // Render-to-texture
    MultipleRenderTargets= 1 << 17,  // MRT support
    Instancing           = 1 << 18,  // Hardware instancing
    IndirectDraw         = 1 << 19,  // Indirect drawing
    OcclusionQuery       = 1 << 20,  // Occlusion queries
    
    // Blending
    BlendSeparate        = 1 << 24,  // Separate RGB/Alpha blending
    BlendDualSource      = 1 << 25,  // Dual-source blending
    BlendMinMax          = 1 << 26,  // Min/max blend equations
    
    // Other
    AnisotropicFiltering = 1 << 28,  // Anisotropic texture filtering
    SRGBTextures         = 1 << 29,  // sRGB texture support
    DepthClamp           = 1 << 30,  // Depth clamping
};

constexpr Capability operator|(Capability a, Capability b) {
    return static_cast<Capability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr Capability operator&(Capability a, Capability b) {
    return static_cast<Capability>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

// Query individual capability
[[nodiscard]] bool has_capability(Capability cap);

// Get all capabilities as bitmask
[[nodiscard]] uint32_t get_capabilities();

// Hardware limits
struct Limits {
    int max_texture_size;           // Maximum texture dimension
    int max_texture_units;          // Maximum bound textures
    int max_render_targets;         // Maximum simultaneous render targets
    int max_vertex_attributes;      // Maximum vertex attributes
    int max_uniform_block_size;     // Maximum uniform buffer size
    int max_anisotropy;             // Maximum anisotropic filtering level
    int max_viewport_dims[2];       // Maximum viewport dimensions
    int max_clip_planes;            // Maximum user clip planes
    float max_point_size;           // Maximum point sprite size
    float line_width_range[2];      // Min/max line width
    float line_width_granularity;   // Line width step
};

[[nodiscard]] const Limits& get_limits();

// Backend info
struct BackendInfo {
    const char* name;               // e.g., "Direct3D 9", "OpenGL 4.5"
    const char* vendor;             // GPU vendor
    const char* renderer;           // GPU model
    int version_major;
    int version_minor;
    int shader_model;               // D3D shader model or GLSL version
};

[[nodiscard]] const BackendInfo& get_backend_info();

// Feature detection helpers
[[nodiscard]] inline bool supports_shaders() {
    return has_capability(Capability::ShaderVertex) && has_capability(Capability::ShaderPixel);
}

[[nodiscard]] inline bool supports_render_to_texture() {
    return has_capability(Capability::Framebuffer);
}

[[nodiscard]] inline bool supports_npot_textures() {
    return has_capability(Capability::TextureNPOT);
}

} // namespace alia::gfx
```

---

## Blend Modes

### Blend State

```cpp
namespace alia::gfx {

// Blend factors
enum class BlendFactor {
    Zero,
    One,
    SrcColor,
    InvSrcColor,
    SrcAlpha,
    InvSrcAlpha,
    DstColor,
    InvDstColor,
    DstAlpha,
    InvDstAlpha,
    SrcAlphaSat,
    ConstColor,
    InvConstColor,
    ConstAlpha,
    InvConstAlpha,
};

// Blend operations
enum class BlendOp {
    Add,
    Subtract,
    RevSubtract,
    Min,
    Max,
};

// Complete blend state
struct BlendState {
    bool enabled = true;
    
    BlendFactor src_color = BlendFactor::SrcAlpha;
    BlendFactor dst_color = BlendFactor::InvSrcAlpha;
    BlendOp color_op = BlendOp::Add;
    
    BlendFactor src_alpha = BlendFactor::One;
    BlendFactor dst_alpha = BlendFactor::InvSrcAlpha;
    BlendOp alpha_op = BlendOp::Add;
    
    // Common presets
    static const BlendState Opaque;
    static const BlendState Alpha;
    static const BlendState Additive;
    static const BlendState Multiply;
    static const BlendState PremultipliedAlpha;
};

inline const BlendState BlendState::Opaque = {.enabled = false};
inline const BlendState BlendState::Alpha = {};  // Default is alpha blend
inline const BlendState BlendState::Additive = {
    .src_color = BlendFactor::SrcAlpha,
    .dst_color = BlendFactor::One,
    .src_alpha = BlendFactor::One,
    .dst_alpha = BlendFactor::One,
};
inline const BlendState BlendState::Multiply = {
    .src_color = BlendFactor::DstColor,
    .dst_color = BlendFactor::Zero,
};
inline const BlendState BlendState::PremultipliedAlpha = {
    .src_color = BlendFactor::One,
    .dst_color = BlendFactor::InvSrcAlpha,
    .src_alpha = BlendFactor::One,
    .dst_alpha = BlendFactor::InvSrcAlpha,
};

// Set current blend state
void set_blend_state(const BlendState& state);

// RAII scoped blend state
class ScopedBlendState {
public:
    explicit ScopedBlendState(const BlendState& state);
    ~ScopedBlendState();
    
private:
    BlendState saved_;
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
    void clear(Color color);
    void clear_depth(float depth = 1.0f);
    void clear_stencil(int value = 0);
    
    // Reset transforms
    void reset_transform();
    void reset_projection();
    
    // Properties of current target
    [[nodiscard]] int width();
    [[nodiscard]] int height();
    [[nodiscard]] Vec2i size();
    [[nodiscard]] RectI rect();
    [[nodiscard]] float aspect_ratio();
    
    // Depth/stencil state
    void set_depth_test(bool enable);
    void set_depth_write(bool enable);
    void set_stencil_test(bool enable);
}

// Current window/display utilities (see os-interfaces.md for Window class)
namespace display {
    [[nodiscard]] int width();
    [[nodiscard]] int height();
    [[nodiscard]] Vec2i size();
    [[nodiscard]] RectI rect();
    [[nodiscard]] float aspect_ratio();
    
    void flip();  // Present backbuffer
    void set_vsync(bool enable);
}

} // namespace alia
```
