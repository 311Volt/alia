#ifndef PIXEL_CONVERTERS_02ADBD69_B842_45D2_88FD_C889D142EA61
#define PIXEL_CONVERTERS_02ADBD69_B842_45D2_88FD_C889D142EA61

#include "pixel_format_conversions.hpp"
#include <alia/core/rect.hpp>
#include <alia/core/vec.hpp>
#include <concepts>
#include <functional>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace alia {

    template <typename T>
    concept hub_convertible_pixel = pixel<T> && !has_palette_index_v<T>;

    template <typename T>
    concept rgba_hub_pixel = std::same_as<T, px_rgba8888> || std::same_as<T, px_rgba_f32>;

    namespace detail {

        template <hub_convertible_pixel THub, hub_convertible_pixel TSrc>
        [[nodiscard]] constexpr THub to_hub(TSrc src) noexcept {
            THub hub{};
            if constexpr (has_color_v<TSrc>) {
                set_red(hub, convert_channel_value<TSrc, THub, red_channel>(get_red(src)));
                set_green(hub, convert_channel_value<TSrc, THub, green_channel>(get_green(src)));
                set_blue(hub, convert_channel_value<TSrc, THub, blue_channel>(get_blue(src)));
            } else if constexpr (has_gray_v<TSrc>) {
                set_red(hub, convert_channel_value<TSrc, THub, gray_channel, red_channel>(get_gray(src)));
                set_green(hub, convert_channel_value<TSrc, THub, gray_channel, green_channel>(get_gray(src)));
                set_blue(hub, convert_channel_value<TSrc, THub, gray_channel, blue_channel>(get_gray(src)));
            } else if constexpr (has_xy_v<TSrc>) {
                set_red(hub, convert_channel_value<TSrc, THub, x_channel, red_channel>(get_x(src)));
                set_green(hub, convert_channel_value<TSrc, THub, y_channel, green_channel>(get_y(src)));
            }

            if constexpr (has_alpha_v<TSrc>)
                set_alpha(hub, convert_channel_value<TSrc, THub, alpha_channel>(get_alpha(src)));
            else
                set_alpha(hub, opaque_alpha<THub>());
            return hub;
        }

        template <hub_convertible_pixel TDst, hub_convertible_pixel THub>
        [[nodiscard]] constexpr TDst from_hub(THub hub) noexcept {
            TDst dst{};
            if constexpr (has_color_v<TDst>) {
                set_red(dst, convert_channel_value<THub, TDst, red_channel>(get_red(hub)));
                set_green(dst, convert_channel_value<THub, TDst, green_channel>(get_green(hub)));
                set_blue(dst, convert_channel_value<THub, TDst, blue_channel>(get_blue(hub)));
            } else if constexpr (has_gray_v<TDst>) {
                const auto gray = luminance(get_red(hub), get_green(hub), get_blue(hub));
                set_gray(dst, convert_channel_value<THub, TDst, red_channel, gray_channel>(gray));
            } else if constexpr (has_xy_v<TDst>) {
                set_x(dst, convert_channel_value<THub, TDst, red_channel, x_channel>(get_red(hub)));
                set_y(dst, convert_channel_value<THub, TDst, green_channel, y_channel>(get_green(hub)));
            }

            if constexpr (has_alpha_v<TDst>)
                set_alpha(dst, convert_channel_value<THub, TDst, alpha_channel>(get_alpha(hub)));
            return dst;
        }

    } // namespace detail

    /// Converts to an 8-bit RGBA hub. Float channels are clamped to [0, 1],
    /// NaN becomes zero, and integer channels are rounded to the nearest value.
    /// Gray is replicated into RGB; XY maps to RG with B = 0. Missing alpha is opaque.
    template <hub_convertible_pixel TSrc>
    [[nodiscard]] constexpr px_rgba8888 to_rgba8888(TSrc src) noexcept {
        return detail::to_hub<px_rgba8888>(src);
    }

    /// Converts from the 8-bit RGBA hub. Gray uses Rec.601 luminance; XY uses RG.
    /// Destinations without alpha discard it without compositing or premultiplication.
    template <hub_convertible_pixel TDst>
    [[nodiscard]] constexpr TDst from_rgba8888(px_rgba8888 src) noexcept {
        return detail::from_hub<TDst>(src);
    }

    /// Converts to a float RGBA hub. Integer channels are normalized to [0, 1];
    /// float channels keep their range. Gray/XY and missing alpha map as above.
    template <hub_convertible_pixel TSrc>
    [[nodiscard]] constexpr px_rgba_f32 to_rgba_f32(TSrc src) noexcept {
        return detail::to_hub<px_rgba_f32>(src);
    }

    /// Converts from the float RGBA hub. Float destinations keep their range;
    /// integer destinations clamp, map NaN to zero, and round to nearest.
    template <hub_convertible_pixel TDst>
    [[nodiscard]] constexpr TDst from_rgba_f32(px_rgba_f32 src) noexcept {
        return detail::from_hub<TDst>(src);
    }

    /// Converts any non-palette pixel, preserving the strict conversion when allowed.
    /// Otherwise uses the float hub if either pixel has float channels, or the u8 hub.
    template <hub_convertible_pixel TDst, hub_convertible_pixel TSrc>
    [[nodiscard]] constexpr TDst convert_pixel_lossy(TSrc src) noexcept {
        if constexpr (is_convert_pixel_allowed_v<TSrc, TDst>)
            return convert_pixel<TDst>(src);
        else if constexpr (is_floating_point_pixel_v<TSrc> || is_floating_point_pixel_v<TDst>)
            return from_rgba_f32<TDst>(to_rgba_f32(src));
        else
            return from_rgba8888<TDst>(to_rgba8888(src));
    }

    template <pixel TPixel>
    class bitmap_view;
    class any_bitmap_view;

    /// Like converting_blit, allowing lossy conversions between non-palette pixels.
    template <hub_convertible_pixel TDst, hub_convertible_pixel TSrc>
    void converting_blit_lossy(bitmap_view<TDst> dst, bitmap_view<TSrc> src) {
        converting_blit(dst, src, [](TSrc px) { return convert_pixel_lossy<TDst>(px); });
    }

    template <hub_convertible_pixel TDst, hub_convertible_pixel TSrc>
    void converting_blit_lossy(bitmap_view<TDst> dst, bitmap_view<TSrc> src, rect_i src_rect, vec2i dst_pos) {
        converting_blit(dst, src, src_rect, dst_pos, [](TSrc px) { return convert_pixel_lossy<TDst>(px); });
    }

    /// True for known, non-palette formats, including conversions that lose channels.
    [[nodiscard]] bool can_convert_pixel_lossy(pixel_format src, pixel_format dst) noexcept;

    /// Converts rows through an RGBA hub, with the same size/rectangle rules as
    /// converting_blit. Palette and unknown formats throw std::invalid_argument
    /// before any destination pixels are written, even for empty views.
    void converting_blit_lossy(any_bitmap_view dst, any_bitmap_view src);
    void converting_blit_lossy(any_bitmap_view dst, any_bitmap_view src, rect_i src_rect, vec2i dst_pos);

    namespace detail {

        // Precondition: can_convert_pixel_lossy(src_fmt, dst_fmt). Buffers must
        // contain count pixels in their respective formats and must not overlap.
        void convert_pixel_row_lossy(pixel_format dst_fmt, void *dst, pixel_format src_fmt, const void *src, int count);

        [[nodiscard]] inline bool has_float_channels(pixel_format fmt) noexcept {
            switch (fmt) {
            case pixel_format::gray_f32:
            case pixel_format::rgba_f32:
            case pixel_format::rgb_f32:
            case pixel_format::xy_f32:
                return true;
            default:
                return false;
            }
        }

        // Visits rows in region-local coordinates. Matching formats use the
        // buffer directly; other formats reuse one value-initialised hub row.
        // Read-write visits through the u8 hub reject float storage to avoid
        // quantizing pixels that the callback leaves untouched.
        template <rgba_hub_pixel THub, bool Read, bool Write, class F>
        [[nodiscard]] bool visit_pixel_rows(pixel_format fmt, std::byte *data, int stride, vec2i extent, F &&f) {
            if (!can_convert_pixel_lossy(fmt, THub::format_id))
                return false;
            if constexpr (Read && Write && std::same_as<THub, px_rgba8888>) {
                if (has_float_channels(fmt))
                    return false;
            }
            if (extent.x <= 0 || extent.y <= 0)
                return true;

            using row_pixel = std::conditional_t<Write, THub, const THub>;
            if (fmt == THub::format_id) {
                for (int y = 0; y < extent.y; ++y) {
                    auto *row = reinterpret_cast<THub *>(data + static_cast<std::ptrdiff_t>(y) * stride);
                    std::invoke(f, std::span<row_pixel>(row, extent.x), y);
                }
            } else {
                auto row = std::make_unique<THub[]>(extent.x);
                for (int y = 0; y < extent.y; ++y) {
                    auto *native = data + static_cast<std::ptrdiff_t>(y) * stride;
                    if constexpr (Read)
                        convert_pixel_row_lossy(THub::format_id, row.get(), fmt, native, extent.x);
                    std::invoke(f, std::span<row_pixel>(row.get(), extent.x), y);
                    if constexpr (Write)
                        convert_pixel_row_lossy(fmt, native, THub::format_id, row.get(), extent.x);
                }
            }
            return true;
        }

        // Dispatches a native view without copying. A callback can constrain
        // its accepted pixel types; unknown or rejected formats return false.
        template <bool Const, class F>
        [[nodiscard]] bool visit_pixel_view(pixel_format fmt, std::byte *data, int stride, vec2i extent, F &&f) {
            return visit_pixel_format(fmt, [&]<pixel P>() {
                using view_type = std::conditional_t<Const, const bitmap_view<P>, bitmap_view<P>>;
                if constexpr (std::is_invocable_v<F &, view_type &>) {
                    view_type view(extent.x, extent.y, stride, data);
                    std::invoke(f, view);
                    return true;
                } else {
                    return false;
                }
            });
        }

    } // namespace detail

} // namespace alia

#endif /* PIXEL_CONVERTERS_02ADBD69_B842_45D2_88FD_C889D142EA61 */
