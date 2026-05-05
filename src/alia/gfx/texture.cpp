#include "texture.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace alia {

namespace {

struct rgba_u8 {
    std::byte r, g, b, a;
};

bool read_rgba_u8(rgba_u8& out, pixel_format fmt, const std::byte* src) {
    switch (fmt) {
        case pixel_format::rgb888:
            out = {src[0], src[1], src[2], std::byte{0xff}};
            return true;
        case pixel_format::rgba8888:
            out = {src[0], src[1], src[2], src[3]};
            return true;
        case pixel_format::bgr888:
            out = {src[2], src[1], src[0], std::byte{0xff}};
            return true;
        case pixel_format::bgra8888:
            out = {src[2], src[1], src[0], src[3]};
            return true;
        case pixel_format::gray_u8:
            out = {src[0], src[0], src[0], std::byte{0xff}};
            return true;
        case pixel_format::rgb565: {
            const auto raw =
                std::to_integer<unsigned>(src[0])
                | (std::to_integer<unsigned>(src[1]) << 8);
            const auto r = (raw >> 11) & 0x1f;
            const auto g = (raw >> 5) & 0x3f;
            const auto b = raw & 0x1f;
            out = {
                std::byte((r * 255 + 15) / 31),
                std::byte((g * 255 + 31) / 63),
                std::byte((b * 255 + 15) / 31),
                std::byte{0xff}};
            return true;
        }
        default:
            return false;
    }
}

bool write_rgba_u8(pixel_format fmt, std::byte* dst, rgba_u8 in) {
    switch (fmt) {
        case pixel_format::rgb888:
            dst[0] = in.r;
            dst[1] = in.g;
            dst[2] = in.b;
            return true;
        case pixel_format::rgba8888:
            dst[0] = in.r;
            dst[1] = in.g;
            dst[2] = in.b;
            dst[3] = in.a;
            return true;
        case pixel_format::bgr888:
            dst[0] = in.b;
            dst[1] = in.g;
            dst[2] = in.r;
            return true;
        case pixel_format::bgra8888:
            dst[0] = in.b;
            dst[1] = in.g;
            dst[2] = in.r;
            dst[3] = in.a;
            return true;
        default:
            return false;
    }
}

bool upload_bitmap_view(texture_impl& dst, const any_bitmap_view& src) {
    const pixel_format src_fmt = src.format();
    const pixel_format dst_fmt = dst.format();
    const int src_bpp = bytes_per_pixel_for_format(src_fmt);
    const int dst_bpp = bytes_per_pixel_for_format(dst_fmt);
    if (src_bpp == 0 || dst_bpp == 0)
        return false;

    texture_lock_info info{};
    const rect_i full{{0, 0}, {src.width(), src.height()}};
    if (!dst.lock(full, 0, info))
        return false;

    bool uploaded = true;
    if (src_fmt == dst_fmt) {
        for (int y = 0; y < src.height(); ++y) {
            std::memcpy(
                info.data + y * info.stride_bytes,
                static_cast<const std::byte*>(src.line(y)),
                static_cast<std::size_t>(src.width()) * src_bpp);
        }
    } else {
        for (int y = 0; y < src.height() && uploaded; ++y) {
            const auto* src_row = static_cast<const std::byte*>(src.line(y));
            auto* dst_row = info.data + y * info.stride_bytes;
            for (int x = 0; x < src.width(); ++x) {
                rgba_u8 px{};
                if (!read_rgba_u8(px, src_fmt, src_row + x * src_bpp)
                    || !write_rgba_u8(dst_fmt, dst_row + x * dst_bpp, px))
                {
                    uploaded = false;
                    break;
                }
            }
        }
    }

    dst.unlock(info, uploaded);
    return uploaded;
}

} // namespace

// ── texture constructors ──────────────────────────────────────────────

texture::texture(gfx_device& device, pixel_format fmt, vec2i size, int mip_levels)
    : impl_(device.impl()->create_texture(fmt, size, mip_levels))
{}

texture::texture(gfx_device& device, const any_bitmap_view& src, int mip_levels)
    : impl_(device.impl()->create_texture(
          src.format(), {src.width(), src.height()}, mip_levels))
{
    if (!impl_) return;

    if (!upload_bitmap_view(*impl_, src))
        impl_.reset();
}

texture::texture(gfx_device& device, const bitmap& src, int mip_levels)
    : texture(device, src.view(), mip_levels)
{}

// ── Accessors ────────────────────────────────────────────────────────

pixel_format texture::format()     const noexcept { return impl_->format(); }
int          texture::width()      const noexcept { return impl_->width(); }
int          texture::height()     const noexcept { return impl_->height(); }
int          texture::mip_levels() const noexcept { return impl_->mip_levels(); }

void texture::set_sampler(const sampler_state& s) { impl_->set_sampler(s); }
sampler_state texture::sampler() const noexcept    { return impl_->sampler(); }

// ── lock_impl ────────────────────────────────────────────────────────

std::unique_ptr<detail::texture_lock_state>
texture::lock_impl(const std::optional<rect_i>& region, int level, pixel_format expected_fmt)
{
    if (!impl_ || impl_->format() != expected_fmt)
        return nullptr;

    const int lw = std::max(1, impl_->width()  >> level);
    const int lh = std::max(1, impl_->height() >> level);
    const rect_i r = region.value_or(rect_i{{0, 0}, {lw, lh}});

    texture_lock_info info{};
    if (!impl_->lock(r, level, info))
        return nullptr;

    auto state   = std::make_unique<detail::texture_lock_state>();
    state->tex   = impl_.get();
    state->info  = info;
    return state;
}

// ── Operations ───────────────────────────────────────────────────────

void texture::generate_mipmaps() {
    impl_->generate_mipmaps();
}

bitmap texture::download(int level) const {
    const int lw = std::max(1, impl_->width()  >> level);
    const int lh = std::max(1, impl_->height() >> level);
    const rect_i full{{0, 0}, {lw, lh}};

    texture_lock_info info{};
    if (!impl_->lock(full, level, info))
        throw std::runtime_error("texture::download: backend lock failed");

    const pixel_format fmt = impl_->format();
    const std::size_t total =
        static_cast<std::size_t>(info.stride_bytes) * lh;

    bitmap result(fmt, {lw, lh},
                  std::span<const std::byte>(info.data, total),
                  info.stride_bytes);

    impl_->unlock(info, false);  // read-only — skip GPU re-upload
    return result;
}

texture texture::clone() const {
    return texture(impl_->clone());
}

} // namespace alia
