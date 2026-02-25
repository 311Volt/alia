#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include <GL/gl.h>
#include <algorithm>
#include <memory>
#include <span>

#include "gfx_device.hpp"

namespace alia {

// ── OpenGL device impl ────────────────────────────────────────────────

struct ogl_device_impl : gfx_device_impl {
    void* ctx = nullptr;  // opaque context handle (HGLRC on Win32)

    ~ogl_device_impl() override {
        const auto& ops = get_ogl_platform();
        ops.make_none_current();
        ops.destroy_context(ctx);
    }

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

    ~ogl_swapchain_impl() override {
        get_ogl_platform().destroy_surface(native, surface);
    }

    void clear(color c) override {
        get_ogl_platform().make_current(surface, ctx);
        glViewport(0, 0, size.x, size.y);
        glClearColor(c.r, c.g, c.b, c.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void set_transform(std::span<const float, 16> m) override {
        std::copy(m.begin(), m.end(), transform_);
    }
    void get_transform(std::span<float, 16> m) const override {
        std::copy(std::begin(transform_), std::end(transform_), m.begin());
    }
    void set_projection(std::span<const float, 16> m) override {
        std::copy(m.begin(), m.end(), projection_);
    }
    void get_projection(std::span<float, 16> m) const override {
        std::copy(std::begin(projection_), std::end(projection_), m.begin());
    }

    void draw_triangle(colored_vertex v0, colored_vertex v1, colored_vertex v2) override {
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(projection_);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(transform_);

        // Immediate-mode OpenGL 2.x
        glBegin(GL_TRIANGLES);
            glColor3f(v0.col.r, v0.col.g, v0.col.b);
            glVertex2f(v0.position.x, v0.position.y);
            glColor3f(v1.col.r, v1.col.g, v1.col.b);
            glVertex2f(v1.position.x, v1.position.y);
            glColor3f(v2.col.r, v2.col.g, v2.col.b);
            glVertex2f(v2.position.x, v2.position.y);
        glEnd();
    }

    void present() override {
        get_ogl_platform().swap_buffers(surface);
    }

    void on_resize(vec2i new_size) override {
        size = new_size;
    }
};

// ── Factory functions ─────────────────────────────────────────────────

static std::unique_ptr<gfx_device_impl> create_ogl_device() {
    void* ctx = get_ogl_platform().create_context();
    if (!ctx) return nullptr;

    auto impl  = std::make_unique<ogl_device_impl>();
    impl->ctx  = ctx;
    return impl;
}

static std::unique_ptr<swapchain_impl> create_ogl_swapchain(
    gfx_device_impl& dev, void* native_handle, vec2i initial_size)
{
    auto& ogl_dev = static_cast<ogl_device_impl&>(dev);
    void* surface = get_ogl_platform().create_surface(native_handle, ogl_dev.ctx);
    if (!surface) return nullptr;

    auto impl    = std::make_unique<ogl_swapchain_impl>();
    impl->native  = native_handle;
    impl->surface = surface;
    impl->ctx     = ogl_dev.ctx;
    impl->size    = initial_size;
    return impl;
}

// ── Registration function ─────────────────────────────────────────────

void register_ogl_backend() {
    register_gfx_backend({
        "opengl",
        gfx_backend::opengl,
        create_ogl_device,
        create_ogl_swapchain,
    });
}

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
