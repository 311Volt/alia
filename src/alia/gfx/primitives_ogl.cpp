#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "detail/ogl_backend.hpp"

namespace alia {

    // ── Helpers ───────────────────────────────────────────────────────────

    static void setup_matrices(const float *proj, const float *xform) {
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(proj);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(xform);
    }

    static void emit_vertex(const colored_vertex &v) {
        glColor3f(v.col.r, v.col.g, v.col.b);
        glVertex2f(v.position.x, v.position.y);
    }

    // ── Single triangle ───────────────────────────────────────────────────

    void ogl_swapchain_impl::draw_triangle(colored_vertex v0, colored_vertex v1, colored_vertex v2) {
        setup_matrices(projection_, transform_);
        glBegin(GL_TRIANGLES);
        emit_vertex(v0);
        emit_vertex(v1);
        emit_vertex(v2);
        glEnd();
    }

    // ── Unindexed bulk ────────────────────────────────────────────────────

    void ogl_swapchain_impl::draw_triangles(std::span<const colored_vertex> vertices) {
        if (vertices.size() < 3)
            return;
        setup_matrices(projection_, transform_);
        glBegin(GL_TRIANGLES);
        for (const auto &v : vertices)
            emit_vertex(v);
        glEnd();
    }

    void ogl_swapchain_impl::draw_triangle_strip(std::span<const colored_vertex> vertices) {
        if (vertices.size() < 3)
            return;
        setup_matrices(projection_, transform_);
        glBegin(GL_TRIANGLE_STRIP);
        for (const auto &v : vertices)
            emit_vertex(v);
        glEnd();
    }

    void ogl_swapchain_impl::draw_triangle_fan(std::span<const colored_vertex> vertices) {
        if (vertices.size() < 3)
            return;
        setup_matrices(projection_, transform_);
        glBegin(GL_TRIANGLE_FAN);
        for (const auto &v : vertices)
            emit_vertex(v);
        glEnd();
    }

    // ── Indexed bulk ──────────────────────────────────────────────────────

    void ogl_swapchain_impl::draw_triangles(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices) {
        if (indices.size() < 3)
            return;
        setup_matrices(projection_, transform_);
        glBegin(GL_TRIANGLES);
        for (uint32_t i : indices)
            emit_vertex(vertices[i]);
        glEnd();
    }

    void ogl_swapchain_impl::draw_triangle_strip(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices) {
        if (indices.size() < 3)
            return;
        setup_matrices(projection_, transform_);
        glBegin(GL_TRIANGLE_STRIP);
        for (uint32_t i : indices)
            emit_vertex(vertices[i]);
        glEnd();
    }

    void ogl_swapchain_impl::draw_triangle_fan(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices) {
        if (indices.size() < 3)
            return;
        setup_matrices(projection_, transform_);
        glBegin(GL_TRIANGLE_FAN);
        for (uint32_t i : indices)
            emit_vertex(vertices[i]);
        glEnd();
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
