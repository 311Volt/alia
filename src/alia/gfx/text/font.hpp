#ifndef FONT_E3608192_E499_42CD_B4DB_D6397F113B99
#define FONT_E3608192_E499_42CD_B4DB_D6397F113B99

#include "alia/core/color.hpp"
#include "alia/core/rect.hpp"
#include "alia/core/vec.hpp"
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace alia {

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
        struct text_impl;
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
        explicit hardware_glyph_buffer(font &source, vec2i page_size = {1024, 1024});
        ~hardware_glyph_buffer();

        hardware_glyph_buffer(hardware_glyph_buffer &&) noexcept;
        hardware_glyph_buffer &operator=(hardware_glyph_buffer &&) noexcept;
        hardware_glyph_buffer(const hardware_glyph_buffer &) = delete;
        hardware_glyph_buffer &operator=(const hardware_glyph_buffer &) = delete;

        [[nodiscard]] font &source_font() const noexcept;
        void clear();

    private:
        friend void draw_text(font &source, std::string_view text, color color, vec2f position, hardware_glyph_buffer *glyph_buffer);

        std::unique_ptr<detail::hardware_glyph_buffer_impl> impl_;
    };

    enum class text_align {
        left,
        center,
        right,
    };

    class text {
    public:
        explicit text(font &source);
        ~text();

        text(text &&) noexcept;
        text &operator=(text &&) noexcept;
        text(const text &) = delete;
        text &operator=(const text &) = delete;

        text &set_text(std::string_view text);
        text &set_align(text_align align);
        text &set_antialiasing(bool enabled);
        text &set_kerning(bool enabled);

        void draw(vec2i position, color text_color = white);

    private:
        std::unique_ptr<detail::text_impl> impl_;
    };

    [[nodiscard]] ttf_font load_ttf_font(std::string_view filename, int pixel_height = 32);

    [[nodiscard]] vec2f measure_text(font &source, std::string_view text);

    void draw_text(font &source, std::string_view text, color color, vec2f position, hardware_glyph_buffer *glyph_buffer = nullptr);

} // namespace alia

#endif /* FONT_E3608192_E499_42CD_B4DB_D6397F113B99 */
