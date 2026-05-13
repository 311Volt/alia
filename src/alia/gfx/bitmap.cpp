#include "bitmap.hpp"

namespace alia {

    namespace {

        template <pixel TPixel>
        [[nodiscard]] bitmap_view<TPixel> typed_view(any_bitmap_view view) noexcept {
            auto *data = view.height() > 0 ? static_cast<std::byte *>(view.line(0)) : nullptr;
            return bitmap_view<TPixel>(view.width(), view.height(), view.stride_bytes(), data);
        }

        template <pixel TSrcPixel, pixel TDstPixel>
        bool try_converting_blit(any_bitmap_view dst, any_bitmap_view src) {
            if constexpr (is_convert_pixel_allowed_v<TSrcPixel, TDstPixel>) {
                converting_blit(typed_view<TDstPixel>(dst), typed_view<TSrcPixel>(src));
                return true;
            } else {
                return false;
            }
        }

        template <pixel TSrcPixel, pixel TDstPixel>
        bool try_converting_blit(any_bitmap_view dst, any_bitmap_view src, rect_i src_rect, vec2i dst_pos) {
            if constexpr (is_convert_pixel_allowed_v<TSrcPixel, TDstPixel>) {
                converting_blit(typed_view<TDstPixel>(dst), typed_view<TSrcPixel>(src), src_rect, dst_pos);
                return true;
            } else {
                return false;
            }
        }

    } // namespace

    void converting_blit(any_bitmap_view dst, any_bitmap_view src) {
        detail::validate_blit_size(dst.size(), src.size(), "converting_blit");

        const bool converted = detail::visit_pixel_format(src.format(), [&]<pixel TSrcPixel>() {
            return detail::visit_pixel_format(dst.format(), [&]<pixel TDstPixel>() {
                return try_converting_blit<TSrcPixel, TDstPixel>(dst, src);
            });
        });

        if (!converted)
            throw std::invalid_argument("converting_blit: unsupported pixel format conversion");
    }

    void converting_blit(any_bitmap_view dst, any_bitmap_view src, rect_i src_rect, vec2i dst_pos) {
        const bool converted = detail::visit_pixel_format(src.format(), [&]<pixel TSrcPixel>() {
            return detail::visit_pixel_format(dst.format(), [&]<pixel TDstPixel>() {
                return try_converting_blit<TSrcPixel, TDstPixel>(dst, src, src_rect, dst_pos);
            });
        });

        if (!converted)
            throw std::invalid_argument("converting_blit: unsupported pixel format conversion");
    }

} // namespace alia
