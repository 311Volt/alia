#include "painter.hpp"

#include <stdexcept>

namespace alia {
    painter::painter(gfx_device &device)
        : colored_fx_{}, textured_fx_{}, glyph_fx_{}, mask_fx_{}, pipeline_(device, pipeline_config{.effect = &colored_fx_}), device_(&device) {
        textured_fx_.texture_op = texture_operation::replace;
        glyph_fx_.texture_op = texture_operation::modulate;
        mask_fx_.texture_op = texture_operation::alpha_mask;
    }
    void painter::begin(frame &frame) {
        if (frame_)
            throw std::logic_error("painter::begin: painter is already active");
        if (!frame.valid())
            throw std::logic_error("painter::begin: frame is not active");
        frame_ = &frame;
        target_size_ = frame.target_size();
        const transform projection = transform::ortho_ui(target_size_);
        colored_fx_.world = transform::identity();
        colored_fx_.projection = projection;
        textured_fx_.world = transform::identity();
        textured_fx_.projection = projection;
        glyph_fx_.world = transform::identity();
        glyph_fx_.projection = projection;
        mask_fx_.world = transform::identity();
        mask_fx_.projection = projection;
    }
    void painter::end() {
        frame_ = nullptr;
        target_size_ = {};
    }
    frame &painter::active_frame() const {
        if (!frame_ || !frame_->valid())
            throw std::logic_error("painter: painter is not active");
        return *frame_;
    }
    void painter::fill_rect(rect_f r, color c) {
        auto &frame = active_frame();
        pipeline_.set_effect(&colored_fx_);
        frame.set_pipeline(pipeline_);
        const colored_vertex vertices[] = {
            {{r.left(), r.top()}, c}, {{r.right(), r.top()}, c}, {{r.left(), r.bottom()}, c},
            {{r.right(), r.top()}, c}, {{r.right(), r.bottom()}, c}, {{r.left(), r.bottom()}, c},
        };
        frame.draw<colored_vertex>(vertices);
    }
    void painter::draw_line(vec2f a, vec2f b, color c, float thickness) {
        const vec2f delta = b - a;
        const float length = delta.length();
        if (length <= 0.0f || thickness <= 0.0f)
            return;
        auto &frame = active_frame();
        const vec2f offset{-delta.y / length * thickness * 0.5f, delta.x / length * thickness * 0.5f};
        pipeline_.set_effect(&colored_fx_);
        frame.set_pipeline(pipeline_);
        const colored_vertex vertices[] = {
            {a + offset, c}, {b + offset, c}, {a - offset, c},
            {b + offset, c}, {b - offset, c}, {a - offset, c},
        };
        frame.draw<colored_vertex>(vertices);
    }
    void painter::draw_rect(rect_f r, color c, float thickness) {
        const float half = thickness * 0.5f;
        draw_line({r.left() - half, r.top()}, {r.right() + half, r.top()}, c, thickness);
        draw_line({r.right(), r.top() - half}, {r.right(), r.bottom() + half}, c, thickness);
        draw_line({r.right() + half, r.bottom()}, {r.left() - half, r.bottom()}, c, thickness);
        draw_line({r.left(), r.bottom() + half}, {r.left(), r.top() - half}, c, thickness);
    }
    void painter::draw_textured_rect(rect_f r, texture &texture) {
        auto &frame = active_frame();
        pipeline_.set_effect(&textured_fx_);
        frame.set_pipeline(pipeline_);
        frame.set_texture(0, texture);
        const uv_vertex vertices[] = {
            {{r.left(), r.top()}, {0.0f, 0.0f}}, {{r.right(), r.top()}, {1.0f, 0.0f}}, {{r.left(), r.bottom()}, {0.0f, 1.0f}},
            {{r.right(), r.top()}, {1.0f, 0.0f}}, {{r.right(), r.bottom()}, {1.0f, 1.0f}}, {{r.left(), r.bottom()}, {0.0f, 1.0f}},
        };
        frame.draw<uv_vertex>(vertices);
    }
} // namespace alia
