#include "alia/gfx/text/font.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>

namespace alia::text_layout_test {

    namespace {

        class fixed_font final : public font {
        public:
            [[nodiscard]] font_metrics metrics() const override {
                return {
                    .ascender = 8.0f,
                    .descender = -2.0f,
                    .line_height = 12.0f,
                };
            }

            [[nodiscard]] glyph_metrics get_glyph_metrics(uint32_t codepoint) override {
                return {.advance = codepoint == ' ' ? 2.0f : 5.0f};
            }

            [[nodiscard]] rendered_glyph render_glyph(uint32_t codepoint) override {
                return {.metrics = get_glyph_metrics(codepoint)};
            }

            [[nodiscard]] float kerning(uint32_t left, uint32_t right) const override {
                return left == 'A' && right == 'V' ? 1.5f : 0.0f;
            }
        };

        void expect_size(vec2f actual, vec2f expected) {
            EXPECT_FLOAT_EQ(actual.x, expected.x);
            EXPECT_FLOAT_EQ(actual.y, expected.y);
        }

    } // namespace

    TEST(text_layout, empty_text_has_zero_size) {
        fixed_font source;
        expect_size(measure_text(source, ""), {});
    }

    TEST(text_layout, single_line_width_is_sum_of_advances) {
        fixed_font source;
        expect_size(measure_text(source, "AB "), {12.0f, 12.0f});
    }

    TEST(text_layout, newline_uses_widest_line_and_carriage_return_is_ignored) {
        fixed_font source;
        expect_size(measure_text(source, "AB\nA\r "), {10.0f, 24.0f});
    }

    TEST(text_layout, tab_is_four_spaces_and_resets_kerning) {
        fixed_font source;
        expect_size(measure_text(source, "A\tV"), {18.0f, 12.0f});
    }

    TEST(text_layout, kerning_only_applies_to_adjacent_glyphs_when_enabled) {
        fixed_font source;
        expect_size(measure_text(source, "AV"), {11.5f, 12.0f});
        expect_size(measure_text(source, "A\nV"), {5.0f, 24.0f});
        expect_size(measure_text(source, "AV", false), {10.0f, 12.0f});
    }

} // namespace alia::text_layout_test
