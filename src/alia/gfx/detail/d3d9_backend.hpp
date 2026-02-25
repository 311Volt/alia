#ifndef ALIA_GFX_DETAIL_D3D9_BACKEND_HPP
#define ALIA_GFX_DETAIL_D3D9_BACKEND_HPP

#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <algorithm>
#include <cstdint>
#include <span>

#include "../gfx_device.hpp"

namespace alia {

// ── Helpers (used by backend_d3d9.cpp and primitives_d3d9.cpp) ────────

inline DWORD to_d3d_color(color c) {
    auto clamp = [](float v) -> BYTE {
        return static_cast<BYTE>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return D3DCOLOR_RGBA(clamp(c.r), clamp(c.g), clamp(c.b), clamp(c.a));
}

struct d3d9_vertex {
    float x, y, z;
    DWORD color;
    static constexpr DWORD FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE;
};

// ── D3D9 device impl ──────────────────────────────────────────────────

struct d3d9_device_impl : gfx_device_impl {
    IDirect3D9*       d3d    = nullptr;
    IDirect3DDevice9* device = nullptr;
    HWND              dummy  = nullptr;

    ~d3d9_device_impl() override;
    const char* backend_name() const noexcept override { return "d3d9"; }
};

// ── D3D9 swapchain impl ───────────────────────────────────────────────

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

    void draw_triangle(colored_vertex v0, colored_vertex v1, colored_vertex v2) override;
    void draw_triangles(std::span<const colored_vertex> vertices) override;
    void draw_triangle_strip(std::span<const colored_vertex> vertices) override;
    void draw_triangle_fan(std::span<const colored_vertex> vertices) override;
    void draw_triangles(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices) override;
    void draw_triangle_strip(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices) override;
    void draw_triangle_fan(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices) override;
};

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9

#endif // ALIA_GFX_DETAIL_D3D9_BACKEND_HPP
