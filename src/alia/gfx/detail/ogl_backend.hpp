#ifndef ALIA_GFX_DETAIL_OGL_BACKEND_HPP
#define ALIA_GFX_DETAIL_OGL_BACKEND_HPP

#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include <GL/gl.h>
#include <algorithm>
#include <cstdint>
#include <span>

#include "../gfx_device.hpp"

namespace alia {

// ── OpenGL device impl ────────────────────────────────────────────────

struct ogl_device_impl : gfx_device_impl {
    void* ctx = nullptr;  // opaque context handle (HGLRC on Win32)

    ~ogl_device_impl() override;
    const char* backend_name() const noexcept override { return "opengl"; }
};

// ── OpenGL swapchain impl ─────────────────────────────────────────────

struct ogl_swapchain_impl : swapchain_impl {
    void*  native  = nullptr;  // native window handle (for destroy_surface)
    void*  surface = nullptr;  // opaque surface handle (HDC on Win32)
    void*  ctx     = nullptr;  // non-owning ref to device context
    vec2i  size    = {};

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

    ~ogl_swapchain_impl() override;

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

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL

#endif // ALIA_GFX_DETAIL_OGL_BACKEND_HPP
