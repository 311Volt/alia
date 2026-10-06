#include "bitmap.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace alia {

    namespace {

        template <hub_convertible_pixel THub>
        void convert_row_through_hub(pixel_format dst_fmt, void *dst, pixel_format src_fmt, const void *src, int count) {
            std::array<THub, 256> buffer;
            auto *dst_bytes = static_cast<std::byte *>(dst);
            const auto *src_bytes = static_cast<const std::byte *>(src);
            const int dst_bpp = bytes_per_pixel_for_format(dst_fmt);
            const int src_bpp = bytes_per_pixel_for_format(src_fmt);

            while (count > 0) {
                const int chunk_size = std::min(count, static_cast<int>(buffer.size()));
                (void)detail::visit_pixel_format(src_fmt, [&]<pixel TSrc>() {
                    if constexpr (hub_convertible_pixel<TSrc>) {
                        for (int i = 0; i < chunk_size; ++i) {
                            TSrc px;
                            // Byte views may have unaligned data or row strides.
                            std::memcpy(&px, src_bytes + i * sizeof(TSrc), sizeof(TSrc));
                            buffer[i] = detail::to_hub<THub>(px);
                        }
                        return true;
                    } else {
                        return false;
                    }
                });
                (void)detail::visit_pixel_format(dst_fmt, [&]<pixel TDst>() {
                    if constexpr (hub_convertible_pixel<TDst>) {
                        for (int i = 0; i < chunk_size; ++i) {
                            const auto px = detail::from_hub<TDst>(buffer[i]);
                            std::memcpy(dst_bytes + i * sizeof(TDst), &px, sizeof(TDst));
                        }
                        return true;
                    } else {
                        return false;
                    }
                });
                src_bytes += chunk_size * src_bpp;
                dst_bytes += chunk_size * dst_bpp;
                count -= chunk_size;
            }
        }

        void validate_lossy_formats(pixel_format src, pixel_format dst) {
            if (!can_convert_pixel_lossy(src, dst))
                throw std::invalid_argument(
                    "converting_blit_lossy: unsupported pixel format conversion (palette formats require a palette)"
                );
        }

    } // namespace

    bool can_convert_pixel_lossy(pixel_format src, pixel_format dst) noexcept {
        return src != pixel_format::palette_u8 && dst != pixel_format::palette_u8
            && bytes_per_pixel_for_format(src) != 0 && bytes_per_pixel_for_format(dst) != 0;
    }

    namespace detail {

        void convert_pixel_row_lossy(pixel_format dst_fmt, void *dst, pixel_format src_fmt, const void *src, int count) {
            assert(can_convert_pixel_lossy(src_fmt, dst_fmt));
            if (count <= 0)
                return;

            if (src_fmt == dst_fmt) {
                std::memcpy(dst, src, static_cast<std::size_t>(count) * bytes_per_pixel_for_format(src_fmt));
            } else if (has_float_channels(src_fmt) || has_float_channels(dst_fmt)) {
                convert_row_through_hub<px_rgba_f32>(dst_fmt, dst, src_fmt, src, count);
            } else {
                convert_row_through_hub<px_rgba8888>(dst_fmt, dst, src_fmt, src, count);
            }
        }

    } // namespace detail

    void converting_blit_lossy(any_bitmap_view dst, any_bitmap_view src) {
        detail::validate_blit_size(dst.size(), src.size(), "converting_blit_lossy");
        validate_lossy_formats(src.format(), dst.format());
        if (dst.width() <= 0 || dst.height() <= 0)
            return;

        for (int y = 0; y < dst.height(); ++y)
            detail::convert_pixel_row_lossy(dst.format(), dst.line(y), src.format(), src.line(y), dst.width());
    }

    void converting_blit_lossy(any_bitmap_view dst, any_bitmap_view src, rect_i src_rect, vec2i dst_pos) {
        validate_lossy_formats(src.format(), dst.format());
        const detail::blit_rects rects = detail::resolve_blit_rects(dst.rect(), src.rect(), src_rect, dst_pos);
        if (rects.dst.width() <= 0 || rects.dst.height() <= 0)
            return;

        const int dst_bpp = bytes_per_pixel_for_format(dst.format());
        const int src_bpp = bytes_per_pixel_for_format(src.format());
        for (int y = 0; y < rects.dst.height(); ++y) {
            auto *dst_row = static_cast<std::byte *>(dst.line(rects.dst.top() + y));
            const auto *src_row = static_cast<const std::byte *>(src.line(rects.src.top() + y));
            detail::convert_pixel_row_lossy(
                dst.format(), dst_row + rects.dst.left() * dst_bpp,
                src.format(), src_row + rects.src.left() * src_bpp, rects.dst.width()
            );
        }
    }

} // namespace alia
