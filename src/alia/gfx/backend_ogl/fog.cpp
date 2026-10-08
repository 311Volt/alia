#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"

namespace alia {
    namespace {
        GLint native_fog_mode(fog_mode mode) {
            switch (mode) {
            // GL has no disabled equation; render_state controls GL_FOG.
            case fog_mode::none:
            case fog_mode::linear: return GL_LINEAR;
            case fog_mode::exp: return GL_EXP;
            case fog_mode::exp2: return GL_EXP2;
            }
            return GL_LINEAR;
        }
    }

    void ogl_set_fog(device_handle *h, const fog_state &fog) {
        const auto &device = *as_ogl_device(h);
        glFogi(GL_FOG_MODE, native_fog_mode(fog.mode));
        glFogf(GL_FOG_START, fog.start);
        glFogf(GL_FOG_END, fog.end);
        glFogf(GL_FOG_DENSITY, fog.density);
        const GLfloat components[]{fog.col.r, fog.col.g, fog.col.b, fog.col.a};
        glFogfv(GL_FOG_COLOR, components);
        // Apply this while the swapchain's context is current.
        if (device.radial_fog)
            glFogi(GL_FOG_DISTANCE_MODE_NV, GL_EYE_RADIAL_NV);
    }
} // namespace alia

#endif
