#pragma once

#include "pixel.hpp"

namespace alia {


    // ============================================================================
    // Pixel format structs
    //
    // Each struct embeds its own trait information as nested type aliases:
    //   - `using has_<channel> = void;`  declares which channels are present.
    //   - `using channel_type = ...;`      declares the channel value type.
    //
    // This means no external specializations are needed for user-defined types
    // that follow the same convention. For library types you cannot modify,
    // specialize the trait templates below.
    //
    // All types satisfy: no internal padding, trivially copyable, standard layout.
    // It is safe to reinterpret_cast a contiguous byte buffer as an array of any
    // of these types (alignment is at most 4 bytes, all sizes divide evenly).
    // ============================================================================

    struct px_rgb888 {
        uint8_t r, g, b;

        using has_red = void;
        using has_green = void;
        using has_blue = void;
        using channel_type = uint8_t;
        static constexpr pixel_format format_id = pixel_format::rgb888;

        // Unpack from a 24-bit value: R[23:16] G[15:8] B[7:0]
        static constexpr px_rgb888 of(uint32_t v) noexcept {
            return {uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
        }
    };
    static_assert(sizeof(px_rgb888) == 3, "px_rgb888 must not have padding");
    static_assert(std::is_trivially_copyable_v<px_rgb888>);
    static_assert(std::is_standard_layout_v<px_rgb888>);

    struct px_rgba8888 {
        uint8_t r, g, b, a;

        using has_red = void;
        using has_green = void;
        using has_blue = void;
        using has_alpha = void;
        using channel_type = uint8_t;
        static constexpr pixel_format format_id = pixel_format::rgba8888;

        // Unpack from a 32-bit value: R[31:24] G[23:16] B[15:8] A[7:0]
        static constexpr px_rgba8888 of(uint32_t v) noexcept {
            return {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
        }
    };
    static_assert(sizeof(px_rgba8888) == 4, "px_rgba8888 must not have padding");
    static_assert(std::is_trivially_copyable_v<px_rgba8888>);
    static_assert(std::is_standard_layout_v<px_rgba8888>);

    // Packed 16-bit RGB, little-endian bit order: B[4:0] G[10:5] R[15:11].
    // This matches the conventional RGB565 memory layout on little-endian targets.
    struct px_rgb565 {
        uint16_t b : 5;
        uint16_t g : 6;
        uint16_t r : 5;

        using has_red = void;
        using has_green = void;
        using has_blue = void;
        using channel_type = uint8_t; // logical channel width fits in uint8_t
        static constexpr pixel_format format_id = pixel_format::rgb565;

        // Unpack from a raw 16-bit word: R[15:11] G[10:5] B[4:0]
        static constexpr px_rgb565 of(uint16_t v) noexcept {
            return {uint16_t(v & 0x1F), uint16_t((v >> 5) & 0x3F), uint16_t((v >> 11) & 0x1F)};
        }
    };
    static_assert(sizeof(px_rgb565) == 2, "px_rgb565 must not have padding");
    static_assert(std::is_trivially_copyable_v<px_rgb565>);
    static_assert(std::is_standard_layout_v<px_rgb565>);
    // Note: standard_layout is not guaranteed for bit-field structs by the
    // standard, but holds on all mainstream compilers for this layout.

    struct px_bgr888 {
        uint8_t b, g, r;

        using has_red = void;
        using has_green = void;
        using has_blue = void;
        using channel_type = uint8_t;
        static constexpr pixel_format format_id = pixel_format::bgr888;

        // Unpack from a 24-bit value: B[23:16] G[15:8] R[7:0]
        static constexpr px_bgr888 of(uint32_t v) noexcept {
            return {uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
        }
    };
    static_assert(sizeof(px_bgr888) == 3, "px_bgr888 must not have padding");
    static_assert(std::is_trivially_copyable_v<px_bgr888>);
    static_assert(std::is_standard_layout_v<px_bgr888>);

    struct px_bgra8888 {
        uint8_t b, g, r, a;

        using has_red = void;
        using has_green = void;
        using has_blue = void;
        using has_alpha = void;
        using channel_type = uint8_t;
        static constexpr pixel_format format_id = pixel_format::bgra8888;

        // Unpack from a 32-bit value: B[31:24] G[23:16] R[15:8] A[7:0]
        static constexpr px_bgra8888 of(uint32_t v) noexcept {
            return {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
        }
    };
    static_assert(sizeof(px_bgra8888) == 4, "px_bgra8888 must not have padding");
    static_assert(std::is_trivially_copyable_v<px_bgra8888>);
    static_assert(std::is_standard_layout_v<px_bgra8888>);

    struct px_gray_u8 {
        uint8_t v;

        using has_gray = void;
        using channel_type = uint8_t;
        static constexpr pixel_format format_id = pixel_format::gray_u8;
    };
    static_assert(sizeof(px_gray_u8) == 1, "px_gray_u8 must not have padding");
    static_assert(std::is_trivially_copyable_v<px_gray_u8>);
    static_assert(std::is_standard_layout_v<px_gray_u8>);

    struct px_gray_f32 {
        float v;

        using has_gray = void;
        using channel_type = float;
        static constexpr pixel_format format_id = pixel_format::gray_f32;
    };
    static_assert(sizeof(px_gray_f32) == 4, "px_gray_f32 must not have padding");
    static_assert(std::is_trivially_copyable_v<px_gray_f32>);
    static_assert(std::is_standard_layout_v<px_gray_f32>);

    struct px_rgba_f32 {
        float r, g, b, a;

        using has_red = void;
        using has_green = void;
        using has_blue = void;
        using has_alpha = void;
        using channel_type = float;
        static constexpr pixel_format format_id = pixel_format::rgba_f32;
    };
    static_assert(sizeof(px_rgba_f32) == 16, "px_rgba_f32 must not have padding");
    static_assert(std::is_trivially_copyable_v<px_rgba_f32>);
    static_assert(std::is_standard_layout_v<px_rgba_f32>);

    struct px_rgb_f32 {
        float r, g, b;

        using has_red = void;
        using has_green = void;
        using has_blue = void;
        using channel_type = float;
        static constexpr pixel_format format_id = pixel_format::rgb_f32;
    };
    static_assert(sizeof(px_rgb_f32) == 12, "px_rgb_f32 must not have padding");
    static_assert(std::is_trivially_copyable_v<px_rgb_f32>);
    static_assert(std::is_standard_layout_v<px_rgb_f32>);

} // namespace alia
