#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"
#include <algorithm>
#include <cstring>
#include <memory>

namespace alia {

    namespace {

        template <typename T>
        class com_ptr {
        public:
            com_ptr() = default;
            ~com_ptr() {
                reset();
            }
            com_ptr(com_ptr &&other) noexcept
                : ptr_(other.ptr_) {
                other.ptr_ = nullptr;
            }
            com_ptr &operator=(com_ptr &&other) noexcept {
                if (this != &other) {
                    reset();
                    ptr_ = other.ptr_;
                    other.ptr_ = nullptr;
                }
                return *this;
            }
            com_ptr(const com_ptr &) = delete;
            com_ptr &operator=(const com_ptr &) = delete;

            T **put() noexcept {
                reset();
                return &ptr_;
            }
            T *get() const noexcept {
                return ptr_;
            }
            T *detach() noexcept {
                T *p = ptr_;
                ptr_ = nullptr;
                return p;
            }
            void reset(T *p = nullptr) noexcept {
                if (ptr_)
                    ptr_->Release();
                ptr_ = p;
            }

        private:
            T *ptr_ = nullptr;
        };

        D3DFORMAT to_d3d_format(pixel_format fmt, texture_role role) noexcept {
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

        int bytes_per_d3d_format(D3DFORMAT fmt) noexcept {
            switch (fmt) {
            case D3DFMT_A8R8G8B8:
            case D3DFMT_A8B8G8R8:
            case D3DFMT_X8R8G8B8:
            case D3DFMT_R32F:
                return 4;
            case D3DFMT_R8G8B8:
                return 3;
            case D3DFMT_R5G6B5:
                return 2;
            case D3DFMT_A8:
            case D3DFMT_L8:
                return 1;
            case D3DFMT_A32B32G32R32F:
                return 16;
            default:
                return 0;
            }
        }

        bool d3d9_supports_texture_format(IDirect3DDevice9 *device, D3DFORMAT fmt, DWORD usage) {
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

        pixel_format choose_d3d9_texture_format(IDirect3DDevice9 *device, pixel_format requested, DWORD usage, texture_role role) {
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

        bool uses_autogen_mips(int mip_levels) noexcept {
            return mip_levels == 0;
        }

        DWORD d3d_texture_usage_flags(bool autogen, texture_usage usage) noexcept {
            DWORD flags = autogen ? D3DUSAGE_AUTOGENMIPMAP : 0u;
            if (usage == texture_usage::render_target)
                flags |= D3DUSAGE_RENDERTARGET;
            return flags;
        }

        D3DPOOL d3d_texture_pool(texture_usage usage) noexcept {
            return usage == texture_usage::render_target ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
        }

        vec2i level_size(const d3d9_texture &t, int level) noexcept {
            return {
                std::max(1, t.width >> level),
                std::max(1, t.height >> level)
            };
        }

        RECT to_rect(rect_i r) noexcept {
            return {r.left(), r.top(), r.right(), r.bottom()};
        }

        bool create_sysmem_surface_for_level(const d3d9_texture &t, int level, IDirect3DSurface9 **out) {
            const vec2i size = level_size(t, level);
            const D3DFORMAT fmt = to_d3d_format(t.fmt, t.role);
            return SUCCEEDED(t.device->CreateOffscreenPlainSurface(
                static_cast<UINT>(size.x),
                static_cast<UINT>(size.y),
                fmt,
                D3DPOOL_SYSTEMMEM,
                out,
                nullptr
            ));
        }

        bool copy_render_target_level_to_sysmem(const d3d9_texture &t, int level, IDirect3DSurface9 *dst) {
            com_ptr<IDirect3DSurface9> src;
            if (FAILED(t.texture->GetSurfaceLevel(static_cast<UINT>(level), src.put())))
                return false;
            return SUCCEEDED(t.device->GetRenderTargetData(src.get(), dst));
        }

        bool update_render_target_level_from_sysmem(d3d9_texture &t, int level, IDirect3DSurface9 *src, const RECT *src_rect, const POINT *dst_point) {
            com_ptr<IDirect3DSurface9> dst;
            if (FAILED(t.texture->GetSurfaceLevel(static_cast<UINT>(level), dst.put())))
                return false;
            return SUCCEEDED(t.device->UpdateSurface(src, src_rect, dst.get(), dst_point));
        }

        void fill_texture_record(
            d3d9_texture &t,
            IDirect3DDevice9 *device,
            IDirect3DTexture9 *texture,
            pixel_format fmt,
            vec2i size,
            bool autogen,
            texture_role role,
            texture_usage usage
        ) {
            t.device = device;
            t.texture = texture;
            t.fmt = fmt;
            t.width = size.x;
            t.height = size.y;
            t.autogen = autogen;
            t.mip_levels = static_cast<int>(texture->GetLevelCount());
            t.role = role;
            t.usage = usage;
        }

    } // namespace

    texture_handle *d3d9_create_texture(
        device_handle *dev_h,
        pixel_format fmt,
        vec2i size,
        int mip_levels,
        texture_role role,
        texture_usage usage
    ) {
        auto *dev = as_d3d9_device(dev_h);
        const bool autogen = uses_autogen_mips(mip_levels);
        const DWORD d3d_usage = d3d_texture_usage_flags(autogen, usage);
        const UINT mips = autogen ? 0u : static_cast<UINT>(mip_levels);
        const D3DPOOL pool = d3d_texture_pool(usage);
        const pixel_format actual_fmt = choose_d3d9_texture_format(dev->device, fmt, d3d_usage, role);
        const D3DFORMAT d3dfmt = to_d3d_format(actual_fmt, role);
        if (!d3d9_supports_texture_format(dev->device, d3dfmt, d3d_usage))
            return nullptr;

        IDirect3DTexture9 *tex = nullptr;
        if (FAILED(dev->device->CreateTexture(
                static_cast<UINT>(size.x), static_cast<UINT>(size.y),
                mips, d3d_usage, d3dfmt, pool, &tex, nullptr
            )) || !tex)
            return nullptr;

        auto *t = new d3d9_texture;
        fill_texture_record(*t, dev->device, tex, actual_fmt, size, autogen, role, usage);
        return t;
    }

    void d3d9_destroy_texture(texture_handle *h) {
        auto *t = as_d3d9_texture(h);
        if (t->lock_surface)
            t->lock_surface->Release();
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

        const vec2i ls = level_size(*t, level);
        const rect_i bounds{{0, 0}, ls};
        const rect_i r = bounds.intersection_with(region);
        if (r.width() <= 0 || r.height() <= 0)
            return false;

        const DWORD lock_flags = (mode == texture_lock_mode::read_only) ? D3DLOCK_READONLY : 0u;
        const RECT dr = to_rect(r);
        D3DLOCKED_RECT lr = {};

        if (t->usage == texture_usage::sampling_only) {
            if (FAILED(t->texture->LockRect(static_cast<UINT>(level), &lr, &dr, lock_flags)))
                return false;
        } else {
            if (t->lock_surface)
                return false;
            com_ptr<IDirect3DSurface9> stage;
            if (!create_sysmem_surface_for_level(*t, level, stage.put()))
                return false;
            if (mode != texture_lock_mode::write_only && !copy_render_target_level_to_sysmem(*t, level, stage.get()))
                return false;
            if (FAILED(stage.get()->LockRect(&lr, &dr, lock_flags)))
                return false;
            t->lock_surface = stage.detach();
        }

        out.data = static_cast<std::byte *>(lr.pBits);
        out.stride_bytes = static_cast<int>(lr.Pitch);
        out.origin = r.p1;
        out.extent = r.size();
        out.level = level;
        return true;
    }

    void d3d9_texture_unlock(texture_handle *h, const texture_lock_info &info, bool wrote) {
        auto *t = as_d3d9_texture(h);
        if (t->usage == texture_usage::sampling_only) {
            t->texture->UnlockRect(static_cast<UINT>(info.level));
            return;
        }

        if (!t->lock_surface)
            return;

        t->lock_surface->UnlockRect();
        if (wrote) {
            const rect_i updated = rect_i::pos_size(info.origin, info.extent);
            const RECT src_rect = to_rect(updated);
            const POINT dst_point{info.origin.x, info.origin.y};
            update_render_target_level_from_sysmem(*t, info.level, t->lock_surface, &src_rect, &dst_point);
        }

        t->lock_surface->Release();
        t->lock_surface = nullptr;
    }

    void d3d9_texture_generate_mipmaps(texture_handle *h) {
        as_d3d9_texture(h)->texture->GenerateMipSubLevels();
    }

    texture_handle *d3d9_texture_clone(const texture_handle *h) {
        const auto *src = as_d3d9_texture(h);
        const D3DFORMAT d3dfmt = to_d3d_format(src->fmt, src->role);
        if (d3dfmt == D3DFMT_UNKNOWN)
            return nullptr;

        const DWORD usage_flags = d3d_texture_usage_flags(src->autogen, src->usage);
        const D3DPOOL pool = d3d_texture_pool(src->usage);
        IDirect3DTexture9 *dst_tex = nullptr;
        if (FAILED(src->device->CreateTexture(
                static_cast<UINT>(src->width),
                static_cast<UINT>(src->height),
                src->autogen ? 0u : static_cast<UINT>(src->mip_levels),
                usage_flags,
                d3dfmt, pool, &dst_tex, nullptr
            )) || !dst_tex)
            return nullptr;

        const int mips_to_copy = src->autogen ? 1 : src->mip_levels;
        const int bpp = bytes_per_pixel_for_format(src->fmt);

        for (int lv = 0; lv < mips_to_copy; ++lv) {
            const vec2i ls = level_size(*src, lv);

            if (src->usage == texture_usage::render_target) {
                com_ptr<IDirect3DSurface9> stage;
                if (!create_sysmem_surface_for_level(*src, lv, stage.put()) ||
                    !copy_render_target_level_to_sysmem(*src, lv, stage.get())) {
                    dst_tex->Release();
                    return nullptr;
                }

                d3d9_texture dst_record;
                fill_texture_record(
                    dst_record, src->device, dst_tex, src->fmt, {src->width, src->height},
                    src->autogen, src->role, src->usage
                );
                if (!update_render_target_level_from_sysmem(dst_record, lv, stage.get(), nullptr, nullptr)) {
                    dst_tex->Release();
                    return nullptr;
                }
                continue;
            }

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

            const int row_bytes = ls.x * bpp;
            for (int y = 0; y < ls.y; ++y) {
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
        fill_texture_record(
            *t, src->device, dst_tex, src->fmt, {src->width, src->height},
            src->autogen, src->role, src->usage
        );
        t->sampler = src->sampler;
        return t;
    }

    bool d3d9_copy_render_target_to_texture(
        device_handle *dev_h,
        texture_handle *dst_h,
        rect_i src_rect,
        vec2i,
        vec2i dst_pos,
        int dst_level
    ) {
        auto *device = as_d3d9_device(dev_h)->device;
        auto *dst = as_d3d9_texture(dst_h);

        com_ptr<IDirect3DSurface9> source;
        if (FAILED(device->GetRenderTarget(0, source.put())))
            return false;

        D3DSURFACE_DESC source_desc = {};
        if (FAILED(source.get()->GetDesc(&source_desc)))
            return false;

        com_ptr<IDirect3DSurface9> stage;
        if (FAILED(device->CreateOffscreenPlainSurface(
                source_desc.Width,
                source_desc.Height,
                source_desc.Format,
                D3DPOOL_SYSTEMMEM,
                stage.put(),
                nullptr
            )))
            return false;

        if (FAILED(device->GetRenderTargetData(source.get(), stage.get())))
            return false;

        const RECT src = to_rect(src_rect);
        if (dst->usage == texture_usage::render_target) {
            const POINT dst_point{dst_pos.x, dst_pos.y};
            return update_render_target_level_from_sysmem(*dst, dst_level, stage.get(), &src, &dst_point);
        }

        const int src_bpp = bytes_per_d3d_format(source_desc.Format);
        const int dst_bpp = bytes_per_pixel_for_format(dst->fmt);
        if (src_bpp <= 0 || dst_bpp <= 0 || src_bpp != dst_bpp)
            return false;

        D3DLOCKED_RECT src_lr = {}, dst_lr = {};
        if (FAILED(stage.get()->LockRect(&src_lr, &src, D3DLOCK_READONLY)))
            return false;

        const rect_i dst_rect = rect_i::pos_size(dst_pos, src_rect.size());
        const RECT dst_lock = to_rect(dst_rect);
        if (FAILED(dst->texture->LockRect(static_cast<UINT>(dst_level), &dst_lr, &dst_lock, 0))) {
            stage.get()->UnlockRect();
            return false;
        }

        const int row_bytes = src_rect.width() * dst_bpp;
        for (int y = 0; y < src_rect.height(); ++y) {
            std::memcpy(
                static_cast<std::byte *>(dst_lr.pBits) + y * dst_lr.Pitch,
                static_cast<const std::byte *>(src_lr.pBits) + y * src_lr.Pitch,
                row_bytes
            );
        }

        dst->texture->UnlockRect(static_cast<UINT>(dst_level));
        stage.get()->UnlockRect();
        return true;
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
