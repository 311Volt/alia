#include "alia/gfx/primitive_renderer.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace alia::primitive_renderer_test {

    namespace {

        struct recording_sink {
            std::vector<colored_vertex> vertices;
            std::vector<uint32_t> indices;

            void append_vertices(std::span<const colored_vertex> value) {
                vertices.insert(vertices.end(), value.begin(), value.end());
            }

            void append_indices(std::span<const uint32_t> value) {
                indices.insert(indices.end(), value.begin(), value.end());
            }

            [[nodiscard]] uint32_t vertex_count() const noexcept {
                return static_cast<uint32_t>(vertices.size());
            }
        };

        static_assert(primitive_sink<recording_sink>);

        [[nodiscard]] float signed_double_area(vec2f a, vec2f b, vec2f c) {
            return detail::vec_cross(b - a, c - a);
        }

        [[nodiscard]] float covered_area(const recording_sink &sink) {
            float result = 0.0f;
            for (std::size_t i = 0; i < sink.indices.size(); i += 3) {
                const vec2f a = sink.vertices[sink.indices[i]].position;
                const vec2f b = sink.vertices[sink.indices[i + 1]].position;
                const vec2f c = sink.vertices[sink.indices[i + 2]].position;
                result += std::abs(signed_double_area(a, b, c)) * 0.5f;
            }
            return result;
        }

        void expect_position(vec2f actual, vec2f expected) {
            EXPECT_FLOAT_EQ(actual.x, expected.x);
            EXPECT_FLOAT_EQ(actual.y, expected.y);
        }

        void expect_uniform_winding(const recording_sink &sink) {
            ASSERT_FALSE(sink.indices.empty());
            float reference_area = 0.0f;
            for (std::size_t i = 0; i < sink.indices.size(); i += 3) {
                const vec2f a = sink.vertices[sink.indices[i]].position;
                const vec2f b = sink.vertices[sink.indices[i + 1]].position;
                const vec2f c = sink.vertices[sink.indices[i + 2]].position;
                const float area = signed_double_area(a, b, c);
                ASSERT_NE(area, 0.0f);
                if (reference_area == 0.0f)
                    reference_area = area;
                EXPECT_GT(area * reference_area, 0.0f);
            }
        }

        template <std::size_t N>
        void emit_closed(
            recording_sink &sink,
            const std::array<vec2f, N> &points,
            float thickness,
            line_join join
        ) {
            detail::emit_polyline(
                sink,
                std::span<const vec2f>{points},
                white,
                thickness,
                join,
                true
            );
        }

    } // namespace

    TEST(primitive_renderer, line_emits_expected_butt_cap_quad) {
        recording_sink sink;
        detail::emit_line(sink, {0.0f, 0.0f}, {4.0f, 0.0f}, white, 2.0f);

        ASSERT_EQ(sink.vertices.size(), 4u);
        ASSERT_EQ(sink.indices.size(), 6u);
        expect_position(sink.vertices[0].position, {0.0f, 1.0f});
        expect_position(sink.vertices[1].position, {4.0f, 1.0f});
        expect_position(sink.vertices[2].position, {4.0f, -1.0f});
        expect_position(sink.vertices[3].position, {0.0f, -1.0f});
        EXPECT_EQ(sink.indices, (std::vector<uint32_t>{0, 1, 2, 0, 2, 3}));
    }

    TEST(primitive_renderer, degenerate_strokes_emit_nothing) {
        recording_sink sink;
        detail::emit_line(sink, {0.0f, 0.0f}, {4.0f, 0.0f}, white, 0.0f);
        detail::emit_line(sink, {1.0f, 1.0f}, {1.0f, 1.0f}, white, 2.0f);

        constexpr std::array one_point{vec2f{1.0f, 1.0f}};
        detail::emit_polyline(sink, one_point, white, 2.0f);
        constexpr std::array two_closed_points{vec2f{0.0f, 0.0f}, vec2f{1.0f, 0.0f}};
        detail::emit_polyline(sink, two_closed_points, white, 2.0f, line_join::miter, true);
        constexpr std::array degenerate_rectangle{
            vec2f{2.0f, 1.0f},
            vec2f{2.0f, 1.0f},
            vec2f{2.0f, 5.0f},
            vec2f{2.0f, 5.0f},
        };
        detail::emit_polyline(sink, degenerate_rectangle, white, 2.0f, line_join::miter, true);

        EXPECT_TRUE(sink.vertices.empty());
        EXPECT_TRUE(sink.indices.empty());
    }

    TEST(primitive_renderer, fill_rect_has_clockwise_area) {
        recording_sink sink;
        detail::emit_fill_rect(sink, rect_f::pos_size({2.0f, 3.0f}, {6.0f, 4.0f}), red);

        ASSERT_EQ(sink.vertices.size(), 4u);
        ASSERT_EQ(sink.indices.size(), 6u);
        EXPECT_FLOAT_EQ(covered_area(sink), 24.0f);
        EXPECT_GT(
            signed_double_area(
                sink.vertices[0].position,
                sink.vertices[1].position,
                sink.vertices[2].position
            ),
            0.0f
        );
    }

    TEST(primitive_renderer, rectangle_miter_outline_is_gap_free_and_non_overlapping) {
        recording_sink sink;
        constexpr std::array points{
            vec2f{0.0f, 0.0f},
            vec2f{10.0f, 0.0f},
            vec2f{10.0f, 6.0f},
            vec2f{0.0f, 6.0f},
        };
        constexpr float thickness = 2.0f;
        emit_closed(sink, points, thickness, line_join::miter);

        EXPECT_EQ(sink.vertices.size(), 8u);
        EXPECT_EQ(sink.indices.size(), 24u);
        const float expected =
            (10.0f + thickness) * (6.0f + thickness) -
            (10.0f - thickness) * (6.0f - thickness);
        EXPECT_NEAR(covered_area(sink), expected, 1.0e-5f);
    }

    TEST(primitive_renderer, rectangle_bevel_outline_cuts_outer_corners) {
        recording_sink sink;
        constexpr std::array points{
            vec2f{0.0f, 0.0f},
            vec2f{10.0f, 0.0f},
            vec2f{10.0f, 6.0f},
            vec2f{0.0f, 6.0f},
        };
        constexpr float thickness = 2.0f;
        emit_closed(sink, points, thickness, line_join::bevel);

        EXPECT_EQ(sink.vertices.size(), 12u);
        EXPECT_EQ(sink.indices.size(), 36u);
        const float miter_area =
            (10.0f + thickness) * (6.0f + thickness) -
            (10.0f - thickness) * (6.0f - thickness);
        EXPECT_NEAR(covered_area(sink), miter_area - thickness * thickness * 0.5f, 1.0e-5f);
    }

    TEST(primitive_renderer, bevel_mesh_uses_uniform_winding) {
        recording_sink sink;
        constexpr std::array points{
            vec2f{0.0f, 0.0f},
            vec2f{8.0f, 0.0f},
            vec2f{8.0f, 6.0f},
            vec2f{2.0f, 9.0f},
        };
        detail::emit_polyline(sink, points, white, 2.0f, line_join::bevel);

        expect_uniform_winding(sink);
    }

    TEST(primitive_renderer, open_right_angle_miter_shares_exact_tip) {
        recording_sink sink;
        constexpr std::array points{
            vec2f{0.0f, 0.0f},
            vec2f{10.0f, 0.0f},
            vec2f{10.0f, 10.0f},
        };
        detail::emit_polyline(sink, points, white, 2.0f, line_join::miter);

        ASSERT_EQ(sink.vertices.size(), 6u);
        EXPECT_EQ(sink.indices.size(), 12u);
        expect_position(sink.vertices[2].position, {9.0f, 1.0f});
        expect_position(sink.vertices[3].position, {11.0f, -1.0f});
    }

    TEST(primitive_renderer, open_right_angle_bevel_adds_join_wedge) {
        recording_sink sink;
        constexpr std::array points{
            vec2f{0.0f, 0.0f},
            vec2f{10.0f, 0.0f},
            vec2f{10.0f, 10.0f},
        };
        detail::emit_polyline(sink, points, white, 2.0f, line_join::bevel);

        EXPECT_EQ(sink.vertices.size(), 7u);
        EXPECT_EQ(sink.indices.size(), 15u);
        expect_uniform_winding(sink);
    }

    TEST(primitive_renderer, over_limit_miter_falls_back_to_bevel) {
        recording_sink sink;
        constexpr std::array points{
            vec2f{0.0f, 0.0f},
            vec2f{10.0f, 0.0f},
            vec2f{0.01f, 0.1f},
        };
        detail::emit_polyline(sink, points, white, 2.0f, line_join::miter);

        EXPECT_EQ(sink.vertices.size(), 7u);
        EXPECT_EQ(sink.indices.size(), 15u);
    }

    TEST(primitive_renderer, collinear_interior_point_shares_pair) {
        recording_sink sink;
        constexpr std::array points{
            vec2f{0.0f, 0.0f},
            vec2f{4.0f, 0.0f},
            vec2f{10.0f, 0.0f},
        };
        detail::emit_polyline(sink, points, white, 2.0f);

        EXPECT_EQ(sink.vertices.size(), 6u);
        EXPECT_EQ(sink.indices.size(), 12u);
        EXPECT_NEAR(covered_area(sink), 20.0f, 1.0e-5f);
    }

    TEST(primitive_renderer, filters_consecutive_and_closed_back_duplicates) {
        recording_sink open_sink;
        constexpr std::array open_points{
            vec2f{0.0f, 0.0f},
            vec2f{0.0f, 0.0f},
            vec2f{5.0f, 0.0f},
            vec2f{5.0f, 0.0f},
            vec2f{10.0f, 0.0f},
        };
        detail::emit_polyline(open_sink, open_points, white, 2.0f);
        EXPECT_EQ(open_sink.vertices.size(), 6u);
        EXPECT_EQ(open_sink.indices.size(), 12u);

        recording_sink closed_sink;
        constexpr std::array closed_points{
            vec2f{0.0f, 0.0f},
            vec2f{4.0f, 0.0f},
            vec2f{4.0f, 4.0f},
            vec2f{0.0f, 4.0f},
            vec2f{0.0f, 0.0f},
        };
        detail::emit_polyline(closed_sink, closed_points, white, 2.0f, line_join::miter, true);
        EXPECT_EQ(closed_sink.vertices.size(), 8u);
        EXPECT_EQ(closed_sink.indices.size(), 24u);
    }

    TEST(primitive_renderer, closed_triangle_and_square_have_exact_counts) {
        recording_sink triangle_sink;
        constexpr std::array triangle{
            vec2f{0.0f, 0.0f},
            vec2f{4.0f, 0.0f},
            vec2f{2.0f, 3.4641016f},
        };
        emit_closed(triangle_sink, triangle, 1.0f, line_join::miter);
        EXPECT_EQ(triangle_sink.vertices.size(), 6u);
        EXPECT_EQ(triangle_sink.indices.size(), 18u);

        recording_sink square_sink;
        constexpr std::array square{
            vec2f{0.0f, 0.0f},
            vec2f{4.0f, 0.0f},
            vec2f{4.0f, 4.0f},
            vec2f{0.0f, 4.0f},
        };
        emit_closed(square_sink, square, 1.0f, line_join::miter);
        EXPECT_EQ(square_sink.vertices.size(), 8u);
        EXPECT_EQ(square_sink.indices.size(), 24u);
    }

    TEST(primitive_renderer, reversal_spike_uses_independent_pairs) {
        recording_sink sink;
        constexpr std::array points{
            vec2f{0.0f, 0.0f},
            vec2f{5.0f, 0.0f},
            vec2f{0.0f, 0.0f},
        };
        detail::emit_polyline(sink, points, white, 2.0f);

        EXPECT_EQ(sink.vertices.size(), 8u);
        EXPECT_EQ(sink.indices.size(), 12u);
    }

    TEST(primitive_renderer, offsets_indices_by_existing_vertex_count) {
        recording_sink sink;
        sink.vertices.push_back({{-100.0f, -100.0f}, black});
        detail::emit_line(sink, {0.0f, 0.0f}, {4.0f, 0.0f}, white, 2.0f);

        ASSERT_EQ(sink.indices.size(), 6u);
        EXPECT_EQ(sink.indices, (std::vector<uint32_t>{1, 2, 3, 1, 3, 4}));
    }

    TEST(primitive_renderer, batching_state_appends_and_clears) {
        primitive_renderer renderer;
        EXPECT_TRUE(renderer.empty());
        EXPECT_EQ(renderer.vertex_count(), 0u);

        const std::array vertices{
            colored_vertex{{0.0f, 0.0f}, white},
            colored_vertex{{1.0f, 0.0f}, white},
        };
        const std::array<uint32_t, 2> indices{0, 1};
        renderer.append_vertices(vertices);
        renderer.append_indices(indices);
        EXPECT_FALSE(renderer.empty());
        EXPECT_EQ(renderer.vertex_count(), 2u);

        renderer.clear();
        EXPECT_TRUE(renderer.empty());
        EXPECT_EQ(renderer.vertex_count(), 0u);
    }

} // namespace alia::primitive_renderer_test
