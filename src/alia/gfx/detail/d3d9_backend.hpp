#ifndef ALIA_GFX_DETAIL_D3D9_BACKEND_HPP
#define ALIA_GFX_DETAIL_D3D9_BACKEND_HPP

#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>

#include "../gfx_device.hpp"

namespace alia {

// ── Helpers ──────────────────────────────────────────────────────────

inline DWORD to_d3d_color(color c) {
    auto clamp = [](float v) -> BYTE {
        return static_cast<BYTE>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return D3DCOLOR_RGBA(clamp(c.r), clamp(c.g), clamp(c.b), clamp(c.a));
}

// ── D3D9 texture impl ───────────────────────────────────────────────

struct d3d9_texture_impl : texture_impl {
    IDirect3DDevice9*  device_     = nullptr;  // non-owning
    IDirect3DTexture9* texture_    = nullptr;
    int                width_      = 0;
    int                height_     = 0;
    int                mip_levels_ = 1;
    bool               autogen_    = false;   // created with D3DUSAGE_AUTOGENMIPMAP
    pixel_format       fmt_        = pixel_format::bgra8888;
    sampler_state      sampler_    = {};

    ~d3d9_texture_impl() override;

    pixel_format  format()     const noexcept override { return fmt_; }
    int           width()      const noexcept override { return width_; }
    int           height()     const noexcept override { return height_; }
    int           mip_levels() const noexcept override { return mip_levels_; }
    sampler_state sampler()    const noexcept override { return sampler_; }

    // Sampler state is stored and applied per-stage at bind time (D3D9 limitation).
    void set_sampler(const sampler_state& s) override { sampler_ = s; }

    bool lock(rect_i region, int level, texture_lock_info& out) override;
    void unlock(const texture_lock_info& info, bool wrote) override;
    void generate_mipmaps() override;
    std::unique_ptr<texture_impl> clone() const override;
};

// ── D3D9 device impl ────────────────────────────────────────────────

struct d3d9_device_impl : gfx_device_impl {
    IDirect3D9*       d3d    = nullptr;
    IDirect3DDevice9* device = nullptr;
    HWND              dummy  = nullptr;

    ~d3d9_device_impl() override;
    const char* backend_name() const noexcept override { return "d3d9"; }

    std::unique_ptr<texture_impl> create_texture(
        pixel_format fmt, vec2i size, int mip_levels) override;
};

// ── D3D9 swapchain impl ─────────────────────────────────────────────

struct d3d9_swapchain_impl : swapchain_impl {
    IDirect3DDevice9*    device     = nullptr;  // non-owning
    IDirect3DSwapChain9* swap_chain = nullptr;
    HWND                 hwnd       = nullptr;
    vec2i                size       = {};

    float transform_[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    float projection_[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    ~d3d9_swapchain_impl() override;

    void set_render_target_to_back_buffer();
    void setup_render_states();

    void set_transform(std::span<const float, 16> m) override;
    void get_transform(std::span<float, 16> m) const override;
    void set_projection(std::span<const float, 16> m) override;
    void get_projection(std::span<float, 16> m) const override;

    void clear(color c) override;
    void present() override;
    void on_resize(vec2i new_size) override;

    void draw_prim(prim_type type,
                   const void* vertices, int count, int stride,
                   std::type_index vtx_type,
                   std::span<const vertex_element> elements) override;

    void draw_indexed_prim(prim_type type,
                           const void* vertices, int count, int stride,
                           std::span<const uint32_t> indices,
                           std::type_index vtx_type,
                           std::span<const vertex_element> elements) override;

    void draw_textured_prim(prim_type type,
                            const void* vertices, int count, int stride,
                            std::type_index vtx_type,
                            std::span<const vertex_element> elements,
                            texture_impl* tex) override;

    void draw_textured_indexed_prim(prim_type type,
                                    const void* vertices, int count, int stride,
                                    std::span<const uint32_t> indices,
                                    std::type_index vtx_type,
                                    std::span<const vertex_element> elements,
                                    texture_impl* tex) override;
};

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9

#endif // ALIA_GFX_DETAIL_D3D9_BACKEND_HPP
