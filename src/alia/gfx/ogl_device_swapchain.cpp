#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "detail/ogl_backend.hpp"
#include <memory>

namespace alia {

    // ── OpenGL device impl ────────────────────────────────────────────────

    ogl_device_impl::~ogl_device_impl() {
        const auto &ops = get_ogl_platform();
        ops.make_none_current();
        ops.destroy_context(ctx);
    }

    // ── OpenGL swapchain impl ─────────────────────────────────────────────

    ogl_swapchain_impl::~ogl_swapchain_impl() { get_ogl_platform().destroy_surface(native, surface); }

    void ogl_swapchain_impl::clear(color c) {
        get_ogl_platform().make_current(surface, ctx);
        glViewport(0, 0, size.x, size.y);
        glClearColor(c.r, c.g, c.b, c.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void ogl_swapchain_impl::present() { get_ogl_platform().swap_buffers(surface); }

    void ogl_swapchain_impl::on_resize(vec2i new_size) { size = new_size; }

    // ── Factory functions ─────────────────────────────────────────────────

    static std::unique_ptr<gfx_device_impl> create_ogl_device() {
        void *ctx = get_ogl_platform().create_context();
        if (!ctx)
            return nullptr;

        auto impl = std::make_unique<ogl_device_impl>();
        impl->ctx = ctx;
        return impl;
    }

    std::unique_ptr<swapchain_impl>
    ogl_device_impl::create_swapchain(void *native_handle, vec2i initial_size) {
        void *surface = get_ogl_platform().create_surface(native_handle, ctx);
        if (!surface)
            return nullptr;

        auto impl = std::make_unique<ogl_swapchain_impl>();
        impl->native = native_handle;
        impl->surface = surface;
        impl->ctx = ctx;
        impl->size = initial_size;
        return impl;
    }

    // ── Registration ──────────────────────────────────────────────────────

    void register_ogl_backend() {
        register_gfx_backend({
            gfx_backend::opengl,
            create_ogl_device,
            {
                ogl_draw_prim,
                ogl_draw_indexed_prim,
                ogl_draw_textured_prim,
                ogl_draw_textured_indexed_prim,
            },
        });
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
