#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"
#include <algorithm>
#include <cstring>
#include <memory>

namespace alia {

    static D3DFORMAT to_d3d_format(pixel_format fmt, texture_role role) noexcept {
        switch (fmt) {
        case pixel_format::bgra8888: return D3DFMT_A8R8G8B8;
        case pixel_format::rgba8888: return D3DFMT_A8B8G8R8;
        case pixel_format::bgr888:   return D3DFMT_R8G8B8;
        case pixel_format::rgb565:   return D3DFMT_R5G6B5;
        case pixel_format::gray_u8:  return role == texture_role::alpha_mask ? D3DFMT_A8 : D3DFMT_L8;
        case pixel_format::gray_f32: return D3DFMT_R32F;
        case pixel_format::rgba_f32: return D3DFMT_A32B32G32R32F;
        default:                     return D3DFMT_UNKNOWN;
        }
    }

    static bool d3d9_supports_texture_format(IDirect3DDevice9 *device, D3DFORMAT fmt, DWORD usage) {
        if (fmt == D3DFMT_UNKNOWN)
            return false;

        IDirect3D9 *d3d = nullptr;
        if (FAILED(device->GetDirect3D(&d3d)) || !d3d)
            return false;

        D3DDEVICE_CREATION_PARAMETERS params = {};
        D3DDISPLAYMODE display_mode = {};
        const bool supported =
            SUCCEEDED(device->GetCreationParameters(&params)) &&
            SUCCEEDED(d3d->GetAdapterDisplayMode(params.AdapterOrdinal, &display_mode)) &&
            SUCCEEDED(d3d->CheckDeviceFormat(
                params.AdapterOrdinal, params.DeviceType,
                display_mode.Format, usage, D3DRTYPE_TEXTURE, fmt
            ));

        d3d->Release();
        return supported;
    }

    static pixel_format choose_d3d9_texture_format(IDirect3DDevice9 *device, pixel_format requested, DWORD usage, texture_role role) {
        const D3DFORMAT requested_fmt = to_d3d_format(requested, role);
        if (d3d9_supports_texture_format(device, requested_fmt, usage))
            return requested;

        if (role == texture_role::alpha_mask)
            return requested;

        switch (requested) {
        case pixel_format::rgb888:
        case pixel_format::rgba8888:
        case pixel_format::bgr888:
        case pixel_format::bgra8888:
        case pixel_format::rgb565:
        case pixel_format::gray_u8:
            if (d3d9_supports_texture_format(device, D3DFMT_A8R8G8B8, usage))
                return pixel_format::bgra8888;
            break;
        default: break;
        }

        return requested;
    }

    texture_handle *d3d9_create_texture(device_handle *dev_h, pixel_format fmt, vec2i size, int mip_levels, texture_role role) {
        auto *dev = as_d3d9_device(dev_h);
        const bool autogen = (mip_levels != 1);
        const DWORD usage = autogen ? D3DUSAGE_AUTOGENMIPMAP : 0u;
        const UINT mips = autogen ? 0u : 1u;
        const pixel_format actual_fmt = choose_d3d9_texture_format(dev->device, fmt, usage, role);
        const D3DFORMAT d3dfmt = to_d3d_format(actual_fmt, role);
        if (!d3d9_supports_texture_format(dev->device, d3dfmt, usage))
            return nullptr;

        IDirect3DTexture9 *tex = nullptr;
        if (FAILED(dev->device->CreateTexture(
                static_cast<UINT>(size.x), static_cast<UINT>(size.y),
                mips, usage, d3dfmt, D3DPOOL_MANAGED, &tex, nullptr
            )) || !tex)
            return nullptr;

        auto *t = new d3d9_texture;
        t->device = dev->device;
        t->texture = tex;
        t->fmt = actual_fmt;
        t->width = size.x;
        t->height = size.y;
        t->autogen = autogen;
        t->mip_levels = static_cast<int>(tex->GetLevelCount());
        t->role = role;
        return t;
    }

    void d3d9_destroy_texture(texture_handle *h) {
        auto *t = as_d3d9_texture(h);
        if (t->texture)
            t->texture->Release();
        delete t;
    }

    pixel_format d3d9_texture_format(const texture_handle *h) {
        return as_d3d9_texture(h)->fmt;
    }
    int d3d9_texture_width(const texture_handle *h) {
        return as_d3d9_texture(h)->width;
    }
    int d3d9_texture_height(const texture_handle *h) {
        return as_d3d9_texture(h)->height;
    }
    int d3d9_texture_mip_levels(const texture_handle *h) {
        return as_d3d9_texture(h)->mip_levels;
    }
    sampler_state d3d9_texture_sampler(const texture_handle *h) {
        return as_d3d9_texture(h)->sampler;
    }
    void d3d9_texture_set_sampler(texture_handle *h, const sampler_state &s) {
        as_d3d9_texture(h)->sampler = s;
    }

