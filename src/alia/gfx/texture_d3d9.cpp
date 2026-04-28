#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "detail/d3d9_backend.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>

namespace alia {

// ── Pixel format table ─────────────────────────────────────────────────
//
// D3DFMT_A8R8G8B8 stores channels in memory as B, G, R, A  → bgra8888.
// D3DFMT_A8B8G8R8 stores channels in memory as R, G, B, A  → rgba8888.
// D3DFMT_R8G8B8   stores channels in memory as B, G, R     → bgr888.
// D3DFMT_A8B8G8R8 is optional on some hardware; check caps if needed.

static D3DFORMAT to_d3d_format(pixel_format fmt) noexcept {
    switch (fmt) {
        case pixel_format::bgra8888: return D3DFMT_A8R8G8B8;
        case pixel_format::rgba8888: return D3DFMT_A8B8G8R8;
        case pixel_format::bgr888:   return D3DFMT_R8G8B8;
        case pixel_format::rgb565:   return D3DFMT_R5G6B5;
        case pixel_format::gray_u8:  return D3DFMT_L8;
        case pixel_format::gray_f32: return D3DFMT_R32F;
        case pixel_format::rgba_f32: return D3DFMT_A32B32G32R32F;
        default:                     return D3DFMT_UNKNOWN;
    }
}

// ── d3d9_texture_impl ─────────────────────────────────────────────────

d3d9_texture_impl::~d3d9_texture_impl() {
    if (texture_) texture_->Release();
}

bool d3d9_texture_impl::lock(rect_i region, int level, texture_lock_info& out) {
    if (level < 0 || level >= mip_levels_) return false;
    // Autogen textures can only lock level 0.
    if (autogen_ && level > 0) return false;

    const int lw = std::max(1, width_  >> level);
    const int lh = std::max(1, height_ >> level);
    const rect_i bounds{{0, 0}, {lw, lh}};
    const rect_i r = bounds.intersection_with(region);
    if (r.width() <= 0 || r.height() <= 0) return false;

    RECT dr = {r.left(), r.top(), r.right(), r.bottom()};
    D3DLOCKED_RECT lr = {};
    if (FAILED(texture_->LockRect(static_cast<UINT>(level), &lr, &dr, 0)))
        return false;

    out.data         = static_cast<std::byte*>(lr.pBits);
    out.stride_bytes = static_cast<int>(lr.Pitch);
    out.origin       = r.p1;
    out.extent       = r.size();
    out.level        = level;
    return true;
}

void d3d9_texture_impl::unlock(const texture_lock_info& info, bool /*wrote*/) {
    // D3DPOOL_MANAGED handles dirty-region tracking internally.
    texture_->UnlockRect(static_cast<UINT>(info.level));
}

void d3d9_texture_impl::generate_mipmaps() {
    if (autogen_) texture_->GenerateMipSubLevels();
}

std::unique_ptr<texture_impl> d3d9_texture_impl::clone() const {
    const D3DFORMAT d3dfmt = to_d3d_format(fmt_);
    if (d3dfmt == D3DFMT_UNKNOWN) return nullptr;

    IDirect3DTexture9* dst = nullptr;
    if (FAILED(device_->CreateTexture(
            static_cast<UINT>(width_), static_cast<UINT>(height_),
            autogen_ ? 0u : static_cast<UINT>(mip_levels_),
            autogen_ ? D3DUSAGE_AUTOGENMIPMAP : 0u,
            d3dfmt, D3DPOOL_MANAGED, &dst, nullptr)) || !dst)
        return nullptr;

    // Copy level 0 (and any manually-managed levels for non-autogen textures).
    const int mips_to_copy = autogen_ ? 1 : mip_levels_;
    const int bpp = bytes_per_pixel_for_format(fmt_);

    for (int lv = 0; lv < mips_to_copy; ++lv) {
        const int lw = std::max(1, width_  >> lv);
        const int lh = std::max(1, height_ >> lv);

        D3DLOCKED_RECT src_lr = {}, dst_lr = {};
        if (FAILED(texture_->LockRect(static_cast<UINT>(lv), &src_lr, nullptr,
                                       D3DLOCK_READONLY))) {
            dst->Release();
            return nullptr;
        }
        if (FAILED(dst->LockRect(static_cast<UINT>(lv), &dst_lr, nullptr, 0))) {
            texture_->UnlockRect(static_cast<UINT>(lv));
            dst->Release();
            return nullptr;
        }

        const int row_bytes = lw * bpp;
        for (int y = 0; y < lh; ++y) {
            std::memcpy(
                static_cast<std::byte*>(dst_lr.pBits) + y * dst_lr.Pitch,
                static_cast<const std::byte*>(src_lr.pBits) + y * src_lr.Pitch,
                row_bytes);
        }

        dst->UnlockRect(static_cast<UINT>(lv));
        texture_->UnlockRect(static_cast<UINT>(lv));
    }

    if (autogen_) dst->GenerateMipSubLevels();

    auto t = std::make_unique<d3d9_texture_impl>();
    t->device_     = device_;
    t->texture_    = dst;
    t->fmt_        = fmt_;
    t->width_      = width_;
    t->height_     = height_;
    t->mip_levels_ = mip_levels_;
    t->autogen_    = autogen_;
    t->sampler_    = sampler_;
    return t;
}

// ── d3d9_device_impl::create_texture ──────────────────────────────────

std::unique_ptr<texture_impl>
d3d9_device_impl::create_texture(pixel_format fmt, vec2i size, int mip_levels) {
    const D3DFORMAT d3dfmt = to_d3d_format(fmt);
    if (d3dfmt == D3DFMT_UNKNOWN) return nullptr;

    // mip_levels == 1  → no mipmaps
    // mip_levels == 0  → full chain via D3DUSAGE_AUTOGENMIPMAP (driver generates)
    // mip_levels >  1  → full chain via D3DUSAGE_AUTOGENMIPMAP (count ignored;
    //                     D3D9 doesn't support partial explicit chains cleanly)
    const bool autogen = (mip_levels != 1);
    const DWORD usage  = autogen ? D3DUSAGE_AUTOGENMIPMAP : 0u;
    const UINT  mips   = autogen ? 0u : 1u;

    IDirect3DTexture9* tex = nullptr;
    if (FAILED(device->CreateTexture(
            static_cast<UINT>(size.x), static_cast<UINT>(size.y),
            mips, usage, d3dfmt, D3DPOOL_MANAGED, &tex, nullptr)) || !tex)
        return nullptr;

    auto t = std::make_unique<d3d9_texture_impl>();
    t->device_     = device;
    t->texture_    = tex;
    t->fmt_        = fmt;
    t->width_      = size.x;
    t->height_     = size.y;
    t->autogen_    = autogen;
    t->mip_levels_ = static_cast<int>(tex->GetLevelCount());
    return t;
}

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
