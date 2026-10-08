#ifndef ALIA_GFX_LIGHTING_HPP
#define ALIA_GFX_LIGHTING_HPP

#include "../core/color.hpp"
#include "../core/vec.hpp"

#include <variant>

namespace alia {
    inline constexpr int max_lights = 8;

    enum class spot_light_model { inner_outer, cutoff_exponent };

    // D3D9 full cone angles, in radians: 0 <= inner <= outer <= pi.
    // Both spot parameter sets are required and validated by frame::set_lights.
    struct spot_inner_outer {
        float inner, outer;
        spot_inner_outer() = delete;
        constexpr spot_inner_outer(float inner, float outer) noexcept
            : inner(inner), outer(outer) {}
        bool operator==(const spot_inner_outer &) const = default;
    };
    // OpenGL half-angle in radians, [0, pi/2], and exponent in [0, 128].
    struct spot_cutoff_exponent {
        float cutoff, exponent;
        spot_cutoff_exponent() = delete;
        constexpr spot_cutoff_exponent(float cutoff, float exponent) noexcept
            : cutoff(cutoff), exponent(exponent) {}
        bool operator==(const spot_cutoff_exponent &) const = default;
    };
    // Terms in the distance denominator constant + linear*d + quadratic*d*d.
    // All must be finite and nonnegative, and at least one must be positive.
    struct light_attenuation {
        float constant = 1.0f, linear = 0.0f, quadratic = 0.0f;
        bool operator==(const light_attenuation &) const = default;
    };

    // Positions and directions are in world space. Directions are nonzero
    // vectors pointing along the light's travel; all components must be finite.
    // Global ambient is the only ambient light term. Lights have no range cutoff.
    struct directional_light {
        vec3f direction;
        color diffuse = white, specular = white;
        bool operator==(const directional_light &) const = default;
    };
    struct point_light {
        vec3f position;
        light_attenuation attenuation = {};
        color diffuse = white, specular = white;
        bool operator==(const point_light &) const = default;
    };
    struct spot_light {
        vec3f position;
        vec3f direction;
        spot_inner_outer inner_outer;         // caps().spot_model == inner_outer
        spot_cutoff_exponent cutoff_exponent; // caps().spot_model == cutoff_exponent
        light_attenuation attenuation = {};
        color diffuse = white, specular = white;
        bool operator==(const spot_light &) const = default;
    };
    using light = std::variant<directional_light, point_light, spot_light>;

    struct material {
        color diffuse = white, ambient = white, specular = black, emissive = black;
        float shininess = 0.0f; // [0, 128].
        // A vertex color, when present, replaces diffuse and ambient only.
        bool use_vertex_color = true;
        bool operator==(const material &) const = default;
    };

    enum class fog_mode { none, linear, exp, exp2 };
    // Radial uses distance from the eye; view_depth uses absolute camera-space Z.
    enum class fog_distance_model { radial, view_depth };

    // Fixed-function fog blends RGB towards col, leaving alpha unchanged.
    // Distances use caps().fog_distance, in the same units as the view transform.
    struct fog_state {
        fog_mode mode = fog_mode::none;
        color col = black;
        float start = 0.0f, end = 1.0f; // Linear distances; must differ in linear mode.
        float density = 1.0f; // Nonnegative; used by exp and exp2.
        bool operator==(const fog_state &) const = default;
    };
} // namespace alia

#endif
