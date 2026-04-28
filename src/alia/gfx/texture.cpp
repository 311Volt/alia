#include "texture.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace alia {

// ── texture constructors ──────────────────────────────────────────────

texture::texture(gfx_device& device, pixel_format fmt, vec2i size, int mip_levels)
    : impl_(device.impl()->create_texture(fmt, size, mip_levels))
{}

texture::texture(gfx_device& device, const any_bitmap_view& src, int mip_levels)
    : impl_(device.impl()->create_texture(
          src.format(), {src.width(), src.height()}, mip_levels))
{
    if (!impl_) return;

    const int bpp = bytes_per_pixel_for_format(src.format());
    texture_lock_info info{};
    const rect_i full{{0, 0}, {src.width(), src.height()}};
    if (!impl_->lock(full, 0, info)) return;

    for (int y = 0; y < src.height(); ++y) {
        std::memcpy(
            info.data + y * info.stride_bytes,
            static_cast<const std::byte*>(src.line(y)),
            static_cast<std::size_t>(src.width()) * bpp);
    }
    impl_->unlock(info, true);
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
