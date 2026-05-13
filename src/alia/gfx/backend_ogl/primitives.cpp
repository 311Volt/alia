#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"
#include "../gfx_device.hpp"
#include <GL/gl.h>
#include <any>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace alia {

    // ── Compiled vertex setup cache ──────────────────────────────────────

    struct ogl_compiled_vtx {
        std::function<void(const void *base, int stride, bool shader_active)> setup;
        std::function<void(bool shader_active)> teardown;
    };

    static std::unordered_map<std::type_index, std::any> &vtx_cache() {
        static std::unordered_map<std::type_index, std::any> cache;
        return cache;
    }

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

        struct attr_action {
            vertex_attr attribute;
            vertex_storage storage;
            int offset;
        };
        std::vector<attr_action> actions;
        for (const auto &e : elements)
            actions.push_back({e.attribute, e.storage, e.offset});

        auto attr_index = [](vertex_attr attribute) -> GLuint {
            switch (attribute) {
            case vertex_attr::position:   return 0;
            case vertex_attr::color_attr: return 1;
            case vertex_attr::tex_coord:  return 2;
            }
            return 0;
        };

        auto component_count = [](vertex_storage storage) -> int {
            switch (storage) {
            case vertex_storage::float_2: return 2;
            case vertex_storage::float_3: return 3;
            case vertex_storage::float_4: return 4;
            }
            return 2;
        };

        auto setup = [actions, attr_index, component_count](const void *base, int stride, bool shader_active) {
            for (const auto &a : actions) {
                const char *ptr = static_cast<const char *>(base) + a.offset;
                if (shader_active) {
                    const GLuint idx = attr_index(a.attribute);
                    ogl_s_glEnableVertexAttribArray(idx);
                    ogl_s_glVertexAttribPointer(
                        idx, component_count(a.storage), GL_FLOAT, GL_FALSE, stride, ptr
                    );
                    continue;
                }
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

        auto teardown = [actions, attr_index](bool shader_active) {
            for (const auto &a : actions) {
                if (shader_active) {
                    ogl_s_glDisableVertexAttribArray(attr_index(a.attribute));
                    continue;
                }
                switch (a.attribute) {
                case vertex_attr::position:   glDisableClientState(GL_VERTEX_ARRAY);        break;
                case vertex_attr::color_attr: glDisableClientState(GL_COLOR_ARRAY);         break;
                case vertex_attr::tex_coord:  glDisableClientState(GL_TEXTURE_COORD_ARRAY); break;
                }
            }
        };

        auto [ins, _] = cache.emplace(vtx_type, ogl_compiled_vtx{std::move(setup), std::move(teardown)});
        s_last_type = vtx_type;
        s_last_compiled = &std::any_cast<const ogl_compiled_vtx &>(ins->second);
        return *s_last_compiled;
    }

    // ── Helpers ──────────────────────────────────────────────────────────

    static void setup_matrices() {
        float transform[16], projection[16];
        get_current_transform_matrix(std::span<float, 16>(transform, 16));
        get_current_projection_matrix(std::span<float, 16>(projection, 16));
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(projection);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(transform);
    }

    static GLenum to_gl_mode(prim_type type) {
        switch (type) {
        case prim_type::triangle_list:  return GL_TRIANGLES;
        case prim_type::triangle_strip: return GL_TRIANGLE_STRIP;
        case prim_type::triangle_fan:   return GL_TRIANGLE_FAN;
        }
        return GL_TRIANGLES;
    }

    static bool has_vertex_color(std::span<const vertex_element> elements) {
        for (const auto &e : elements)
            if (e.attribute == vertex_attr::color_attr)
                return true;
        return false;
    }

    static void apply_ogl_alpha_blend() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    static void apply_ogl_alpha_mask_color() {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
    }

    // ── Drawing ──────────────────────────────────────────────────────────

    void ogl_draw_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        shader_program_handle *shader
    ) {
        if (count < 3)
            return;
        apply_ogl_alpha_blend();
        const auto &compiled = get_or_compile(vtx_type, elements);
        if (shader) {
            ogl_apply_shader_program(shader, nullptr);
            compiled.setup(vertices, stride, true);
            glDrawArrays(to_gl_mode(type), 0, count);
            compiled.teardown(true);
            return;
        }

        ogl_apply_shader_program(nullptr, nullptr);
        setup_matrices();
        compiled.setup(vertices, stride, false);
        glDrawArrays(to_gl_mode(type), 0, count);
        compiled.teardown(false);
    }

    void ogl_draw_indexed_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::span<const uint32_t> indices,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        shader_program_handle *shader
    ) {
        if (indices.size() < 3 || count == 0)
            return;
        apply_ogl_alpha_blend();
        const auto &compiled = get_or_compile(vtx_type, elements);
        if (shader) {
            ogl_apply_shader_program(shader, nullptr);
            compiled.setup(vertices, stride, true);
            glDrawElements(to_gl_mode(type), static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, indices.data());
            compiled.teardown(true);
            return;
        }

        ogl_apply_shader_program(nullptr, nullptr);
        setup_matrices();
        compiled.setup(vertices, stride, false);
        glDrawElements(to_gl_mode(type), static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, indices.data());
        compiled.teardown(false);
    }

    void ogl_draw_textured_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        texture_handle *tex,
        shader_program_handle *shader
    ) {
        if (count < 3 || !tex)
            return;
        auto *ogl_tex = as_ogl_texture(tex);
        apply_ogl_alpha_blend();
        const auto &compiled = get_or_compile(vtx_type, elements);
        if (shader) {
            ogl_apply_shader_program(shader, tex);
            compiled.setup(vertices, stride, true);
            glDrawArrays(to_gl_mode(type), 0, count);
            compiled.teardown(true);
            return;
        }

        ogl_apply_shader_program(nullptr, nullptr);
        setup_matrices();
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, ogl_tex->tex_id);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, has_vertex_color(elements) ? GL_MODULATE : GL_REPLACE);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        compiled.setup(vertices, stride, false);
        glDrawArrays(to_gl_mode(type), 0, count);
        compiled.teardown(false);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }

    void ogl_draw_alpha_masked_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        texture_handle *tex,
        shader_program_handle *shader
    ) {
        if (count < 3 || !tex)
            return;
        auto *ogl_tex = as_ogl_texture(tex);
        apply_ogl_alpha_blend();
        const auto &compiled = get_or_compile(vtx_type, elements);
        if (shader) {
            ogl_apply_shader_program(shader, tex);
            compiled.setup(vertices, stride, true);
            glDrawArrays(to_gl_mode(type), 0, count);
            compiled.teardown(true);
            return;
        }

        ogl_apply_shader_program(nullptr, nullptr);
        setup_matrices();
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, ogl_tex->tex_id);
        apply_ogl_alpha_mask_color();
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        compiled.setup(vertices, stride, false);
        glDrawArrays(to_gl_mode(type), 0, count);
        compiled.teardown(false);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }

    void ogl_draw_textured_indexed_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::span<const uint32_t> indices,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        texture_handle *tex,
        shader_program_handle *shader
    ) {
        if (indices.size() < 3 || count == 0 || !tex)
            return;
        auto *ogl_tex = as_ogl_texture(tex);
        apply_ogl_alpha_blend();
        const auto &compiled = get_or_compile(vtx_type, elements);
        if (shader) {
            ogl_apply_shader_program(shader, tex);
            compiled.setup(vertices, stride, true);
            glDrawElements(to_gl_mode(type), static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, indices.data());
            compiled.teardown(true);
            return;
        }

        ogl_apply_shader_program(nullptr, nullptr);
        setup_matrices();
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, ogl_tex->tex_id);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, has_vertex_color(elements) ? GL_MODULATE : GL_REPLACE);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        compiled.setup(vertices, stride, false);
        glDrawElements(to_gl_mode(type), static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, indices.data());
        compiled.teardown(false);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
