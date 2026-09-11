#ifndef ALIA_GFX_BACKEND_OGL_PLATFORM_HPP
#define ALIA_GFX_BACKEND_OGL_PLATFORM_HPP

#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "../graphics_backend_interface.hpp"

namespace alia {

    struct ogl_platform_ops {
        void *(*create_context)();
        void (*destroy_context)(void *ctx);
        void *(*create_surface)(
            void *native_handle,
            void *root_ctx,
            const swapchain_desc &,
            framebuffer_properties &out);
        void (*destroy_surface)(void *surface, void *root_ctx);
        void (*make_current)(void *surface);
        void (*make_root_current)(void *root_ctx);
        void (*swap_buffers)(void *surface);
        void (*make_none_current)();
    };

    void register_ogl_platform(ogl_platform_ops ops);
    const ogl_platform_ops &get_ogl_platform();

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
#endif // ALIA_GFX_BACKEND_OGL_PLATFORM_HPP
