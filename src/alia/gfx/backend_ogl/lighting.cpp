#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <type_traits>

namespace alia {
    namespace {
        std::array<GLfloat, 4> native_color(color value) {
            return {value.r, value.g, value.b, value.a};
        }
        vec3f unit_direction(vec3f value) {
            const double length = std::hypot(
                static_cast<double>(value.x), static_cast<double>(value.y), static_cast<double>(value.z));
            return {static_cast<float>(value.x / length), static_cast<float>(value.y / length),
                static_cast<float>(value.z / length)};
        }
    }

    void ogl_upload_lights(ogl_device &device) {
        const auto ambient = native_color(device.ambient);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient.data());
        glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
        if (device.separate_specular_color)
            glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);

        // GL captures light positions/directions in eye space at upload time.
        // Use only the view matrix so that object transforms cannot move lights.
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(&device.transforms.view.m[0][0]);
        const auto no_ambient = native_color(black);
        for (int index = 0; index != device.light_count; ++index) {
            const GLenum slot = GL_LIGHT0 + index;
            std::visit([&](const auto &source) {
                using source_type = std::decay_t<decltype(source)>;
                const auto diffuse = native_color(source.diffuse);
                const auto specular = native_color(source.specular);
                glLightfv(slot, GL_AMBIENT, no_ambient.data());
                glLightfv(slot, GL_DIFFUSE, diffuse.data());
                glLightfv(slot, GL_SPECULAR, specular.data());
                if constexpr (std::is_same_v<source_type, directional_light>) {
                    const auto direction = unit_direction(source.direction);
                    const GLfloat position[]{-direction.x, -direction.y, -direction.z, 0.0f};
                    glLightfv(slot, GL_POSITION, position);
                } else {
                    const GLfloat position[]{source.position.x, source.position.y, source.position.z, 1.0f};
                    glLightfv(slot, GL_POSITION, position);
                    glLightf(slot, GL_CONSTANT_ATTENUATION, source.attenuation.constant);
                    glLightf(slot, GL_LINEAR_ATTENUATION, source.attenuation.linear);
                    glLightf(slot, GL_QUADRATIC_ATTENUATION, source.attenuation.quadratic);
                }
                if constexpr (std::is_same_v<source_type, spot_light>) {
                    const auto direction = unit_direction(source.direction);
                    const GLfloat components[]{direction.x, direction.y, direction.z};
                    glLightfv(slot, GL_SPOT_DIRECTION, components);
                    glLightf(slot, GL_SPOT_CUTOFF,
                        source.cutoff_exponent.cutoff * (180.0f / std::numbers::pi_v<float>));
                    glLightf(slot, GL_SPOT_EXPONENT, source.cutoff_exponent.exponent);
                } else {
                    glLightf(slot, GL_SPOT_CUTOFF, 180.0f);
                    glLightf(slot, GL_SPOT_EXPONENT, 0.0f);
                }
            }, device.lights[index]);
            glEnable(slot);
        }
        const transform modelview = device.transforms.world * device.transforms.view;
        glLoadMatrixf(&modelview.m[0][0]);
        device.lights_need_upload = false;
    }

    void ogl_set_lighting(device_handle *h, const lighting_state &lighting) {
        auto &device = *as_ogl_device(h);
        const int count = static_cast<int>(lighting.lights.size());
        for (int index = count; index < device.light_count; ++index)
            glDisable(GL_LIGHT0 + index);
        std::copy(lighting.lights.begin(), lighting.lights.end(), device.lights.begin());
        device.light_count = count;
        device.ambient = lighting.ambient;
        ogl_upload_lights(device);
    }

    void ogl_apply_material(ogl_device &device) {
        const auto &surface = device.surface_material;
        const auto diffuse = native_color(surface.diffuse);
        const auto ambient = native_color(surface.ambient);
        const auto specular = native_color(surface.specular);
        const auto emissive = native_color(surface.emissive);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse.data());
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient.data());
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular.data());
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emissive.data());
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, surface.shininess);
    }

    void ogl_set_material(device_handle *h, const material &surface) {
        auto &device = *as_ogl_device(h);
        device.surface_material = surface;
        ogl_apply_material(device);
    }
} // namespace alia

#endif
