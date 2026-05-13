#ifndef ALIA_GFX_BACKEND_D3D9_OPS_HPP
#define ALIA_GFX_BACKEND_D3D9_OPS_HPP

#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>

#include <cstddef>
#include <memory>
#include <span>

#include "../graphics_backend_interface.hpp"

namespace alia {

    // ── Concrete device/texture/swapchain structs ─────────────────────────

    struct d3d9_device : device_handle {
        IDirect3D9 *d3d = nullptr;
        IDirect3DDevice9 *device = nullptr;
        HWND dummy = nullptr;
    };

    struct d3d9_texture : texture_handle {
        IDirect3DDevice9 *device = nullptr; // non-owning
        IDirect3DTexture9 *texture = nullptr;
        int width = 0;
        int height = 0;
        int mip_levels = 1;
        bool autogen = false;
        pixel_format fmt = pixel_format::bgra8888;
        texture_role role = texture_role::color;
        sampler_state sampler = {};
    };

    struct d3d9_swapchain : swapchain_handle {
        IDirect3DDevice9 *device = nullptr; // non-owning
        IDirect3DSwapChain9 *swap_chain = nullptr;
        HWND hwnd = nullptr;
        vec2i size = {};
    };

    // ── Cast helpers ──────────────────────────────────────────────────────

    inline d3d9_device *as_d3d9_device(device_handle *h) {
        return static_cast<d3d9_device *>(h);
    }
    inline const d3d9_device *as_d3d9_device(const device_handle *h) {
        return static_cast<const d3d9_device *>(h);
    }
    inline d3d9_texture *as_d3d9_texture(texture_handle *h) {
        return static_cast<d3d9_texture *>(h);
    }
    inline const d3d9_texture *as_d3d9_texture(const texture_handle *h) {
        return static_cast<const d3d9_texture *>(h);
    }
    inline d3d9_swapchain *as_d3d9_swapchain(swapchain_handle *h) {
        return static_cast<d3d9_swapchain *>(h);
    }

    // ── Helper ────────────────────────────────────────────────────────────

    inline DWORD to_d3d_color(color c) {
        auto clamp = [](float v) -> BYTE {
            return static_cast<BYTE>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
        };
        return D3DCOLOR_RGBA(clamp(c.r), clamp(c.g), clamp(c.b), clamp(c.a));
    }

    // ── Device ops ────────────────────────────────────────────────────────

    d3d9_device *d3d9_create_device();
    void d3d9_destroy_device(device_handle *h);

    // ── Texture ops ───────────────────────────────────────────────────────

    texture_handle *d3d9_create_texture(device_handle *dev, pixel_format fmt, vec2i size, int mip_levels, texture_role role);
    void d3d9_destroy_texture(texture_handle *h);
    pixel_format d3d9_texture_format(const texture_handle *h);
    int d3d9_texture_width(const texture_handle *h);
    int d3d9_texture_height(const texture_handle *h);
    int d3d9_texture_mip_levels(const texture_handle *h);
    sampler_state d3d9_texture_sampler(const texture_handle *h);
    void d3d9_texture_set_sampler(texture_handle *h, const sampler_state &s);
    bool d3d9_texture_lock(texture_handle *h, rect_i region, int level, texture_lock_info &out);
    void d3d9_texture_unlock(texture_handle *h, const texture_lock_info &info, bool wrote);
    void d3d9_texture_generate_mipmaps(texture_handle *h);
    texture_handle *d3d9_texture_clone(const texture_handle *h);

    // ── Swapchain ops ─────────────────────────────────────────────────────

    swapchain_handle *d3d9_create_swapchain(device_handle *dev, void *native_handle, vec2i size);
    void d3d9_destroy_swapchain(swapchain_handle *h);
    void d3d9_swapchain_clear(swapchain_handle *h, color c);
    void d3d9_swapchain_present(swapchain_handle *h);
    void d3d9_swapchain_on_resize(swapchain_handle *h, vec2i new_size);

    // ── Draw ops ──────────────────────────────────────────────────────────

    void d3d9_draw_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::type_index vtx_type, std::span<const vertex_element> elements
    );
    void d3d9_draw_indexed_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::span<const uint32_t> indices,
        std::type_index vtx_type, std::span<const vertex_element> elements
    );
    void d3d9_draw_textured_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        texture_handle *tex
    );
    void d3d9_draw_alpha_masked_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        texture_handle *tex
    );
    void d3d9_draw_textured_indexed_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::span<const uint32_t> indices,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        texture_handle *tex
    );

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
#endif // ALIA_GFX_BACKEND_D3D9_OPS_HPP