    bool d3d9_texture_lock(texture_handle *h, rect_i region, int level, texture_lock_mode mode, texture_lock_info &out) {
        auto *t = as_d3d9_texture(h);
        if (level < 0 || level >= t->mip_levels)
            return false;
        if (t->autogen && level > 0)
            return false;

        const int lw = std::max(1, t->width >> level);
        const int lh = std::max(1, t->height >> level);
        const rect_i bounds{{0, 0}, {lw, lh}};
        const rect_i r = bounds.intersection_with(region);
        if (r.width() <= 0 || r.height() <= 0)
            return false;

        // D3DLOCK_DISCARD requires D3DUSAGE_DYNAMIC, which managed-pool textures
        // don't have, so write_only doesn't earn any special flag here.
        const DWORD lock_flags = (mode == texture_lock_mode::read_only) ? D3DLOCK_READONLY : 0u;

        RECT dr = {r.left(), r.top(), r.right(), r.bottom()};
        D3DLOCKED_RECT lr = {};
        if (FAILED(t->texture->LockRect(static_cast<UINT>(level), &lr, &dr, lock_flags)))
            return false;

        out.data = static_cast<std::byte *>(lr.pBits);
        out.stride_bytes = static_cast<int>(lr.Pitch);
        out.origin = r.p1;
        out.extent = r.size();
        out.level = level;
        return true;
    }

    void d3d9_texture_unlock(texture_handle *h, const texture_lock_info &info, bool /*wrote*/) {
        as_d3d9_texture(h)->texture->UnlockRect(static_cast<UINT>(info.level));
    }

    void d3d9_texture_generate_mipmaps(texture_handle *h) {
        as_d3d9_texture(h)->texture->GenerateMipSubLevels();
    }

    texture_handle *d3d9_texture_clone(const texture_handle *h) {
        const auto *src = as_d3d9_texture(h);
        const D3DFORMAT d3dfmt = to_d3d_format(src->fmt, src->role);
        if (d3dfmt == D3DFMT_UNKNOWN)
            return nullptr;

        IDirect3DTexture9 *dst_tex = nullptr;
        if (FAILED(src->device->CreateTexture(
                static_cast<UINT>(src->width),
                static_cast<UINT>(src->height),
                src->autogen ? 0u : static_cast<UINT>(src->mip_levels),
                src->autogen ? D3DUSAGE_AUTOGENMIPMAP : 0u,
                d3dfmt, D3DPOOL_MANAGED, &dst_tex, nullptr
            )) || !dst_tex)
            return nullptr;

        const int mips_to_copy = src->autogen ? 1 : src->mip_levels;
        const int bpp = bytes_per_pixel_for_format(src->fmt);

        for (int lv = 0; lv < mips_to_copy; ++lv) {
            const int lw = std::max(1, src->width >> lv);
            const int lh = std::max(1, src->height >> lv);

            D3DLOCKED_RECT src_lr = {}, dst_lr = {};
            if (FAILED(src->texture->LockRect(static_cast<UINT>(lv), &src_lr, nullptr, D3DLOCK_READONLY))) {
                dst_tex->Release();
                return nullptr;
            }
            if (FAILED(dst_tex->LockRect(static_cast<UINT>(lv), &dst_lr, nullptr, 0))) {
                src->texture->UnlockRect(static_cast<UINT>(lv));
                dst_tex->Release();
                return nullptr;
            }

            const int row_bytes = lw * bpp;
            for (int y = 0; y < lh; ++y) {
                std::memcpy(
                    static_cast<std::byte *>(dst_lr.pBits) + y * dst_lr.Pitch,
                    static_cast<const std::byte *>(src_lr.pBits) + y * src_lr.Pitch,
                    row_bytes
                );
            }

            dst_tex->UnlockRect(static_cast<UINT>(lv));
            src->texture->UnlockRect(static_cast<UINT>(lv));
        }

        if (src->autogen)
            dst_tex->GenerateMipSubLevels();

        auto *t = new d3d9_texture;
        t->device = src->device;
        t->texture = dst_tex;
        t->fmt = src->fmt;
        t->width = src->width;
        t->height = src->height;
        t->mip_levels = src->mip_levels;
        t->autogen = src->autogen;
        t->sampler = src->sampler;
        t->role = src->role;
        return t;
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
