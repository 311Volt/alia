#ifndef ALIA_CORE_COLOR_HPP
#define ALIA_CORE_COLOR_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <tuple>

// std::sqrt is constexpr when __cpp_lib_constexpr_cmath is available (C++26).
#if defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202306L
#  define ALIA_CXX26_MATH_CONSTEXPR constexpr
#else
#  define ALIA_CXX26_MATH_CONSTEXPR
#endif

namespace alia {

    struct color {
        float r, g, b, a;

        // Constructs a fully-opaque black.
        constexpr color() : r(0), g(0), b(0), a(1) {
        }
        // Constructs a color from floating-point channel values in [0, 1].
        constexpr color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {
        }

        // Creates a color from 8-bit RGBA channel values.
        static constexpr color from_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            constexpr float f = 1.0f / 255.0f;
            return {r * f, g * f, b * f, a * f};
        }

        // Creates an opaque color from a 32-bit number of the form 0xXXRRGGBB.
        static constexpr color from_rgb_u32(uint32_t x) {
            return from_rgba8(uint8_t((x >> 16) & 0xFF), uint8_t((x >> 8) & 0xFF), uint8_t((x >> 0) & 0xFF));
        }

        // Creates a color from a 32-bit number of the form 0xAARRGGBB.
        static constexpr color from_rgba_u32(uint32_t x) {
            return from_rgba8(uint8_t((x >> 16) & 0xFF), uint8_t((x >> 8) & 0xFF), uint8_t((x >> 0) & 0xFF), uint8_t((x >> 24) & 0xFF));
        }

        // Creates a color from pre-multiplied floating-point RGBA values; un-multiplies RGB by alpha.
        static constexpr color from_premul_rgba_f(float r, float g, float b, float a) {
            return {r * a, g * a, b * a, a};
        }

        // Creates a color from pre-multiplied 8-bit RGBA values; un-multiplies RGB by alpha.
        static constexpr color from_premul_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
            constexpr float f = 1.0f / 255.0f;
            return from_premul_rgba_f(r * f, g * f, b * f, a * f);
        }

        // Creates an opaque gray from a luminance value in [0, 1]; clamps out-of-range input.
        static constexpr color gray(float lum) {
            lum = std::clamp(lum, 0.0f, 1.0f);
            return {lum, lum, lum, 1.0f};
        }

        // Creates an opaque gray from an 8-bit luminance value.
        static constexpr color gray8(uint8_t lum) {
            return from_rgba8(lum, lum, lum);
        }

        // Returns the midpoint of two colors blended in approximate linear RGB (gamma 2.0).
        // RGB channels are squared before mixing and sqrt'd after; alpha is blended linearly.
        static ALIA_CXX26_MATH_CONSTEXPR color average(const color& a, const color& b) {
            return {
                std::sqrt((a.r * a.r + b.r * b.r) * 0.5f),
                std::sqrt((a.g * a.g + b.g * b.g) * 0.5f),
                std::sqrt((a.b * a.b + b.b * b.b) * 0.5f),
                (a.a + b.a) * 0.5f
            };
        }

        // Interpolates between two colors in approximate linear RGB (gamma 2.0).
        // RGB channels are squared before mixing and sqrt'd after; alpha is blended linearly.
        static ALIA_CXX26_MATH_CONSTEXPR color lerp(const color& a, const color& b, float t) {
            float s = 1.0f - t;
            return {
                std::sqrt(a.r * a.r * s + b.r * b.r * t),
                std::sqrt(a.g * a.g * s + b.g * b.g * t),
                std::sqrt(a.b * a.b * s + b.b * b.b * t),
                a.a * s + b.a * t
            };
        }

        // Returns the midpoint of two colors blended directly in sRGB space (no gamma correction).
        static constexpr color average_srgb(const color& a, const color& b) {
            return {
                (a.r + b.r) * 0.5f,
                (a.g + b.g) * 0.5f,
                (a.b + b.b) * 0.5f,
                (a.a + b.a) * 0.5f
            };
        }

        // Interpolates between two colors directly in sRGB space (no gamma correction).
        static constexpr color lerp_srgb(const color& a, const color& b, float t) {
            float s = 1.0f - t;
            return {a.r * s + b.r * t, a.g * s + b.g * t, a.b * s + b.b * t, a.a * s + b.a * t};
        }

        // Creates the nth color from the 16-color CGA palette. Upper 4 bits are ignored.
        static constexpr color cga(uint8_t c) {
            if (c == 6) {
                return from_rgb_u32(0xAA5500u);
            }
            uint32_t b = -uint32_t((c & 1) != 0);
            uint32_t g = -uint32_t((c & 2) != 0);
            uint32_t r = -uint32_t((c & 4) != 0);
            uint32_t i = -uint32_t((c & 8) != 0);
            return from_rgb_u32((r & 0xAA0000u) | (g & 0x00AA00u) | (b & 0x0000AAu) | (i & 0x555555u));
        }

        // Returns all channel values as a tuple of bytes.
        constexpr std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> rgba_u8() const {
            return {
                static_cast<uint8_t>(r * 255.0f),
                static_cast<uint8_t>(g * 255.0f),
                static_cast<uint8_t>(b * 255.0f),
                static_cast<uint8_t>(a * 255.0f)
            };
        }

        // Returns the color as a 32-bit int of the form 0xXXRRGGBB.
        constexpr uint32_t rgb_u32() const {
            const auto [r8, g8, b8, a8] = rgba_u8();
            return (uint32_t(r8) << 16) | (uint32_t(g8) << 8) | uint32_t(b8);
        }

        // Returns the color as a 32-bit int of the form 0xAARRGGBB.
        constexpr uint32_t rgba_u32() const {
            const auto [r8, g8, b8, a8] = rgba_u8();
            return (uint32_t(a8) << 24) | (uint32_t(r8) << 16) | (uint32_t(g8) << 8) | uint32_t(b8);
        }

        // Compares all four channels for equality.
        constexpr bool operator==(const color &other) const = default;
    };


    inline constexpr color black = color::cga(0);
    inline constexpr color blue = color::cga(1);
    inline constexpr color green = color::cga(2);
    inline constexpr color cyan = color::cga(3);
    inline constexpr color red = color::cga(4);
    inline constexpr color magenta = color::cga(5);
    inline constexpr color brown = color::cga(6);
    inline constexpr color light_gray = color::cga(7);
    inline constexpr color dark_gray = color::cga(8);
    inline constexpr color light_blue = color::cga(9);
    inline constexpr color light_green = color::cga(10);
    inline constexpr color light_cyan = color::cga(11);
    inline constexpr color light_red = color::cga(12);
    inline constexpr color light_magenta = color::cga(13);
    inline constexpr color yellow = color::cga(14);
    inline constexpr color white = color::cga(15);

    inline constexpr color pure_blue = color::from_rgb_u32(0x0000FFu);
    inline constexpr color pure_green = color::from_rgb_u32(0x00FF00u);
    inline constexpr color pure_cyan = color::from_rgb_u32(0x00FFFFu);
    inline constexpr color pure_red = color::from_rgb_u32(0xFF0000u);
    inline constexpr color pure_magenta = color::from_rgb_u32(0xFF00FFu);
    inline constexpr color pure_yellow = color::from_rgb_u32(0xFFFF00u);

    inline namespace color_literals {
        // Creates an opaque color from a hex literal of the form 0xRRGGBB_rgb.
        constexpr color operator""_rgb(unsigned long long c) {
            return color::from_rgb_u32(static_cast<uint32_t>(c));
        }
    } // namespace color_literals

} // namespace alia

#endif // ALIA_CORE_COLOR_HPP
