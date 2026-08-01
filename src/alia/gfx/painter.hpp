#ifndef ALIA_GFX_PAINTER_HPP
#define ALIA_GFX_PAINTER_HPP

#include "frame.hpp"

#include <string_view>

namespace alia {
    class font;
    class text;
    class hardware_glyph_buffer;

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
        // A null glyph cache rasterizes and uploads glyphs for this call; pass
        // a persistent cache to reuse the atlas across calls and frames.
        void draw_text(vec2f position, font &, std::string_view, color = white, hardware_glyph_buffer *glyphs = nullptr);
        void draw_text(vec2i position, text &, color = white);
        [[nodiscard]] vec2i target_size() const noexcept { return target_size_; }

    private:
        frame &active_frame() const;

        basic_effect colored_fx_;
        basic_effect textured_fx_;
        basic_effect glyph_fx_;
        basic_effect mask_fx_;
        dynamic_pipeline pipeline_;
        gfx_device *device_ = nullptr;
        frame *frame_ = nullptr;
        vec2i target_size_ = {};
    };
} // namespace alia

#endif
