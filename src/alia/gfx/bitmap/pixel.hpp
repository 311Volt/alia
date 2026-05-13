#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace alia {

    // ============================================================================
    // Pixel format identifier
    //
    // Each pixel struct carries `static constexpr pixel_format format_id`
    // and the pixel concept enforces its presence.
    // ============================================================================

    enum class pixel_format : uint8_t {
        rgb888,
        rgba8888,
        rgb565,
        bgr888,
        bgra8888,
        gray_u8,
        gray_f32,
        rgba_f32,
        rgb_f32,
        xy_u8,
        xy_f32,
        palette_u8,
    };

    // Returns the number of bytes per pixel for a pixel_format.
    [[nodiscard]] inline constexpr int bytes_per_pixel_for_format(pixel_format fmt) noexcept {
        switch (fmt) {
        case pixel_format::rgb888:   return 3;
        case pixel_format::rgba8888: return 4;
        case pixel_format::rgb565:   return 2;
        case pixel_format::bgr888:   return 3;
        case pixel_format::bgra8888: return 4;
        case pixel_format::gray_u8:  return 1;
        case pixel_format::gray_f32: return 4;
        case pixel_format::rgba_f32: return 16;
        case pixel_format::rgb_f32:  return 12;
        case pixel_format::xy_u8:    return 2;
        case pixel_format::xy_f32:   return 8;
        case pixel_format::palette_u8: return 1;
        default:                     return 0;
        }
    }

    template <typename T>
    inline constexpr bool is_pixel_type_v = requires {
        typename std::remove_cvref_t<T>::is_pixel_type;
    };

    template <typename...>
    inline constexpr bool dependent_false_v = false;

    // ============================================================================
    // Channel presence traits
    //
    // Detect via nested type aliases inside the pixel struct
    // (e.g. `using has_red = void;`).
    // ============================================================================

    template <typename T>
        struct has_red : std::bool_constant < requires {
        typename T::has_red;
    } > {};
    template <typename T>
        struct has_green : std::bool_constant < requires {
        typename T::has_green;
    } > {};
    template <typename T>
        struct has_blue : std::bool_constant < requires {
        typename T::has_blue;
    } > {};
    template <typename T>
        struct has_alpha : std::bool_constant < requires {
        typename T::has_alpha;
    } > {};
    template <typename T>
        struct has_gray : std::bool_constant < requires {
        typename T::has_gray;
    } > {};
    template <typename T>
        struct has_x : std::bool_constant < requires {
        typename T::has_x;
    } > {};
    template <typename T>
        struct has_y : std::bool_constant < requires {
        typename T::has_y;
    } > {};
    template <typename T>
        struct has_palette_index : std::bool_constant < requires {
        typename T::has_palette_index;
    } > {};

    template <typename T>
    inline constexpr bool has_red_v = has_red<T>::value;
    template <typename T>
    inline constexpr bool has_green_v = has_green<T>::value;
    template <typename T>
    inline constexpr bool has_blue_v = has_blue<T>::value;
    template <typename T>
    inline constexpr bool has_alpha_v = has_alpha<T>::value;
    template <typename T>
    inline constexpr bool has_gray_v = has_gray<T>::value;
    template <typename T>
    inline constexpr bool has_x_v = has_x<T>::value;
    template <typename T>
    inline constexpr bool has_y_v = has_y<T>::value;
    template <typename T>
    inline constexpr bool has_palette_index_v = has_palette_index<T>::value;

    // ============================================================================
    // channel_type — the natural numeric type for individual channel values
    //
    // Reads T::channel_type. For bit-field formats (px_rgb565) this is the
    // logical value type (uint8_t), not the backing storage type.
    // ============================================================================

    template <typename T, typename = void>
    struct channel_type {}; // no 'type' member for unknown types → SFINAE-friendly

    template <typename T>
    struct channel_type<T, std::void_t<typename T::channel_type>> {
        using type = typename T::channel_type;
    };

    template <typename T>
    using channel_type_t = typename channel_type<T>::type;

    // ============================================================================
    // Additional traits
    //
    // Derived from struct-embedded info.
    // ============================================================================

    // bits_per_pixel — valid for no-padding types (all sizeof * 8 == logical bits).
    template <typename T>
    inline constexpr std::size_t bits_per_pixel_v = sizeof(T) * 8;

    // is_floating_point_pixel — derived from channel_type.
    template <typename T>
    inline constexpr bool is_floating_point_pixel_v =
        requires { typename channel_type<T>::type; } && std::is_floating_point_v<channel_type_t<T>>;

    // has_color — true when R, G, B channels are all present.
    template <typename T>
    inline constexpr bool has_color_v = has_red_v<T> && has_green_v<T> && has_blue_v<T>;

    // has_xy — true when X and Y channels are both present.
    template <typename T>
    inline constexpr bool has_xy_v = has_x_v<T> && has_y_v<T>;

    // ============================================================================
    // pixel concept
    //
    // Satisfied by pixel types that:
    //   - are trivially copyable and standard-layout (safe to memcpy / cast),
    //   - have a channel_type::type,
    //   - have a supported logical channel group.
    // ============================================================================

    template <typename T>
    concept pixel = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T> && is_pixel_type_v<T> && requires {
        typename channel_type<T>::type;
    } && (has_color_v<T> || has_gray_v<T> || has_xy_v<T> || has_palette_index_v<T>) && requires { requires std::is_same_v<std::remove_cv_t<decltype(T::format_id)>, pixel_format>; };

    // ============================================================================
    // Channel getters
    //
    // Dispatch order for each channel:
    //   1. T::get_<channel>() method
    //   2. T::<member>  (.r/.g/.b/.a/.v)
    //   3. static_assert
    //

    // ============================================================================

    template <pixel PixelT>
        requires has_red_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_red(PixelT px) noexcept {
        if constexpr (requires { px.get_red(); })
            return static_cast<channel_type_t<PixelT>>(px.get_red());
        else if constexpr (requires { px.r; })
            return static_cast<channel_type_t<PixelT>>(px.r);
        else
            static_assert(dependent_false_v<PixelT>, "pixel type declares has_red but has no get_red() or .r member");
    }

    template <pixel PixelT>
        requires has_green_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_green(PixelT px) noexcept {
        if constexpr (requires { px.get_green(); })
            return static_cast<channel_type_t<PixelT>>(px.get_green());
        else if constexpr (requires { px.g; })
            return static_cast<channel_type_t<PixelT>>(px.g);
        else
            static_assert(dependent_false_v<PixelT>, "pixel type declares has_green but has no get_green() or .g member");
    }

    template <pixel PixelT>
        requires has_blue_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_blue(PixelT px) noexcept {
        if constexpr (requires { px.get_blue(); })
            return static_cast<channel_type_t<PixelT>>(px.get_blue());
        else if constexpr (requires { px.b; })
            return static_cast<channel_type_t<PixelT>>(px.b);
        else
            static_assert(dependent_false_v<PixelT>, "pixel type declares has_blue but has no get_blue() or .b member");
    }

    template <pixel PixelT>
        requires has_alpha_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_alpha(PixelT px) noexcept {
        if constexpr (requires { px.get_alpha(); })
            return static_cast<channel_type_t<PixelT>>(px.get_alpha());
        else if constexpr (requires { px.a; })
            return static_cast<channel_type_t<PixelT>>(px.a);
        else
            static_assert(dependent_false_v<PixelT>, "pixel type declares has_alpha but has no get_alpha() or .a member");
    }

    template <pixel PixelT>
        requires has_gray_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_gray(PixelT px) noexcept {
        if constexpr (requires { px.get_gray(); })
            return static_cast<channel_type_t<PixelT>>(px.get_gray());
        else if constexpr (requires { px.v; })
            return static_cast<channel_type_t<PixelT>>(px.v);
        else
            static_assert(dependent_false_v<PixelT>, "pixel type declares has_gray but has no get_gray() or .v member");
    }

    template <pixel PixelT>
        requires has_x_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_x(PixelT px) noexcept {
        if constexpr (requires { px.get_x(); })
            return static_cast<channel_type_t<PixelT>>(px.get_x());
        else if constexpr (requires { px.x; })
            return static_cast<channel_type_t<PixelT>>(px.x);
        else
            static_assert(dependent_false_v<PixelT>, "pixel type declares has_x but has no get_x() or .x member");
    }

    template <pixel PixelT>
        requires has_y_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_y(PixelT px) noexcept {
        if constexpr (requires { px.get_y(); })
            return static_cast<channel_type_t<PixelT>>(px.get_y());
        else if constexpr (requires { px.y; })
            return static_cast<channel_type_t<PixelT>>(px.y);
        else
            static_assert(dependent_false_v<PixelT>, "pixel type declares has_y but has no get_y() or .y member");
    }

    template <pixel PixelT>
        requires has_palette_index_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_palette_index(PixelT px) noexcept {
        if constexpr (requires { px.get_palette_index(); })
            return static_cast<channel_type_t<PixelT>>(px.get_palette_index());
        else if constexpr (requires { px.palette_index; })
            return static_cast<channel_type_t<PixelT>>(px.palette_index);
        else
            static_assert(dependent_false_v<PixelT>, "pixel type declares has_palette_index but has no get_palette_index() or .palette_index member");
    }

} // namespace alia
