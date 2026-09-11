#include "alia/gfx/text/font.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>
#include <vector>

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

        class box_font final : public font {
        public:
            [[nodiscard]] font_metrics metrics() const override {
                return {
                    .ascender = 4.0f,
                    .descender = -1.0f,
                    .line_height = 5.0f,
                };
            }

            [[nodiscard]] glyph_metrics get_glyph_metrics(uint32_t codepoint) override {
                if (codepoint == ' ')
                    return {.advance = 2.0f};
                return {
                    .bitmap_size = {3, 3},
                    .bearing = {codepoint == 'j' ? -2 : 0, 3},
                    .advance = 4.0f,
                };
            }

            [[nodiscard]] rendered_glyph render_glyph(uint32_t codepoint) override {
                const glyph_metrics glyph = get_glyph_metrics(codepoint);
                if (codepoint == ' ')
                    return {.metrics = glyph};

                unsigned char coverage = 255;
                if (codepoint == 'x')
                    coverage = 100;
                else if (codepoint == 'y')
                    coverage = 200;
                return {
                    .metrics = glyph,
                    .coverage = std::vector<unsigned char>(9, coverage),
                };
            }

            [[nodiscard]] float kerning(uint32_t left, uint32_t right) const override {
                return left == 'A' && right == 'V' ? 1.5f : 0.0f;
            }
        };

        void expect_size(vec2f actual, vec2f expected) {
            EXPECT_FLOAT_EQ(actual.x, expected.x);
            EXPECT_FLOAT_EQ(actual.y, expected.y);
        }

        void expect_vec(vec2i actual, vec2i expected) {
            EXPECT_EQ(actual.x, expected.x);
            EXPECT_EQ(actual.y, expected.y);
        }

        void expect_all_zero(bitmap &value) {
            auto view = value.view_as<px_gray_u8>();
            for (int y = 0; y < view.height(); ++y) {
                for (int x = 0; x < view.width(); ++x)
                    EXPECT_EQ(view[x, y].v, 0);
            }
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

    TEST(text_raster, empty_text_has_bordered_layout_bitmap) {
        box_font source;
        auto raster = create_text_bitmap(source, "");

        expect_vec(raster.coverage.size(), {2, 7});
        expect_vec(raster.offset, {-1, -1});
        expect_all_zero(raster.coverage);
    }

    TEST(text_raster, glyph_coverage_is_positioned_inside_transparent_border) {
        box_font source;
        auto raster = create_text_bitmap(source, "A");
        auto view = raster.coverage.view_as<px_gray_u8>();

        expect_vec(raster.coverage.size(), {6, 7});
        expect_vec(raster.offset, {-1, -1});
        EXPECT_EQ(view[1, 2].v, 255);
        EXPECT_EQ(view[3, 4].v, 255);
        EXPECT_EQ(view[0, 0].v, 0);
        EXPECT_EQ(view[4, 2].v, 0);
        EXPECT_EQ(view[1, 1].v, 0);
        EXPECT_EQ(view[1, 5].v, 0);
    }

    TEST(text_raster, right_alignment_offsets_each_line_within_the_block) {
        box_font source;
        auto raster = create_text_bitmap(
            source,
            "A\nAA",
            {.align = text_align::right}
        );
        auto view = raster.coverage.view_as<px_gray_u8>();

        EXPECT_EQ(view[5, 2].v, 255);
        EXPECT_EQ(view[1, 2].v, 0);
        EXPECT_EQ(view[1, 7].v, 255);
        EXPECT_EQ(view[5, 7].v, 255);
    }

    TEST(text_raster, antialiasing_option_controls_coverage_thresholding) {
        box_font source;
        auto antialiased = create_text_bitmap(source, "xy");
        auto thresholded = create_text_bitmap(
            source,
            "xy",
            {.antialiasing = false}
        );
        auto antialiased_view = antialiased.coverage.view_as<px_gray_u8>();
        auto thresholded_view = thresholded.coverage.view_as<px_gray_u8>();

        EXPECT_EQ(antialiased_view[1, 2].v, 100);
        EXPECT_EQ(antialiased_view[5, 2].v, 200);
        EXPECT_EQ(thresholded_view[1, 2].v, 0);
        EXPECT_EQ(thresholded_view[5, 2].v, 255);
    }

    TEST(text_raster, negative_left_bearing_is_preserved_in_offset) {
        box_font source;
        auto raster = create_text_bitmap(source, "j");
        auto view = raster.coverage.view_as<px_gray_u8>();

        EXPECT_EQ(raster.offset.x, -3);
        EXPECT_EQ(view[1, 2].v, 255);
    }

    TEST(text_raster, kerning_option_changes_glyph_position) {
        box_font source;
        auto kerned = create_text_bitmap(source, "AV");
        auto unkerned = create_text_bitmap(source, "AV", {.kerning = false});
        auto kerned_view = kerned.coverage.view_as<px_gray_u8>();
        auto unkerned_view = unkerned.coverage.view_as<px_gray_u8>();

        EXPECT_EQ(kerned_view[5, 2].v, 0);
        EXPECT_EQ(kerned_view[7, 2].v, 255);
        EXPECT_EQ(unkerned_view[5, 2].v, 255);
    }

} // namespace alia::text_layout_test
