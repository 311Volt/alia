#ifndef ALIA_GFX_DETAIL_OGL_BACKEND_HPP
#define ALIA_GFX_DETAIL_OGL_BACKEND_HPP

#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include <GL/gl.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "../gfx_device.hpp"

namespace alia {

// ── OpenGL texture impl ──────────────────────────────────────────────

struct ogl_texture_impl : texture_impl {
    GLuint       tex_id          = 0;
    int          width_          = 0;
    int          height_         = 0;
    int          mip_levels_     = 1;
    pixel_format fmt_            = pixel_format::rgba8888;
    sampler_state sampler_       = {};

    // CPU staging buffer for lock/unlock (glGetTexImage / glTexSubImage2D)
    std::unique_ptr<std::byte[]> stage_buf_;
    std::size_t                  stage_buf_bytes_ = 0;

    ~ogl_texture_impl() override;

    pixel_format  format()     const noexcept override { return fmt_; }
    int           width()      const noexcept override { return width_; }
    int           height()     const noexcept override { return height_; }
    int           mip_levels() const noexcept override { return mip_levels_; }
    sampler_state sampler()    const noexcept override { return sampler_; }

    void set_sampler(const sampler_state& s) override;
    bool lock(rect_i region, int level, texture_lock_info& out) override;
    void unlock(const texture_lock_info& info, bool wrote) override;
    void generate_mipmaps() override;
    std::unique_ptr<texture_impl> clone() const override;

    void apply_sampler() noexcept;
};

// ── OpenGL device impl ──────────────────────────────────────────────

struct ogl_device_impl : gfx_device_impl {
    void* ctx = nullptr;  // opaque context handle (HGLRC on Win32)

    ~ogl_device_impl() override;
    const char* backend_name() const noexcept override { return "opengl"; }

    std::unique_ptr<texture_impl> create_texture(
        pixel_format fmt, vec2i size, int mip_levels) override;
};

// ── OpenGL swapchain impl ───────────────────────────────────────────

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

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL

#endif // ALIA_GFX_DETAIL_OGL_BACKEND_HPP
