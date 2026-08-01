#include "simplified_render_pass.hpp"

namespace alia {
    simplified_render_pass::simplified_render_pass(gfx_device &device, frame &frame, const render_pass_desc &desc)
        : colored_fx_{}, textured_fx_{},
          pipeline_(device, pipeline_config{.effect = &colored_fx_}),
          pass_(frame.begin_render_pass(pipeline_, desc)) {
        make_current(device);
        const transform projection = transform::ortho_ui(pass_.target_size());
        colored_fx_.world = transform::identity();
        colored_fx_.projection = projection;
        textured_fx_.texture_op = texture_operation::replace;
        textured_fx_.world = transform::identity();
        textured_fx_.projection = projection;
    }
    simplified_render_pass::simplified_render_pass(gfx_device &device, frame &frame, texture &target, const render_pass_desc &desc)
        : colored_fx_{}, textured_fx_{},
          pipeline_(device, pipeline_config{.effect = &colored_fx_}),
          pass_(frame.begin_render_pass(pipeline_, target, desc)) {
        make_current(device);
        const transform projection = transform::ortho_ui(pass_.target_size());
        colored_fx_.world = transform::identity();
        colored_fx_.projection = projection;
        textured_fx_.texture_op = texture_operation::replace;
        textured_fx_.world = transform::identity();
        textured_fx_.projection = projection;
    }
    void simplified_render_pass::fill_rect(rect_f r, color c) {
        pipeline_.set_effect(&colored_fx_);
        const colored_vertex vertices[] = {
            {{r.left(), r.top()}, c}, {{r.right(), r.top()}, c}, {{r.left(), r.bottom()}, c},
            {{r.right(), r.top()}, c}, {{r.right(), r.bottom()}, c}, {{r.left(), r.bottom()}, c},
        };
        pass_.draw<colored_vertex>(vertices);
    }
    void simplified_render_pass::draw_line(vec2f a, vec2f b, color c, float thickness) {
        const vec2f delta = b - a;
        const float length = delta.length();
        if (length <= 0.0f || thickness <= 0.0f)
            return;
        const vec2f offset{-delta.y / length * thickness * 0.5f, delta.x / length * thickness * 0.5f};
        pipeline_.set_effect(&colored_fx_);
        const colored_vertex vertices[] = {
            {a + offset, c}, {b + offset, c}, {a - offset, c},
            {b + offset, c}, {b - offset, c}, {a - offset, c},
        };
        pass_.draw<colored_vertex>(vertices);
    }
    void simplified_render_pass::draw_rect(rect_f r, color c, float thickness) {
        const float half = thickness * 0.5f;
        draw_line({r.left() - half, r.top()}, {r.right() + half, r.top()}, c, thickness);
        draw_line({r.right(), r.top() - half}, {r.right(), r.bottom() + half}, c, thickness);
        draw_line({r.right() + half, r.bottom()}, {r.left() - half, r.bottom()}, c, thickness);
        draw_line({r.left(), r.bottom() + half}, {r.left(), r.top() - half}, c, thickness);
    }
    void simplified_render_pass::draw_textured_rect(rect_f r, texture &texture) {
        pipeline_.set_effect(&textured_fx_);
        pass_.set_texture(0, texture);
        const uv_vertex vertices[] = {
            {{r.left(), r.top()}, {0.0f, 0.0f}}, {{r.right(), r.top()}, {1.0f, 0.0f}}, {{r.left(), r.bottom()}, {0.0f, 1.0f}},
            {{r.right(), r.top()}, {1.0f, 0.0f}}, {{r.right(), r.bottom()}, {1.0f, 1.0f}}, {{r.left(), r.bottom()}, {0.0f, 1.0f}},
        };
        pass_.draw<uv_vertex>(vertices);
    }
} // namespace alia
