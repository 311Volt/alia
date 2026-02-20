#ifndef ALIA_CORE_COLOR_HPP
#define ALIA_CORE_COLOR_HPP

#include <cstdint>

namespace alia {

struct color {
    float r, g, b, a;

    constexpr color() : r(0), g(0), b(0), a(1) {}
    constexpr color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    static constexpr color from_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }

    constexpr bool operator==(const color& other) const = default;

    // Preserved colors from design docs example
    static const color red;
    static const color green;
    static const color blue;
    static const color black;
    static const color white;
    static const color cornflower_blue;
};

inline constexpr color color::red = {1, 0, 0, 1};
inline constexpr color color::green = {0, 1, 0, 1};
inline constexpr color color::blue = {0, 0, 1, 1};
inline constexpr color color::black = {0, 0, 0, 1};
inline constexpr color color::white = {1, 1, 1, 1};
inline constexpr color color::cornflower_blue = {0.392f, 0.584f, 0.929f, 1.0f};

} // namespace alia

#endif // ALIA_CORE_COLOR_HPP
