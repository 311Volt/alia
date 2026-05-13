#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"

namespace alia {

    // ── ogl_platform_ops storage and registration ─────────────────────────

    static ogl_platform_ops s_ogl_platform = {};

    void register_ogl_platform(ogl_platform_ops ops) {
        s_ogl_platform = ops;
    }

    const ogl_platform_ops &get_ogl_platform() {
        return s_ogl_platform;
    }

    // ── Device create / destroy ───────────────────────────────────────────

    ogl_device *ogl_create_device() {
        void *ctx = get_ogl_platform().create_context();
        if (!ctx)
            return nullptr;

        auto *dev = new ogl_device;
        dev->ctx = ctx;
        return dev;
    }

    void ogl_destroy_device(device_handle *h) {
        auto *dev = as_ogl_device(h);
        const auto &ops = get_ogl_platform();
        ops.make_none_current();
        ops.destroy_context(dev->ctx);
        delete dev;
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
