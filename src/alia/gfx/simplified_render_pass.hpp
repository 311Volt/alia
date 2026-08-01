#ifndef ALIA_GFX_SIMPLIFIED_RENDER_PASS_HPP
#define ALIA_GFX_SIMPLIFIED_RENDER_PASS_HPP

#include "render_pass.hpp"

namespace alia {
    // Small 2D convenience layer over an explicit pass. Text deliberately is
    // not exposed until its renderer is rebuilt on top of this API.
    class simplified_render_pass {
    public:
        simplified_render_pass(gfx_device &, frame &, const render_pass_desc & = {});
        simplified_render_pass(gfx_device &, frame &, texture &target, const render_pass_desc & = {});
        ~simplified_render_pass() = default;
        simplified_render_pass(const simplified_render_pass &) = delete;
        simplified_render_pass &operator=(const simplified_render_pass &) = delete;
        simplified_render_pass(simplified_render_pass &&) = delete;
        simplified_render_pass &operator=(simplified_render_pass &&) = delete;

        void fill_rect(rect_f, color);
        void draw_rect(rect_f, color, float thickness = 1.0f);
        void draw_line(vec2f a, vec2f b, color, float thickness = 1.0f);
        void draw_textured_rect(rect_f, texture &);
        [[nodiscard]] render_pass &pass() noexcept { return pass_; }
        [[nodiscard]] vec2i target_size() const noexcept { return pass_.target_size(); }

    private:
        basic_effect colored_fx_;
        basic_effect textured_fx_;
        dynamic_pipeline pipeline_;
        render_pass pass_;
    };
} // namespace alia

#endif
