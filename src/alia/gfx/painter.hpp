#ifndef ALIA_GFX_PAINTER_HPP
#define ALIA_GFX_PAINTER_HPP

#include "frame.hpp"

namespace alia {
    // Persistent 2D renderer. begin/end establish the frame-local drawing
    // interval while effects and the dynamic pipeline survive across frames.
    class painter {
    public:
        explicit painter(gfx_device &);
        ~painter() = default;
        painter(const painter &) = delete;
        painter &operator=(const painter &) = delete;
        painter(painter &&) = delete;
        painter &operator=(painter &&) = delete;

        void begin(frame &);
        void end();

        void fill_rect(rect_f, color);
        void draw_rect(rect_f, color, float thickness = 1.0f);
        void draw_line(vec2f a, vec2f b, color, float thickness = 1.0f);
        void draw_textured_rect(rect_f, texture &);
        [[nodiscard]] vec2i target_size() const noexcept { return target_size_; }

    private:
        frame &active_frame() const;

        basic_effect colored_fx_;
        basic_effect textured_fx_;
        dynamic_pipeline pipeline_;
        frame *frame_ = nullptr;
        vec2i target_size_ = {};
    };
} // namespace alia

#endif
