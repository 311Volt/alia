#ifndef PRIMITIVE_RENDERER_A43DDFAC_1C6F_42FC_92D8_ACFBCC464245
#define PRIMITIVE_RENDERER_A43DDFAC_1C6F_42FC_92D8_ACFBCC464245

#include "frame.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace alia {

    enum class line_join {
        miter,
        bevel,
    };

    // Indices passed to append_indices are absolute. Tessellators read the
    // current vertex count before appending and offset their local indices.
    template <class T>
    concept primitive_sink = requires(
        T &sink,
        const T &const_sink,
        std::span<const colored_vertex> vertices,
        std::span<const uint32_t> indices
    ) {
        sink.append_vertices(vertices);
        sink.append_indices(indices);
        { const_sink.vertex_count() } -> std::convertible_to<uint32_t>;
    };

    namespace detail {

        inline constexpr float miter_limit = 4.0f;
        inline constexpr float collinear_eps = 1.0e-6f;
        inline constexpr float min_cos_half = 1.0e-4f;
        inline constexpr float duplicate_point_distance_squared = 1.0e-12f;

        [[nodiscard]] constexpr float vec_dot(vec2f a, vec2f b) noexcept {
            return a.x * b.x + a.y * b.y;
        }

        [[nodiscard]] constexpr float vec_cross(vec2f a, vec2f b) noexcept {
            return a.x * b.y - a.y * b.x;
        }

        [[nodiscard]] constexpr vec2f vec_perp(vec2f value) noexcept {
            return {-value.y, value.x};
        }

        struct primitive_vertex_pair {
            // y grows downward; plus/minus are the +normal/-normal sides.
            uint32_t plus;
            uint32_t minus;
        };

        struct primitive_joint {
            primitive_vertex_pair in;
            primitive_vertex_pair out;
        };

        template <primitive_sink Sink>
        [[nodiscard]] primitive_vertex_pair append_vertex_pair(
            Sink &sink,
            vec2f plus,
            vec2f minus,
            color c
        ) {
            const uint32_t base = sink.vertex_count();
            const std::array vertices{
                colored_vertex{plus, c},
                colored_vertex{minus, c},
            };
            sink.append_vertices(std::span<const colored_vertex>{vertices});
            return {base, base + 1};
        }

        template <primitive_sink Sink>
        void append_quad(
            Sink &sink,
            primitive_vertex_pair start,
            primitive_vertex_pair end
        ) {
            const std::array<uint32_t, 6> indices{
                start.plus,
                end.plus,
                end.minus,
                start.plus,
                end.minus,
                start.minus,
            };
            sink.append_indices(std::span<const uint32_t>{indices});
        }

        template <primitive_sink Sink>
        [[nodiscard]] primitive_joint append_joint(
            Sink &sink,
            vec2f point,
            vec2f direction_in,
            vec2f normal_in,
            float length_in,
            vec2f direction_out,
            vec2f normal_out,
            float length_out,
            color c,
            float half_thickness,
            line_join join
        ) {
            const float cross = vec_cross(direction_in, direction_out);
            const float dot = vec_dot(direction_in, direction_out);

            if (std::abs(cross) <= collinear_eps) {
                if (dot >= 0.0f) {
                    const auto pair = append_vertex_pair(
                        sink,
                        point + normal_in * half_thickness,
                        point - normal_in * half_thickness,
                        c
                    );
                    return {pair, pair};
                }

                const uint32_t base = sink.vertex_count();
                const std::array vertices{
                    colored_vertex{point + normal_in * half_thickness, c},
                    colored_vertex{point - normal_in * half_thickness, c},
                    colored_vertex{point + normal_out * half_thickness, c},
                    colored_vertex{point - normal_out * half_thickness, c},
                };
                sink.append_vertices(std::span<const colored_vertex>{vertices});
                return {{base, base + 1}, {base + 2, base + 3}};
            }

            const vec2f miter = (normal_in + normal_out).normalized();
            const float cos_half = vec_dot(miter, normal_out);
            if (join == line_join::miter && cos_half * miter_limit >= 1.0f) {
                const vec2f offset = miter * (half_thickness / cos_half);
                const auto pair = append_vertex_pair(sink, point + offset, point - offset, c);
                return {pair, pair};
            }

            const float outer_sign = cross > 0.0f ? -1.0f : 1.0f;
            const float inner_extent = (std::min)(
                half_thickness / (std::max)(cos_half, min_cos_half),
                (std::min)(length_in, length_out)
            );
            const vec2f inner = point - miter * (outer_sign * inner_extent);
            const vec2f outer_in = point + normal_in * (outer_sign * half_thickness);
            const vec2f outer_out = point + normal_out * (outer_sign * half_thickness);
            const uint32_t base = sink.vertex_count();

            if (outer_sign > 0.0f) {
                const std::array vertices{
                    colored_vertex{outer_in, c},
                    colored_vertex{outer_out, c},
                    colored_vertex{inner, c},
                };
                const std::array<uint32_t, 3> indices{base, base + 1, base + 2};
                sink.append_vertices(std::span<const colored_vertex>{vertices});
                sink.append_indices(std::span<const uint32_t>{indices});
                return {{base, base + 2}, {base + 1, base + 2}};
            }

            const std::array vertices{
                colored_vertex{inner, c},
                colored_vertex{outer_in, c},
                colored_vertex{outer_out, c},
            };
            const std::array<uint32_t, 3> indices{base, base + 2, base + 1};
            sink.append_vertices(std::span<const colored_vertex>{vertices});
            sink.append_indices(std::span<const uint32_t>{indices});
            return {{base, base + 1}, {base, base + 2}};
        }

        template <primitive_sink Sink>
        void emit_fill_rect(Sink &sink, rect_f rectangle, color c) {
            const uint32_t base = sink.vertex_count();
            const std::array vertices{
                colored_vertex{rectangle.tl(), c},
                colored_vertex{rectangle.tr(), c},
                colored_vertex{rectangle.br(), c},
                colored_vertex{rectangle.bl(), c},
            };
            const std::array<uint32_t, 6> indices{
                base,
                base + 1,
                base + 2,
                base,
                base + 2,
                base + 3,
            };
            sink.append_vertices(std::span<const colored_vertex>{vertices});
            sink.append_indices(std::span<const uint32_t>{indices});
        }

        template <primitive_sink Sink>
        void emit_line(
            Sink &sink,
            vec2f a,
            vec2f b,
            color c,
            float thickness = 1.0f
        ) {
            const vec2f delta = b - a;
            const float length = delta.length();
            if (thickness <= 0.0f || length <= 0.0f)
                return;

            const float half_thickness = thickness * 0.5f;
            const vec2f normal = vec_perp(delta / length);
            const vec2f offset = normal * half_thickness;
            const uint32_t base = sink.vertex_count();
            const std::array vertices{
                colored_vertex{a + offset, c},
                colored_vertex{b + offset, c},
                colored_vertex{b - offset, c},
                colored_vertex{a - offset, c},
            };
            const std::array<uint32_t, 6> indices{
                base,
                base + 1,
                base + 2,
                base,
                base + 2,
                base + 3,
            };
            sink.append_vertices(std::span<const colored_vertex>{vertices});
            sink.append_indices(std::span<const uint32_t>{indices});
        }

        template <primitive_sink Sink>
        void emit_polyline(
            Sink &sink,
            std::span<const vec2f> input_points,
            color c,
            float thickness = 1.0f,
            line_join join = line_join::miter,
            bool closed = false
        ) {
            if (thickness <= 0.0f)
                return;

            std::vector<vec2f> points;
            points.reserve(input_points.size());
            for (vec2f point : input_points) {
                if (points.empty() || (point - points.back()).length_squared() >= duplicate_point_distance_squared)
                    points.push_back(point);
            }
            if (closed && points.size() > 1 &&
                (points.front() - points.back()).length_squared() < duplicate_point_distance_squared)
                points.pop_back();

            const std::size_t point_count = points.size();
            if ((!closed && point_count < 2) || (closed && point_count < 3))
                return;

            const std::size_t segment_count = closed ? point_count : point_count - 1;
            std::vector<vec2f> directions(segment_count);
            std::vector<vec2f> normals(segment_count);
            std::vector<float> lengths(segment_count);
            for (std::size_t i = 0; i < segment_count; ++i) {
                const vec2f delta = points[(i + 1) % point_count] - points[i];
                lengths[i] = delta.length();
                directions[i] = delta / lengths[i];
                normals[i] = vec_perp(directions[i]);
            }

            const float half_thickness = thickness * 0.5f;
            const auto joint_at = [&](std::size_t point_index) {
                const std::size_t incoming = (point_index + segment_count - 1) % segment_count;
                const std::size_t outgoing = point_index % segment_count;
                return append_joint(
                    sink,
                    points[point_index],
                    directions[incoming],
                    normals[incoming],
                    lengths[incoming],
                    directions[outgoing],
                    normals[outgoing],
                    lengths[outgoing],
                    c,
                    half_thickness,
                    join
                );
            };

            if (closed) {
                const primitive_joint first = joint_at(0);
                primitive_vertex_pair previous = first.out;
                for (std::size_t i = 1; i < point_count; ++i) {
                    const primitive_joint current = joint_at(i);
                    append_quad(sink, previous, current.in);
                    previous = current.out;
                }
                append_quad(sink, previous, first.in);
                return;
            }

            primitive_vertex_pair previous = append_vertex_pair(
                sink,
                points.front() + normals.front() * half_thickness,
                points.front() - normals.front() * half_thickness,
                c
            );
            for (std::size_t i = 1; i + 1 < point_count; ++i) {
                const primitive_joint current = append_joint(
                    sink,
                    points[i],
                    directions[i - 1],
                    normals[i - 1],
                    lengths[i - 1],
                    directions[i],
                    normals[i],
                    lengths[i],
                    c,
                    half_thickness,
                    join
                );
                append_quad(sink, previous, current.in);
                previous = current.out;
            }
            const primitive_vertex_pair end = append_vertex_pair(
                sink,
                points.back() + normals.back() * half_thickness,
                points.back() - normals.back() * half_thickness,
                c
            );
            append_quad(sink, previous, end);
        }

    } // namespace detail

    // These mesh-only renderers never bind a pipeline or modify an effect;
    // input coordinates remain in world space for the caller's transform.
    // These CRTP-like methods use the concrete explicit-object type as their
    // sink. Calling through generic_primitive_renderer& therefore fails the
    // primitive_sink constraint. Calling an immediate renderer through a
    // primitive_renderer& compiles but loses auto-flush. Always call draw
    // methods on the concrete renderer type.
    //
    // Degenerate policy: non-positive thickness and zero-length lines are
    // ignored. Open polylines need at least two distinct points; closed ones
    // need three. Consequently, degenerate rectangle outlines are ignored.
    //
    // For well-formed input without 180-degree reversals and with join extents
    // no longer than adjacent segments, neighboring quads share joint pairs or
    // meet bevel wedges only at their edges, so coverage does not overlap.
    // Sharp inner clamps and reversal spikes may produce localized overlap.
    class generic_primitive_renderer {
    public:
        void finish_primitive(frame &) noexcept {}

        template <class Self>
        void fill_rect(this Self &&self, frame &target, rect_f rectangle, color c)
            requires primitive_sink<std::remove_reference_t<Self>>
        {
            detail::emit_fill_rect(self, rectangle, c);
            self.finish_primitive(target);
        }

        template <class Self>
        void draw_rect(
            this Self &&self,
            frame &target,
            rect_f rectangle,
            color c,
            float thickness = 1.0f,
            line_join join = line_join::miter
        ) requires primitive_sink<std::remove_reference_t<Self>> {
            const std::array points{
                rectangle.tl(),
                rectangle.tr(),
                rectangle.br(),
                rectangle.bl(),
            };
            detail::emit_polyline(
                self,
                std::span<const vec2f>{points},
                c,
                thickness,
                join,
                true
            );
            self.finish_primitive(target);
        }

        template <class Self>
        void draw_line(
            this Self &&self,
            frame &target,
            vec2f a,
            vec2f b,
            color c,
            float thickness = 1.0f
        ) requires primitive_sink<std::remove_reference_t<Self>> {
            detail::emit_line(self, a, b, c, thickness);
            self.finish_primitive(target);
        }

        template <class Self>
        void draw_polyline(
            this Self &&self,
            frame &target,
            std::span<const vec2f> points,
            color c,
            float thickness = 1.0f,
            line_join join = line_join::miter,
            bool closed = false
        ) requires primitive_sink<std::remove_reference_t<Self>> {
            detail::emit_polyline(self, points, c, thickness, join, closed);
            self.finish_primitive(target);
        }
    };

    class primitive_renderer : public generic_primitive_renderer {
    public:
        void append_vertices(std::span<const colored_vertex> vertices) {
            vertices_.insert(vertices_.end(), vertices.begin(), vertices.end());
        }

        void append_indices(std::span<const uint32_t> indices) {
            indices_.insert(indices_.end(), indices.begin(), indices.end());
        }

        [[nodiscard]] uint32_t vertex_count() const noexcept {
            return static_cast<uint32_t>(vertices_.size());
        }

        [[nodiscard]] bool empty() const noexcept {
            return vertices_.empty();
        }

        void clear() noexcept {
            vertices_.clear();
            indices_.clear();
        }

        void flush(frame &target) {
            if (empty())
                return;
            target.draw_indexed<colored_vertex>(vertices_, indices_);
            clear();
        }

    private:
        std::vector<colored_vertex> vertices_;
        std::vector<uint32_t> indices_;
    };

    class immediate_primitive_renderer : public primitive_renderer {
    public:
        void finish_primitive(frame &target) {
            flush(target);
        }
    };

} // namespace alia

#endif /* PRIMITIVE_RENDERER_A43DDFAC_1C6F_42FC_92D8_ACFBCC464245 */
