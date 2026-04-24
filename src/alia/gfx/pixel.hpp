#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace alia {


    // ============================================================================
    // Pixel format identifier
    //
    // Each built-in pixel struct carries `static constexpr pixel_format format_id`
    // and the pixel concept enforces its presence. User-defined types must also
    // provide format_id; use pixel_format::custom for non-built-in formats.
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
        custom, // for user-defined formats outside this list
    };

    // Returns the number of bytes per pixel for a built-in pixel_format.
	// Returns 0 for pixel_format::custom (size is unknown without the concrete type).
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
			default:                     return 0;
		}
	}

    // ============================================================================
    // Channel presence traits
    //
    // Default: detect via nested type alias inside the pixel struct
    // (e.g. `using has_red = void;`). Explicitly specialize for library types
    // you cannot modify.
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
    inline constexpr bool has_red_v = has_red<T>::value;
    template <typename T>
    inline constexpr bool has_green_v = has_green<T>::value;
    template <typename T>
    inline constexpr bool has_blue_v = has_blue<T>::value;
    template <typename T>
    inline constexpr bool has_alpha_v = has_alpha<T>::value;
    template <typename T>
    inline constexpr bool has_gray_v = has_gray<T>::value;

    // ============================================================================
    // channel_type — the natural numeric type for individual channel values
    //
    // Default: reads T::channel_type. For bit-field formats (px_rgb565) this is the
    // logical value type (uint8_t), not the backing storage type.
    // Explicitly specialize for library types you cannot modify.
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
    // All default correctly from struct-embedded info. Specialize as variable
    // templates for library types you cannot modify.
    // ============================================================================

    // bits_per_pixel — valid for no-padding types (all sizeof * 8 == logical bits).
    template <typename T>
    inline constexpr std::size_t bits_per_pixel_v = sizeof(T) * 8;

    // is_floating_point_pixel — derived from channel_type.
    template <typename T>
    inline constexpr bool is_floating_point_pixel_v = requires { typename channel_type<T>::type; } && std::is_floating_point_v<channel_type_t<T>>;

    // has_color — true when R, G, B channels are all present.
    template <typename T>
    inline constexpr bool has_color_v = has_red_v<T> && has_green_v<T> && has_blue_v<T>;

    // ============================================================================
    // pixel concept
    //
    // Satisfied by any type that:
    //   - is trivially copyable and standard-layout (safe to memcpy / cast),
    //   - has a channel_type::type (via T::channel_type or explicit specialization),
    //   - has at least one logical channel (color or grayscale).
    // ============================================================================

    template <typename T>
    concept pixel =
        std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T> &&
        requires { typename channel_type<T>::type; } &&
        (has_red_v<T> || has_gray_v<T>) &&
        requires { requires std::is_same_v<std::remove_cv_t<decltype(T::format_id)>, pixel_format>; };

    // ============================================================================
    // Channel getters
    //
    // Dispatch order for each channel:
    //   1. T::get_<channel>() method  — for custom accessor logic
    //   2. T::<member>  (.r/.g/.b/.a/.v)  — for plain struct fields / bitfields
    //   3. static_assert  — clear error if the type advertises a channel it lacks
    //
    // User types must follow the convention above OR define the matching method.
    // ============================================================================

    template <pixel PixelT>
        requires has_red_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_red(PixelT px) noexcept {
        if constexpr (requires { px.get_red(); })
            return static_cast<channel_type_t<PixelT>>(px.get_red());
        else if constexpr (requires { px.r; })
            return static_cast<channel_type_t<PixelT>>(px.r);
        else
            static_assert(false, "pixel type declares has_red but has no get_red() or .r member");
    }

    template <pixel PixelT>
        requires has_green_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_green(PixelT px) noexcept {
        if constexpr (requires { px.get_green(); })
            return static_cast<channel_type_t<PixelT>>(px.get_green());
        else if constexpr (requires { px.g; })
            return static_cast<channel_type_t<PixelT>>(px.g);
        else
            static_assert(false, "pixel type declares has_green but has no get_green() or .g member");
    }

    template <pixel PixelT>
        requires has_blue_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_blue(PixelT px) noexcept {
        if constexpr (requires { px.get_blue(); })
            return static_cast<channel_type_t<PixelT>>(px.get_blue());
        else if constexpr (requires { px.b; })
            return static_cast<channel_type_t<PixelT>>(px.b);
        else
            static_assert(false, "pixel type declares has_blue but has no get_blue() or .b member");
    }

    template <pixel PixelT>
        requires has_alpha_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_alpha(PixelT px) noexcept {
        if constexpr (requires { px.get_alpha(); })
            return static_cast<channel_type_t<PixelT>>(px.get_alpha());
        else if constexpr (requires { px.a; })
            return static_cast<channel_type_t<PixelT>>(px.a);
        else
            static_assert(false, "pixel type declares has_alpha but has no get_alpha() or .a member");
    }

    template <pixel PixelT>
        requires has_gray_v<PixelT>
    [[nodiscard]] constexpr channel_type_t<PixelT> get_gray(PixelT px) noexcept {
        if constexpr (requires { px.get_gray(); })
            return static_cast<channel_type_t<PixelT>>(px.get_gray());
        else if constexpr (requires { px.v; })
            return static_cast<channel_type_t<PixelT>>(px.v);
        else
            static_assert(false, "pixel type declares has_gray but has no get_gray() or .v member");
    }

} // namespace alia
