#ifndef ALIA_GFX_FRAMEBUFFER_CONFIG_HPP
#define ALIA_GFX_FRAMEBUFFER_CONFIG_HPP

#include "bitmap/pixel.hpp"

#include <optional>
#include <string>

namespace alia {

    enum class option_importance {
        dont_care,
        suggest,
        require,
    };

    inline constexpr option_importance dont_care = option_importance::dont_care;
    inline constexpr option_importance suggest = option_importance::suggest;
    inline constexpr option_importance require = option_importance::require;

    // dont_care ignores the value, suggest selects the closest available
    // configuration, and require rejects creation unless the value is exact.
    template <class T>
    struct gfx_option {
        T value{};
        option_importance importance = option_importance::dont_care;

        constexpr gfx_option() = default;
        constexpr gfx_option(T v, option_importance i = option_importance::suggest)
            : value(v), importance(i) {}

        [[nodiscard]] constexpr bool requested() const noexcept {
            return importance != option_importance::dont_care;
        }
        [[nodiscard]] constexpr bool required() const noexcept {
            return importance == option_importance::require;
        }
    };

    enum class swap_method {
        undefined,
        copy,
        flip,
    };

    enum class render_method {
        software,
        hardware,
    };

    struct framebuffer_config {
        gfx_option<int> color_bits;
        gfx_option<int> red_bits;
        gfx_option<int> green_bits;
        gfx_option<int> blue_bits;
        gfx_option<int> alpha_bits;
        gfx_option<int> red_shift;
        gfx_option<int> green_shift;
        gfx_option<int> blue_shift;
        gfx_option<int> alpha_shift;
        gfx_option<int> depth_bits;
        gfx_option<int> stencil_bits;
        gfx_option<int> samples;
        gfx_option<int> aux_buffers;
        gfx_option<int> accum_red_bits;
        gfx_option<int> accum_green_bits;
        gfx_option<int> accum_blue_bits;
        gfx_option<int> accum_alpha_bits;
        gfx_option<bool> float_color;
        gfx_option<bool> float_depth;
        gfx_option<bool> single_buffer;
        gfx_option<bool> update_display_region;
        gfx_option<swap_method> swap;
    };

    struct framebuffer_properties {
        pixel_format color_format = pixel_format::bgra8888;
        int color_bits = 0;
        int red_bits = 0;
        int green_bits = 0;
        int blue_bits = 0;
        int alpha_bits = 0;
        int red_shift = 0;
        int green_shift = 0;
        int blue_shift = 0;
        int alpha_shift = 0;
        int depth_bits = 0;
        int stencil_bits = 0;
        int sample_buffers = 0;
        int samples = 0;
        int aux_buffers = 0;
        int accum_red_bits = 0;
        int accum_green_bits = 0;
        int accum_blue_bits = 0;
        int accum_alpha_bits = 0;
        bool float_color = false;
        bool float_depth = false;
        bool single_buffer = false;
        bool update_display_region = false;
        bool vsync = false;
        swap_method swap = swap_method::undefined;
    };

    struct gfx_device_caps {
        render_method render = render_method::hardware;
        int max_texture_size = 0;
        bool npot_textures = false;
        bool render_to_texture = false;
        bool separate_alpha_blend = false;
        std::string renderer_name;
    };

    struct gfx_device_config {
        int adapter = -1;
        gfx_option<render_method> render;
    };

    [[nodiscard]] std::optional<std::string> check_framebuffer_requirements(
        const framebuffer_config &requested,
        const framebuffer_properties &actual);

} // namespace alia

#endif // ALIA_GFX_FRAMEBUFFER_CONFIG_HPP
