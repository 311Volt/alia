#ifndef PIXEL_FORMAT_CONVERSIONS_A5403053_FECF_4593_B185_70E123769583
#define PIXEL_FORMAT_CONVERSIONS_A5403053_FECF_4593_B185_70E123769583

#include "pixel_types.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace alia {

    namespace detail {

        struct red_channel {};
        struct green_channel {};
        struct blue_channel {};
        struct alpha_channel {};
        struct gray_channel {};
        struct x_channel {};
        struct y_channel {};

        template <typename...>
        inline constexpr bool always_false_v = false;

        template <pixel PixelT, typename ChannelT>
        inline constexpr int channel_bits_v = std::numeric_limits<channel_type_t<PixelT>>::digits;

        template <>
        inline constexpr int channel_bits_v<px_rgb565, red_channel> = 5;
        template <>
        inline constexpr int channel_bits_v<px_rgb565, green_channel> = 6;
        template <>
        inline constexpr int channel_bits_v<px_rgb565, blue_channel> = 5;

        template <pixel PixelT, typename ChannelT>
        inline constexpr std::uint64_t integer_channel_max_v = (std::uint64_t{1} << channel_bits_v<PixelT, ChannelT>) - 1;

        template <pixel TSrcPixel, pixel TDstPixel, typename TSrcChannel, typename TDstChannel = TSrcChannel>
        consteval bool channel_conversion_is_non_narrowing() {
            using src_channel_type = channel_type_t<TSrcPixel>;
            using dst_channel_type = channel_type_t<TDstPixel>;

            if constexpr (std::is_floating_point_v<src_channel_type> && std::is_floating_point_v<dst_channel_type>)
                return std::numeric_limits<dst_channel_type>::digits >= std::numeric_limits<src_channel_type>::digits;
            else if constexpr (std::is_integral_v<src_channel_type> && std::is_floating_point_v<dst_channel_type>)
                return true;
            else if constexpr (std::is_integral_v<src_channel_type> && std::is_integral_v<dst_channel_type>)
                return channel_bits_v<TDstPixel, TDstChannel> >= channel_bits_v<TSrcPixel, TSrcChannel>;
            else
                return false;
        }

        template <pixel TSrcPixel, pixel TDstPixel, typename TSrcChannel, typename TDstChannel = TSrcChannel>
        [[nodiscard]] constexpr channel_type_t<TDstPixel>
        convert_channel_value(channel_type_t<TSrcPixel> value) noexcept {
            using src_channel_type = channel_type_t<TSrcPixel>;
            using dst_channel_type = channel_type_t<TDstPixel>;

            if constexpr (std::is_floating_point_v<src_channel_type> && std::is_floating_point_v<dst_channel_type>) {
                return static_cast<dst_channel_type>(value);
            } else if constexpr (std::is_integral_v<src_channel_type> && std::is_floating_point_v<dst_channel_type>) {
                return static_cast<dst_channel_type>(value)
                     / static_cast<dst_channel_type>(integer_channel_max_v<TSrcPixel, TSrcChannel>);
            } else if constexpr (std::is_integral_v<src_channel_type> && std::is_integral_v<dst_channel_type>) {
                constexpr auto src_max = integer_channel_max_v<TSrcPixel, TSrcChannel>;
                constexpr auto dst_max = integer_channel_max_v<TDstPixel, TDstChannel>;
                if constexpr (src_max == dst_max) {
                    return static_cast<dst_channel_type>(value);
                } else {
                    return static_cast<dst_channel_type>(
                        (static_cast<std::uint64_t>(value) * dst_max + src_max / 2) / src_max
                    );
                }
            } else {
                static_assert(always_false_v<TSrcPixel, TDstPixel>, "unsupported pixel channel conversion");
            }
        }

        template <pixel PixelT>
        [[nodiscard]] constexpr channel_type_t<PixelT> opaque_alpha() noexcept {
            using channel_type = channel_type_t<PixelT>;
            if constexpr (std::is_floating_point_v<channel_type>)
                return channel_type{1};
            else
                return static_cast<channel_type>(integer_channel_max_v<PixelT, alpha_channel>);
        }

        template <pixel PixelT>
        constexpr void set_red(PixelT &px, channel_type_t<PixelT> value) noexcept {
            if constexpr (requires { px.r = value; })
                px.r = value;
            else
                static_assert(always_false_v<PixelT>, "pixel type declares has_red but has no assignable .r member");
        }

        template <pixel PixelT>
        constexpr void set_green(PixelT &px, channel_type_t<PixelT> value) noexcept {
            if constexpr (requires { px.g = value; })
                px.g = value;
            else
                static_assert(always_false_v<PixelT>, "pixel type declares has_green but has no assignable .g member");
        }

        template <pixel PixelT>
        constexpr void set_blue(PixelT &px, channel_type_t<PixelT> value) noexcept {
            if constexpr (requires { px.b = value; })
                px.b = value;
            else
                static_assert(always_false_v<PixelT>, "pixel type declares has_blue but has no assignable .b member");
        }

        template <pixel PixelT>
        constexpr void set_alpha(PixelT &px, channel_type_t<PixelT> value) noexcept {
            if constexpr (requires { px.a = value; })
                px.a = value;
            else
                static_assert(always_false_v<PixelT>, "pixel type declares has_alpha but has no assignable .a member");
        }

        template <pixel PixelT>
        constexpr void set_gray(PixelT &px, channel_type_t<PixelT> value) noexcept {
            if constexpr (requires { px.v = value; })
                px.v = value;
            else
                static_assert(always_false_v<PixelT>, "pixel type declares has_gray but has no assignable .v member");
        }

        template <pixel PixelT>
        constexpr void set_x(PixelT &px, channel_type_t<PixelT> value) noexcept {
            if constexpr (requires { px.x = value; })
                px.x = value;
            else
                static_assert(always_false_v<PixelT>, "pixel type declares has_x but has no assignable .x member");
        }

        template <pixel PixelT>
        constexpr void set_y(PixelT &px, channel_type_t<PixelT> value) noexcept {
            if constexpr (requires { px.y = value; })
                px.y = value;
            else
                static_assert(always_false_v<PixelT>, "pixel type declares has_y but has no assignable .y member");
        }

        template <pixel TSrcPixel, pixel TDstPixel>
        consteval bool color_channels_are_non_narrowing() {
            return channel_conversion_is_non_narrowing<TSrcPixel, TDstPixel, red_channel>()
                && channel_conversion_is_non_narrowing<TSrcPixel, TDstPixel, green_channel>()
                && channel_conversion_is_non_narrowing<TSrcPixel, TDstPixel, blue_channel>();
        }

        template <pixel TSrcPixel, pixel TDstPixel>
        consteval bool alpha_channel_is_non_narrowing() {
            if constexpr (has_alpha_v<TSrcPixel> && has_alpha_v<TDstPixel>)
                return channel_conversion_is_non_narrowing<TSrcPixel, TDstPixel, alpha_channel>();
            else
                return true;
        }

        template <typename TSrcPixel, typename TDstPixel>
        consteval bool default_pixel_conversion_is_allowed() {
            using src_pixel = std::remove_cvref_t<TSrcPixel>;
            using dst_pixel = std::remove_cvref_t<TDstPixel>;

            if constexpr (!(pixel<src_pixel> && pixel<dst_pixel>)) {
                return false;
            } else if constexpr (std::is_same_v<src_pixel, dst_pixel>) {
                return true;
            } else if constexpr (has_gray_v<src_pixel> && has_gray_v<dst_pixel>) {
                return channel_conversion_is_non_narrowing<src_pixel, dst_pixel, gray_channel>();
            } else if constexpr (has_gray_v<src_pixel> && has_color_v<dst_pixel>) {
                return channel_conversion_is_non_narrowing<src_pixel, dst_pixel, gray_channel, red_channel>()
                    && channel_conversion_is_non_narrowing<src_pixel, dst_pixel, gray_channel, green_channel>()
                    && channel_conversion_is_non_narrowing<src_pixel, dst_pixel, gray_channel, blue_channel>();
            } else if constexpr (has_color_v<src_pixel> && has_color_v<dst_pixel>) {
                return (!has_alpha_v<src_pixel> || has_alpha_v<dst_pixel>)
                    && color_channels_are_non_narrowing<src_pixel, dst_pixel>()
                    && alpha_channel_is_non_narrowing<src_pixel, dst_pixel>();
            } else if constexpr (has_xy_v<src_pixel> && has_xy_v<dst_pixel>) {
                return channel_conversion_is_non_narrowing<src_pixel, dst_pixel, x_channel>()
                    && channel_conversion_is_non_narrowing<src_pixel, dst_pixel, y_channel>();
            } else {
                return false;
            }
        }

        template <typename TSrcPixel, typename TDstPixel>
        consteval bool alpha_drop_conversion_is_allowed() {
            using src_pixel = std::remove_cvref_t<TSrcPixel>;
            using dst_pixel = std::remove_cvref_t<TDstPixel>;

            if constexpr (!(pixel<src_pixel> && pixel<dst_pixel>))
                return false;
            else
                return has_color_v<src_pixel>
                    && has_color_v<dst_pixel>
                    && has_alpha_v<src_pixel>
                    && !has_alpha_v<dst_pixel>
                    && color_channels_are_non_narrowing<src_pixel, dst_pixel>();
        }

        template <typename TSrcPixel, typename TDstPixel>
        consteval bool grayscale_conversion_is_allowed() {
            using src_pixel = std::remove_cvref_t<TSrcPixel>;
            using dst_pixel = std::remove_cvref_t<TDstPixel>;

            if constexpr (!(pixel<src_pixel> && pixel<dst_pixel>))
                return false;
            else
                return has_color_v<src_pixel>
                    && has_gray_v<dst_pixel>
                    && channel_conversion_is_non_narrowing<src_pixel, dst_pixel, red_channel, gray_channel>()
                    && channel_conversion_is_non_narrowing<src_pixel, dst_pixel, green_channel, gray_channel>()
                    && channel_conversion_is_non_narrowing<src_pixel, dst_pixel, blue_channel, gray_channel>();
        }

        template <pixel TDstPixel, pixel TSrcPixel>
        [[nodiscard]] constexpr TDstPixel convert_color_channels(TSrcPixel src) noexcept {
            TDstPixel dst{};
            set_red(dst, convert_channel_value<TSrcPixel, TDstPixel, red_channel>(get_red(src)));
            set_green(dst, convert_channel_value<TSrcPixel, TDstPixel, green_channel>(get_green(src)));
            set_blue(dst, convert_channel_value<TSrcPixel, TDstPixel, blue_channel>(get_blue(src)));
            return dst;
        }

        template <typename F>
        [[nodiscard]] constexpr bool visit_pixel_format(pixel_format fmt, F &&f) {
            switch (fmt) {
            case pixel_format::rgb888:     return std::forward<F>(f).template operator()<px_rgb888>();
            case pixel_format::rgba8888:   return std::forward<F>(f).template operator()<px_rgba8888>();
            case pixel_format::rgb565:     return std::forward<F>(f).template operator()<px_rgb565>();
            case pixel_format::bgr888:     return std::forward<F>(f).template operator()<px_bgr888>();
            case pixel_format::bgra8888:   return std::forward<F>(f).template operator()<px_bgra8888>();
            case pixel_format::gray_u8:    return std::forward<F>(f).template operator()<px_gray_u8>();
            case pixel_format::gray_f32:   return std::forward<F>(f).template operator()<px_gray_f32>();
            case pixel_format::rgba_f32:   return std::forward<F>(f).template operator()<px_rgba_f32>();
            case pixel_format::rgb_f32:    return std::forward<F>(f).template operator()<px_rgb_f32>();
            case pixel_format::xy_u8:      return std::forward<F>(f).template operator()<px_xy_u8>();
            case pixel_format::xy_f32:     return std::forward<F>(f).template operator()<px_xy_f32>();
            case pixel_format::palette_u8: return std::forward<F>(f).template operator()<px_palette_u8>();
            default:                       return false;
            }
        }

    } // namespace detail

    template <typename TSrcPixel, typename TDstPixel>
    struct is_convert_pixel_allowed
        : std::bool_constant<detail::default_pixel_conversion_is_allowed<TSrcPixel, TDstPixel>()> {};

    template <typename TSrcPixel, typename TDstPixel>
    inline constexpr bool is_convert_pixel_allowed_v = is_convert_pixel_allowed<TSrcPixel, TDstPixel>::value;

    template <pixel TDstPixel, pixel TSrcPixel>
    [[nodiscard]] constexpr TDstPixel convert_pixel(TSrcPixel src) noexcept
        requires is_convert_pixel_allowed_v<TSrcPixel, TDstPixel>
    {
        if constexpr (std::is_same_v<TSrcPixel, TDstPixel>) {
            return src;
        } else if constexpr (has_gray_v<TSrcPixel> && has_gray_v<TDstPixel>) {
            TDstPixel dst{};
            detail::set_gray(dst, detail::convert_channel_value<TSrcPixel, TDstPixel, detail::gray_channel>(get_gray(src)));
            return dst;
        } else if constexpr (has_gray_v<TSrcPixel> && has_color_v<TDstPixel>) {
            TDstPixel dst{};
            detail::set_red(dst, detail::convert_channel_value<TSrcPixel, TDstPixel, detail::gray_channel, detail::red_channel>(get_gray(src)));
            detail::set_green(dst, detail::convert_channel_value<TSrcPixel, TDstPixel, detail::gray_channel, detail::green_channel>(get_gray(src)));
            detail::set_blue(dst, detail::convert_channel_value<TSrcPixel, TDstPixel, detail::gray_channel, detail::blue_channel>(get_gray(src)));
            if constexpr (has_alpha_v<TDstPixel>)
                detail::set_alpha(dst, detail::opaque_alpha<TDstPixel>());
            return dst;
        } else if constexpr (has_color_v<TSrcPixel> && has_color_v<TDstPixel>) {
            TDstPixel dst = detail::convert_color_channels<TDstPixel>(src);
            if constexpr (has_alpha_v<TSrcPixel> && has_alpha_v<TDstPixel>)
                detail::set_alpha(dst, detail::convert_channel_value<TSrcPixel, TDstPixel, detail::alpha_channel>(get_alpha(src)));
            else if constexpr (has_alpha_v<TDstPixel>)
                detail::set_alpha(dst, detail::opaque_alpha<TDstPixel>());
            return dst;
        } else if constexpr (has_xy_v<TSrcPixel> && has_xy_v<TDstPixel>) {
            TDstPixel dst{};
            detail::set_x(dst, detail::convert_channel_value<TSrcPixel, TDstPixel, detail::x_channel>(get_x(src)));
            detail::set_y(dst, detail::convert_channel_value<TSrcPixel, TDstPixel, detail::y_channel>(get_y(src)));
            return dst;
        } else {
            static_assert(detail::always_false_v<TSrcPixel, TDstPixel>, "unsupported default pixel conversion");
        }
    }

    template <pixel TDstPixel, pixel TSrcPixel>
    [[nodiscard]] constexpr TDstPixel convert_pixel_no_preserve_alpha(TSrcPixel src) noexcept
        requires (detail::alpha_drop_conversion_is_allowed<TSrcPixel, TDstPixel>())
    {
        return detail::convert_color_channels<TDstPixel>(src);
    }

    template <pixel TDstPixel, pixel TSrcPixel>
    [[nodiscard]] constexpr TDstPixel convert_to_grayscale(TSrcPixel src) noexcept
        requires (detail::grayscale_conversion_is_allowed<TSrcPixel, TDstPixel>())
    {
        TDstPixel dst{};
        using dst_channel_type = channel_type_t<TDstPixel>;

        const auto r = detail::convert_channel_value<TSrcPixel, TDstPixel, detail::red_channel, detail::gray_channel>(get_red(src));
        const auto g = detail::convert_channel_value<TSrcPixel, TDstPixel, detail::green_channel, detail::gray_channel>(get_green(src));
        const auto b = detail::convert_channel_value<TSrcPixel, TDstPixel, detail::blue_channel, detail::gray_channel>(get_blue(src));

        if constexpr (std::is_integral_v<dst_channel_type>) {
            const auto gray = static_cast<dst_channel_type>(
                static_cast<std::uint16_t>(77u * r + 150u * g + 30u * b) >> 8
            );
            detail::set_gray(dst, gray);
        } else {
            const auto gray = static_cast<dst_channel_type>(
                r * dst_channel_type{0.299f} + g * dst_channel_type{0.587f} + b * dst_channel_type{0.114f}
            );
            detail::set_gray(dst, gray);
        }

        return dst;
    }

    template <pixel TDstPixel, pixel TSrcPixel>
    [[nodiscard]] constexpr TDstPixel convert_to_grayscale_fast(TSrcPixel src) noexcept
        requires (detail::grayscale_conversion_is_allowed<TSrcPixel, TDstPixel>())
    {
        TDstPixel dst{};
        using dst_channel_type = channel_type_t<TDstPixel>;

        const auto r = detail::convert_channel_value<TSrcPixel, TDstPixel, detail::red_channel, detail::gray_channel>(get_red(src));
        const auto g = detail::convert_channel_value<TSrcPixel, TDstPixel, detail::green_channel, detail::gray_channel>(get_green(src));
        const auto b = detail::convert_channel_value<TSrcPixel, TDstPixel, detail::blue_channel, detail::gray_channel>(get_blue(src));

        if constexpr (std::is_integral_v<dst_channel_type>) {
            detail::set_gray(dst, static_cast<dst_channel_type>((static_cast<std::uint16_t>(r) + 2u * g + b) >> 2));
        } else {
            detail::set_gray(dst, static_cast<dst_channel_type>((r + dst_channel_type{2} * g + b) * dst_channel_type{0.25f}));
        }

        return dst;
    }

    template <pixel TDstPixel, pixel TSrcPixel>
    [[nodiscard]] constexpr TDstPixel convert_from_palette(TSrcPixel src, std::span<const TDstPixel, 256> palette) noexcept
        requires has_palette_index_v<TSrcPixel>
    {
        return palette[static_cast<std::size_t>(get_palette_index(src))];
    }

    [[nodiscard]] inline constexpr bool can_convert_pixel(pixel_format src, pixel_format dst) noexcept {
        return detail::visit_pixel_format(src, [&]<pixel TSrcPixel>() constexpr {
            return detail::visit_pixel_format(dst, [&]<pixel TDstPixel>() constexpr {
                return is_convert_pixel_allowed_v<TSrcPixel, TDstPixel>;
            });
        });
    }

} // namespace alia

#endif /* PIXEL_FORMAT_CONVERSIONS_A5403053_FECF_4593_B185_70E123769583 */
