#include "framebuffer_config.hpp"

#include <string_view>

namespace alia {
    namespace {
        template <class T>
        std::optional<std::string> check(
            std::string_view name, const gfx_option<T> &requested, const T &actual) {
            if (requested.required() && requested.value != actual)
                return std::string(name) + " requirement was not met";
            return std::nullopt;
        }
    }

    std::optional<std::string> check_framebuffer_requirements(
        const framebuffer_config &requested,
        const framebuffer_properties &actual) {
#define ALIA_CHECK_FRAMEBUFFER_OPTION(name) \
        if (auto error = check(#name, requested.name, actual.name)) return error
        ALIA_CHECK_FRAMEBUFFER_OPTION(color_bits);
        ALIA_CHECK_FRAMEBUFFER_OPTION(red_bits);
        ALIA_CHECK_FRAMEBUFFER_OPTION(green_bits);
        ALIA_CHECK_FRAMEBUFFER_OPTION(blue_bits);
        ALIA_CHECK_FRAMEBUFFER_OPTION(alpha_bits);
        ALIA_CHECK_FRAMEBUFFER_OPTION(red_shift);
        ALIA_CHECK_FRAMEBUFFER_OPTION(green_shift);
        ALIA_CHECK_FRAMEBUFFER_OPTION(blue_shift);
        ALIA_CHECK_FRAMEBUFFER_OPTION(alpha_shift);
        ALIA_CHECK_FRAMEBUFFER_OPTION(depth_bits);
        ALIA_CHECK_FRAMEBUFFER_OPTION(stencil_bits);
        ALIA_CHECK_FRAMEBUFFER_OPTION(samples);
        ALIA_CHECK_FRAMEBUFFER_OPTION(aux_buffers);
        ALIA_CHECK_FRAMEBUFFER_OPTION(accum_red_bits);
        ALIA_CHECK_FRAMEBUFFER_OPTION(accum_green_bits);
        ALIA_CHECK_FRAMEBUFFER_OPTION(accum_blue_bits);
        ALIA_CHECK_FRAMEBUFFER_OPTION(accum_alpha_bits);
        ALIA_CHECK_FRAMEBUFFER_OPTION(float_color);
        ALIA_CHECK_FRAMEBUFFER_OPTION(float_depth);
        ALIA_CHECK_FRAMEBUFFER_OPTION(single_buffer);
        ALIA_CHECK_FRAMEBUFFER_OPTION(update_display_region);
        ALIA_CHECK_FRAMEBUFFER_OPTION(swap);
#undef ALIA_CHECK_FRAMEBUFFER_OPTION
        return std::nullopt;
    }
} // namespace alia
