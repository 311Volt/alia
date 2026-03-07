#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "detail/ogl_backend.hpp"
#include <unordered_map>
#include <typeindex>
#include <any>
#include <functional>
#include <vector>

namespace alia {

    // ── Compiled vertex setup cache ──────────────────────────────────────

    struct ogl_compiled_vtx {
        // Sets up vertex array pointers given base address and stride.
        // Also enables the appropriate client states.
        std::function<void(const void *base, int stride)> setup;

        // Disables client states that were enabled by setup.
        std::function<void()> teardown;
    };

    static std::unordered_map<std::type_index, std::any> &vtx_cache() {
        static std::unordered_map<std::type_index, std::any> cache;
        return cache;
    }

    // Single-slot cache: avoids the map lookup when the same type is used consecutively.
    static std::type_index s_last_type{typeid(void)};
    static const ogl_compiled_vtx *s_last_compiled = nullptr;

    static const ogl_compiled_vtx &get_or_compile(std::type_index vtx_type, std::span<const vertex_element> elements) {
        if (vtx_type == s_last_type)
            return *s_last_compiled;

        auto &cache = vtx_cache();
        auto it = cache.find(vtx_type);
        if (it != cache.end()) {
            s_last_type = vtx_type;
            s_last_compiled = &std::any_cast<const ogl_compiled_vtx &>(it->second);
            return *s_last_compiled;
        }

        // Collect setup/teardown actions from element descriptors
        struct attr_action {
            vertex_attr attribute;
            vertex_storage storage;
            int offset;
        };
        std::vector<attr_action> actions;
        for (const auto &e : elements)
            actions.push_back({e.attribute, e.storage, e.offset});

        auto setup = [actions](const void *base, int stride) {
            for (const auto &a : actions) {
                const char *ptr = static_cast<const char *>(base) + a.offset;
                switch (a.attribute) {
                case vertex_attr::position: {
                    int components = (a.storage == vertex_storage::float_3) ? 3 : 2;
                    glEnableClientState(GL_VERTEX_ARRAY);
                    glVertexPointer(components, GL_FLOAT, stride, ptr);
                    break;
                }
                case vertex_attr::color_attr:
                    glEnableClientState(GL_COLOR_ARRAY);
                    glColorPointer(4, GL_FLOAT, stride, ptr);
                    break;
                case vertex_attr::tex_coord:
                    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    glTexCoordPointer(2, GL_FLOAT, stride, ptr);
                    break;
                }
            }
        };

        auto teardown = [actions]() {
            for (const auto &a : actions) {
                switch (a.attribute) {
                case vertex_attr::position:
                    glDisableClientState(GL_VERTEX_ARRAY);
                    break;
                case vertex_attr::color_attr:
                    glDisableClientState(GL_COLOR_ARRAY);
                    break;
                case vertex_attr::tex_coord:
                    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                    break;
                }
            }
        };

        auto [ins, _] = cache.emplace(vtx_type, ogl_compiled_vtx{std::move(setup), std::move(teardown)});
        s_last_type = vtx_type;
        s_last_compiled = &std::any_cast<const ogl_compiled_vtx &>(ins->second);
        return *s_last_compiled;
    }

    // ── Helpers ──────────────────────────────────────────────────────────

    static void setup_matrices(const float *proj, const float *xform) {
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(proj);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(xform);
    }

    static GLenum to_gl_mode(prim_type type) {
        switch (type) {
        case prim_type::triangle_list:
            return GL_TRIANGLES;
        case prim_type::triangle_strip:
            return GL_TRIANGLE_STRIP;
        case prim_type::triangle_fan:
            return GL_TRIANGLE_FAN;
        }
        return GL_TRIANGLES;
    }

    // ── Drawing ──────────────────────────────────────────────────────────

    void ogl_swapchain_impl::draw_prim(
        prim_type type, const void *vertices, int count, int stride, std::type_index vtx_type, std::span<const vertex_element> elements
    ) {
        if (count < 3)
            return;

        setup_matrices(projection_, transform_);
        const auto &compiled = get_or_compile(vtx_type, elements);
        compiled.setup(vertices, stride);
        glDrawArrays(to_gl_mode(type), 0, count);
        compiled.teardown();
    }

    void ogl_swapchain_impl::draw_indexed_prim(
        prim_type type, const void *vertices, int count, int stride, std::span<const uint32_t> indices, std::type_index vtx_type,
        std::span<const vertex_element> elements
    ) {
        if (indices.size() < 3 || count == 0)
            return;

        setup_matrices(projection_, transform_);
        const auto &compiled = get_or_compile(vtx_type, elements);
        compiled.setup(vertices, stride);
        glDrawElements(to_gl_mode(type), static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, indices.data());
        compiled.teardown();
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
