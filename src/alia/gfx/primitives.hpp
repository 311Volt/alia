#ifndef PRIMITIVES_D9F74B98_135A_46B0_8FD9_AE94ABAE9905
#define PRIMITIVES_D9F74B98_135A_46B0_8FD9_AE94ABAE9905

#include "gfx_device.hpp"
#include "texture.hpp"
#include "../core/rect.hpp"
#include <cstdint>
#include <span>

namespace alia {

    template <vertex_type TVertex>
    void draw_triangle(TVertex v0, TVertex v1, TVertex v2) {
        TVertex verts[3] = {v0, v1, v2};
        static constexpr auto elems = TVertex::elements();
        draw_prim(prim_type::triangle_list, verts, 3, static_cast<int>(sizeof(TVertex)), typeid(TVertex), elems);
    }

    template <vertex_type TVertex>
    void draw_triangles(std::span<const TVertex> vertices) {
        static constexpr auto elems = TVertex::elements();
        draw_prim(
            prim_type::triangle_list,
            vertices.data(),
            static_cast<int>(vertices.size()),
            static_cast<int>(sizeof(TVertex)),
            typeid(TVertex),
            elems
        );
    }

    template <vertex_type TVertex>
    void draw_triangle_strip(std::span<const TVertex> vertices) {
        static constexpr auto elems = TVertex::elements();
        draw_prim(
            prim_type::triangle_strip,
            vertices.data(),
            static_cast<int>(vertices.size()),
            static_cast<int>(sizeof(TVertex)),
            typeid(TVertex),
            elems
        );
    }

    template <vertex_type TVertex>
    void draw_triangle_fan(std::span<const TVertex> vertices) {
        static constexpr auto elems = TVertex::elements();
        draw_prim(
            prim_type::triangle_fan,
            vertices.data(),
            static_cast<int>(vertices.size()),
            static_cast<int>(sizeof(TVertex)),
            typeid(TVertex),
            elems
        );
    }

    template <vertex_type TVertex>
    void draw_triangles(std::span<const TVertex> vertices, std::span<const uint32_t> indices) {
        static constexpr auto elems = TVertex::elements();
        draw_indexed_prim(
            prim_type::triangle_list,
            vertices.data(),
            static_cast<int>(vertices.size()),
            static_cast<int>(sizeof(TVertex)),
            indices,
            typeid(TVertex),
            elems
        );
    }

    template <vertex_type TVertex>
    void draw_triangle_strip(std::span<const TVertex> vertices, std::span<const uint32_t> indices) {
        static constexpr auto elems = TVertex::elements();
        draw_indexed_prim(
            prim_type::triangle_strip,
            vertices.data(),
            static_cast<int>(vertices.size()),
            static_cast<int>(sizeof(TVertex)),
            indices,
            typeid(TVertex),
            elems
        );
    }

    template <vertex_type TVertex>
    void draw_triangle_fan(std::span<const TVertex> vertices, std::span<const uint32_t> indices) {
        static constexpr auto elems = TVertex::elements();
        draw_indexed_prim(
            prim_type::triangle_fan,
            vertices.data(),
            static_cast<int>(vertices.size()),
            static_cast<int>(sizeof(TVertex)),
            indices,
            typeid(TVertex),
            elems
        );
    }

    template <vertex_type TVertex>
    void draw_textured_triangles(texture &tex, std::span<const TVertex> vertices) {
        static constexpr auto elems = TVertex::elements();
        draw_textured_prim(
            prim_type::triangle_list,
            vertices.data(),
            static_cast<int>(vertices.size()),
            static_cast<int>(sizeof(TVertex)),
            typeid(TVertex),
            elems,
            tex
        );
    }

    template <vertex_type TVertex>
    void draw_textured_triangle_strip(texture &tex, std::span<const TVertex> vertices) {
        static constexpr auto elems = TVertex::elements();
        draw_textured_prim(
            prim_type::triangle_strip,
            vertices.data(),
            static_cast<int>(vertices.size()),
            static_cast<int>(sizeof(TVertex)),
            typeid(TVertex),
            elems,
            tex
        );
    }

    template <vertex_type TVertex>
    void draw_textured_triangle_fan(texture &tex, std::span<const TVertex> vertices) {
        static constexpr auto elems = TVertex::elements();
        draw_textured_prim(
            prim_type::triangle_fan,
            vertices.data(),
            static_cast<int>(vertices.size()),
            static_cast<int>(sizeof(TVertex)),
            typeid(TVertex),
            elems,
            tex
        );
    }

    template <vertex_type TVertex>
    void draw_textured_triangles(texture &tex, std::span<const TVertex> vertices, std::span<const uint32_t> indices) {
        static constexpr auto elems = TVertex::elements();
        draw_textured_indexed_prim(
            prim_type::triangle_list,
            vertices.data(),
            static_cast<int>(vertices.size()),
            static_cast<int>(sizeof(TVertex)),
            indices,
            typeid(TVertex),
            elems,
            tex
        );
    }

    template <vertex_type TVertex>
    void draw_textured_triangle_strip(texture &tex, std::span<const TVertex> vertices, std::span<const uint32_t> indices) {
        static constexpr auto elems = TVertex::elements();
        draw_textured_indexed_prim(
            prim_type::triangle_strip,
            vertices.data(),
            static_cast<int>(vertices.size()),
            static_cast<int>(sizeof(TVertex)),
            indices,
            typeid(TVertex),
            elems,
            tex
        );
    }

    template <vertex_type TVertex>
    void draw_textured_triangle_fan(texture &tex, std::span<const TVertex> vertices, std::span<const uint32_t> indices) {
        static constexpr auto elems = TVertex::elements();
        draw_textured_indexed_prim(
            prim_type::triangle_fan,
            vertices.data(),
            static_cast<int>(vertices.size()),
            static_cast<int>(sizeof(TVertex)),
            indices,
            typeid(TVertex),
            elems,
            tex
        );
    }

    void fill_rect(rect_f r, color c);
    void draw_rect(rect_f r, color c, float thickness = 1.0f);
    void draw_line(vec2f a, vec2f b, color c, float thickness = 1.0f);
    void draw_textured_rect(rect_f r, texture &tex);

} // namespace alia

#endif /* PRIMITIVES_D9F74B98_135A_46B0_8FD9_AE94ABAE9905 */
