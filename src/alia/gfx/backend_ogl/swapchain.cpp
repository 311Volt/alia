#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"
#include <GL/gl.h>

namespace alia {

    swapchain_handle *ogl_create_swapchain(
        device_handle *dev_h,
        void *native_handle,
        vec2i size,
        const swapchain_desc &desc) {
        auto *dev = as_ogl_device(dev_h);
        framebuffer_properties props;
        void *surface = get_ogl_platform().create_surface(
            native_handle, dev->ctx, desc, props);
        if (!surface)
            return nullptr;

        auto *sc = new ogl_swapchain;
        sc->owner = dev;
        sc->surface = surface;
        sc->size = size;
        sc->props = std::move(props);
        return sc;
    }

    void ogl_destroy_swapchain(swapchain_handle *h) {
        auto *sc = as_ogl_swapchain(h);
        const auto &platform = get_ogl_platform();
        platform.make_current(sc->surface);
        if (sc->target_fbo && ogl_s_glDeleteFramebuffers)
            ogl_s_glDeleteFramebuffers(1, &sc->target_fbo);
        if (sc->owner->current_swapchain == sc)
            sc->owner->current_swapchain = nullptr;
        platform.destroy_surface(sc->surface, sc->owner->ctx);
        delete sc;
    }

    framebuffer_properties ogl_swapchain_properties(const swapchain_handle *h) {
        return as_ogl_swapchain(h)->props;
    }

    void ogl_swapchain_begin_frame(swapchain_handle *h) {
        auto *sc = as_ogl_swapchain(h);
        get_ogl_platform().make_current(sc->surface);
        sc->owner->current_swapchain = sc;
    }

    void ogl_swapchain_end_frame(swapchain_handle *h) {
        auto *sc = as_ogl_swapchain(h);
        ogl_reset_frame_state(*sc->owner);
        sc->owner->current_swapchain = nullptr;
    }

    void ogl_swapchain_present(swapchain_handle *h) {
        get_ogl_platform().swap_buffers(as_ogl_swapchain(h)->surface);
    }

    void ogl_swapchain_on_resize(swapchain_handle *h, vec2i new_size) {
        as_ogl_swapchain(h)->size = new_size;
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
