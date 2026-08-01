#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"
#include <GL/gl.h>

namespace alia {

    swapchain_handle *ogl_create_swapchain(device_handle *dev_h, void *native_handle, vec2i size) {
        auto *dev = as_ogl_device(dev_h);
        void *surface = get_ogl_platform().create_surface(native_handle, dev->ctx);
        if (!surface)
            return nullptr;

        auto *sc = new ogl_swapchain;
        sc->owner = dev;
        sc->native = native_handle;
        sc->surface = surface;
        sc->ctx = dev->ctx;
        sc->size = size;
        return sc;
    }

    void ogl_destroy_swapchain(swapchain_handle *h) {
        auto *sc = as_ogl_swapchain(h);
        get_ogl_platform().destroy_surface(sc->native, sc->surface);
        delete sc;
    }

    void ogl_swapchain_begin_frame(swapchain_handle *h) {
        auto *sc = as_ogl_swapchain(h);
        get_ogl_platform().make_current(sc->surface, sc->ctx);
    }

    void ogl_swapchain_end_frame(swapchain_handle *h) {
        ogl_reset_frame_state(*as_ogl_swapchain(h)->owner);
    }

    void ogl_swapchain_present(swapchain_handle *h) {
        auto *sc = as_ogl_swapchain(h);
        get_ogl_platform().swap_buffers(sc->surface);
    }

    void ogl_swapchain_on_resize(swapchain_handle *h, vec2i new_size) {
        as_ogl_swapchain(h)->size = new_size;
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
