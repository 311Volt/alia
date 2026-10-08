#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"

#include <cmath>
#include <limits>
#include <type_traits>

namespace alia {
    namespace {
        D3DCOLORVALUE native_color(color value) {
            return {value.r, value.g, value.b, value.a};
        }
        D3DVECTOR native_direction(vec3f value) {
            // Validation permits any finite, nonzero magnitude.
            const double length = std::hypot(
                static_cast<double>(value.x), static_cast<double>(value.y), static_cast<double>(value.z));
            return {static_cast<float>(value.x / length), static_cast<float>(value.y / length),
                static_cast<float>(value.z / length)};
        }
    }

    void d3d9_set_lighting(device_handle *h, const lighting_state &lighting) {
        auto &device = *as_d3d9_device(h);
        device.device->SetRenderState(D3DRS_AMBIENT, to_d3d_color(lighting.ambient));
        const int count = static_cast<int>(lighting.lights.size());
        for (int index = 0; index != count; ++index) {
            D3DLIGHT9 value{};
            value.Range = std::sqrt((std::numeric_limits<float>::max)());
            value.Falloff = 1.0f;
            value.Ambient = native_color(black);
            std::visit([&](const auto &source) {
                using source_type = std::decay_t<decltype(source)>;
                value.Diffuse = native_color(source.diffuse);
                value.Specular = native_color(source.specular);
                if constexpr (std::is_same_v<source_type, directional_light>) {
                    value.Type = D3DLIGHT_DIRECTIONAL;
                } else {
                    value.Type = std::is_same_v<source_type, point_light> ? D3DLIGHT_POINT : D3DLIGHT_SPOT;
                    value.Position = {source.position.x, source.position.y, source.position.z};
                    value.Attenuation0 = source.attenuation.constant;
                    value.Attenuation1 = source.attenuation.linear;
                    value.Attenuation2 = source.attenuation.quadratic;
                }
                if constexpr (!std::is_same_v<source_type, point_light>)
                    value.Direction = native_direction(source.direction);
                if constexpr (std::is_same_v<source_type, spot_light>) {
                    value.Theta = source.inner_outer.inner;
                    value.Phi = source.inner_outer.outer;
                }
            }, lighting.lights[index]);
            device.device->SetLight(static_cast<DWORD>(index), &value);
            device.device->LightEnable(static_cast<DWORD>(index), TRUE);
        }
        for (int index = count; index < device.enabled_light_count; ++index)
            device.device->LightEnable(static_cast<DWORD>(index), FALSE);
        device.enabled_light_count = count;
    }

    void d3d9_set_material(device_handle *h, const material &surface) {
        auto &device = *as_d3d9_device(h);
        D3DMATERIAL9 value{};
        value.Diffuse = native_color(surface.diffuse);
        value.Ambient = native_color(surface.ambient);
        value.Specular = native_color(surface.specular);
        value.Emissive = native_color(surface.emissive);
        value.Power = surface.shininess;
        device.device->SetMaterial(&value);
        device.surface_material = surface;
        const bool specular = surface.specular.r != 0.0f || surface.specular.g != 0.0f || surface.specular.b != 0.0f;
        device.device->SetRenderState(D3DRS_SPECULARENABLE, device.state.lighting && specular ? TRUE : FALSE);
    }
} // namespace alia

#endif
