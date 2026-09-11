#ifndef FONT_A9A7DDC3_2E5E_40F5_BD9A_A4D226FAD5B4
#define FONT_A9A7DDC3_2E5E_40F5_BD9A_A4D226FAD5B4

#include "alia/core/color.hpp"
#include "alia/core/rect.hpp"
#include "alia/core/vec.hpp"
#include "alia/gfx/bitmap/bitmap.hpp"
#include "alia/gfx/texture.hpp"
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace alia {

    class frame;

    struct font_metrics {
        float ascender = 0.0f;
        float descender = 0.0f;
        float line_height = 0.0f;
    };

    struct glyph_metrics {
        vec2i bitmap_size;
        vec2i bearing;
        float advance = 0.0f;
    };

    struct rendered_glyph {
        glyph_metrics metrics;
        std::vector<unsigned char> coverage;

        [[nodiscard]] bool has_bitmap() const noexcept {
            return metrics.bitmap_size.x > 0 && metrics.bitmap_size.y > 0;
        }
    };

    class font {
    public:
        virtual ~font() = default;

        [[nodiscard]] virtual font_metrics metrics() const = 0;
        [[nodiscard]] virtual glyph_metrics get_glyph_metrics(uint32_t codepoint) = 0;
        [[nodiscard]] virtual rendered_glyph render_glyph(uint32_t codepoint) = 0;
        [[nodiscard]] virtual float kerning(uint32_t left, uint32_t right) const = 0;
    };

    namespace detail {
        struct ttf_font_impl;
        struct hardware_glyph_buffer_impl;
    } // namespace detail

    class ttf_font final : public font {
    public:
        ttf_font() = default;
        ~ttf_font() override;

        ttf_font(ttf_font &&) noexcept;
        ttf_font &operator=(ttf_font &&) noexcept;
        ttf_font(const ttf_font &) = delete;
        ttf_font &operator=(const ttf_font &) = delete;

        [[nodiscard]] font_metrics metrics() const override;
        [[nodiscard]] glyph_metrics get_glyph_metrics(uint32_t codepoint) override;
        [[nodiscard]] rendered_glyph render_glyph(uint32_t codepoint) override;
        [[nodiscard]] float kerning(uint32_t left, uint32_t right) const override;

    private:
        friend ttf_font load_ttf_font(std::string_view filename, int pixel_height);

        explicit ttf_font(std::unique_ptr<detail::ttf_font_impl> impl) noexcept;

        std::unique_ptr<detail::ttf_font_impl> impl_;
    };

    class hardware_glyph_buffer {
    public:
        hardware_glyph_buffer(gfx_device &device, font &source, vec2i page_size = {1024, 1024});
        ~hardware_glyph_buffer();

        hardware_glyph_buffer(hardware_glyph_buffer &&) noexcept;
        hardware_glyph_buffer &operator=(hardware_glyph_buffer &&) noexcept;
        hardware_glyph_buffer(const hardware_glyph_buffer &) = delete;
        hardware_glyph_buffer &operator=(const hardware_glyph_buffer &) = delete;

        [[nodiscard]] font &source_font() const noexcept;
        void clear();

    private:
        friend void draw_text(
            frame &target,
            vec2f position,
            hardware_glyph_buffer &buffer,
            std::string_view value,
            color text_color
        );

        std::unique_ptr<detail::hardware_glyph_buffer_impl> impl_;
    };

    enum class text_align {
        left,
        center,
        right,
    };

    struct text_raster_options {
        text_align align = text_align::left;
        bool antialiasing = true;
        bool kerning = true;
    };

    // CPU-rasterized text. coverage is a gray_u8 alpha mask. offset is where
    // its top-left pixel lands relative to the layout origin (the position
    // later passed to draw_text). It is at most {-1, -1} because of the 1px
    // transparent border, and smaller when glyphs overhang the layout box.
    struct text_bitmap {
        bitmap coverage;
        vec2i offset;
    };

    // GPU counterpart: mask is an alpha_mask texture with a clamp sampler.
    struct text_texture {
        texture mask;
        vec2i offset;
    };

    [[nodiscard]] ttf_font load_ttf_font(std::string_view filename, int pixel_height = 32);

    [[nodiscard]] vec2f measure_text(font &source, std::string_view text, bool kerning = true);

    [[nodiscard]] text_bitmap create_text_bitmap(
        font &source,
        std::string_view value,
        const text_raster_options &options = {}
    );

    [[nodiscard]] text_texture create_text_texture(
        gfx_device &device,
        const text_bitmap &source,
        texture_filter filter = texture_filter::linear
    );

    [[nodiscard]] text_texture create_text_texture(
        gfx_device &device,
        font &source,
        std::string_view value,
        const text_raster_options &options = {}
    );

    // The caller binds a full_vertex pipeline whose basic_effect uses
    // texture_operation::alpha_mask and owns the projection. These overloads
    // rebind texture slot 0 and submit immediately.
    void draw_text(
        frame &target,
        vec2f position,
        hardware_glyph_buffer &buffer,
        std::string_view value,
        color text_color = white
    );
    void draw_text(frame &target, vec2f position, text_texture &value, color text_color = white);

} // namespace alia

#endif /* FONT_A9A7DDC3_2E5E_40F5_BD9A_A4D226FAD5B4 */
