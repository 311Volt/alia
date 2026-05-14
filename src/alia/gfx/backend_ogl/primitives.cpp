#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"
#include <GL/gl.h>
#include <any>
#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace alia {

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
            case vertex_attr::normal:     return 3;
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
                const auto ptr_value = reinterpret_cast<std::uintptr_t>(base) + static_cast<std::uintptr_t>(a.offset);
                const void *ptr = reinterpret_cast<const void *>(ptr_value);
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
                case vertex_attr::normal:
                    glEnableClientState(GL_NORMAL_ARRAY);
                    glNormalPointer(GL_FLOAT, stride, ptr);
                    break;
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
                case vertex_attr::normal:     glDisableClientState(GL_NORMAL_ARRAY);        break;
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

    static GLenum to_gl_mode(prim_type type) {
        switch (type) {
        case prim_type::triangle_list:  return GL_TRIANGLES;
        case prim_type::triangle_strip: return GL_TRIANGLE_STRIP;
        case prim_type::triangle_fan:   return GL_TRIANGLE_FAN;
        }
        return GL_TRIANGLES;
    }

    static GLenum to_gl_blend_factor(blend_factor factor) {
        switch (factor) {
        case blend_factor::zero:          return GL_ZERO;
        case blend_factor::one:           return GL_ONE;
        case blend_factor::src_alpha:     return GL_SRC_ALPHA;
        case blend_factor::inv_src_alpha: return GL_ONE_MINUS_SRC_ALPHA;
        }
        return GL_ONE;
    }

    static void bind_texture_unit(int unit, texture_handle *texture) {
        auto *ogl_texture = texture ? as_ogl_texture(texture) : nullptr;
        if (ogl_s_glActiveTexture)
            ogl_s_glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
        glBindTexture(GL_TEXTURE_2D, ogl_texture ? ogl_texture->tex_id : 0);
        if (ogl_s_glActiveTexture)
            ogl_s_glActiveTexture(GL_TEXTURE0);
    }

    static void apply_ogl_sampler(const ogl_texture &texture, const sampler_state &s) {
        glBindTexture(GL_TEXTURE_2D, texture.tex_id);

        auto min_filt = [&]() -> GLint {
            if (texture.mip_levels <= 1)
                return (s.min_filter == texture_filter::nearest) ? GL_NEAREST : GL_LINEAR;
            bool mn = (s.min_filter == texture_filter::nearest);
            bool mp = (s.mip_filter == texture_filter::nearest);
            if (mn && mp)  return GL_NEAREST_MIPMAP_NEAREST;
            if (mn && !mp) return GL_NEAREST_MIPMAP_LINEAR;
            if (!mn && mp) return GL_LINEAR_MIPMAP_NEAREST;
            return GL_LINEAR_MIPMAP_LINEAR;
        };
        auto mag_filt = [&]() -> GLint {
            return (s.mag_filter == texture_filter::nearest) ? GL_NEAREST : GL_LINEAR;
        };
        auto wrap_mode = [](texture_wrap w) -> GLint {
            switch (w) {
            case texture_wrap::clamp:  return GL_CLAMP_TO_EDGE;
            case texture_wrap::repeat: return GL_REPEAT;
            case texture_wrap::mirror: return GL_MIRRORED_REPEAT;
            }
            return GL_CLAMP_TO_EDGE;
        };

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filt());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filt());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode(s.wrap_u));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode(s.wrap_v));
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

    void ogl_set_viewport(device_handle *, const render_viewport &viewport) {
        glViewport(viewport.origin.x, viewport.origin.y, viewport.size.x, viewport.size.y);
    }

    void ogl_clear_render_target(device_handle *, color c) {
        glClearColor(c.r, c.g, c.b, c.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void ogl_set_render_state(device_handle *, const render_state &state) {
        if (state.depth_test_enabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
        glDepthMask(state.depth_write_enabled ? GL_TRUE : GL_FALSE);

        if (state.fixed_function_lighting_enabled)
            glEnable(GL_LIGHTING);
        else
            glDisable(GL_LIGHTING);

        if (state.cull == cull_mode::none) {
            glDisable(GL_CULL_FACE);
            glFrontFace(GL_CCW);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(state.cull == cull_mode::clockwise ? GL_CCW : GL_CW);
        }
    }

    void ogl_set_blend_state(device_handle *, const blend_state &state) {
        if (!state.enabled) {
            glDisable(GL_BLEND);
            return;
        }

        glEnable(GL_BLEND);
        glBlendFunc(to_gl_blend_factor(state.src), to_gl_blend_factor(state.dst));
    }

    void ogl_set_fixed_function_matrices(device_handle *, const fixed_function_matrices &matrices) {
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(matrices.projection.data());
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(matrices.world.data());
    }

    void ogl_set_fixed_function_texture_mode(device_handle *, fixed_function_texture_mode mode) {
        switch (mode) {
        case fixed_function_texture_mode::vertex_color:
            glDisable(GL_TEXTURE_2D);
            break;
        case fixed_function_texture_mode::texture_replace:
            glEnable(GL_TEXTURE_2D);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            break;
        case fixed_function_texture_mode::texture_modulate_vertex_color:
            glEnable(GL_TEXTURE_2D);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            break;
        case fixed_function_texture_mode::alpha_mask:
            glEnable(GL_TEXTURE_2D);
            apply_ogl_alpha_mask_color();
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            break;
        }
    }

    void ogl_bind_texture(device_handle *, const texture_binding &binding) {
        bind_texture_unit(binding.slot, binding.texture);
    }

    void ogl_set_texture_sampler(device_handle *, const texture_sampler_binding &binding) {
        if (!binding.texture)
            return;
        apply_ogl_sampler(*as_ogl_texture(binding.texture), binding.sampler);
    }

    void ogl_bind_vertex_input(device_handle *, const vertex_input_binding &binding) {
        if (ogl_s_glBindBuffer) {
            const GLuint buffer_id = binding.buffer ? as_ogl_vertex_buffer(binding.buffer)->buffer_id : 0;
            ogl_s_glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
        }
        const auto &compiled = get_or_compile(binding.vertex_type, binding.elements);
        const void *base = binding.buffer
            ? reinterpret_cast<const void *>(static_cast<std::uintptr_t>(binding.vertex_offset_bytes))
            : binding.vertices;
        compiled.setup(base, binding.stride, binding.mode == vertex_input_mode::shader_attributes);
    }

    void ogl_unbind_vertex_input(device_handle *, const vertex_input_binding &binding) {
        const auto &compiled = get_or_compile(binding.vertex_type, binding.elements);
        compiled.teardown(binding.mode == vertex_input_mode::shader_attributes);
        if (ogl_s_glBindBuffer)
            ogl_s_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void ogl_draw_arrays_immediate(device_handle *, const draw_arrays_immediate &draw) {
        glDrawArrays(to_gl_mode(draw.type), 0, draw.vertex_count);
    }

    void ogl_draw_elements_immediate(device_handle *, const draw_elements_immediate &draw) {
        if (ogl_s_glBindBuffer)
            ogl_s_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDrawElements(
            to_gl_mode(draw.type),
            static_cast<GLsizei>(draw.indices.size()),
            GL_UNSIGNED_INT,
            draw.indices.data()
        );
    }

    void ogl_draw_arrays_buffered(device_handle *, const draw_arrays_buffered &draw) {
        glDrawArrays(to_gl_mode(draw.type), draw.first_vertex, draw.vertex_count);
    }

    void ogl_draw_elements_buffered(device_handle *, const draw_elements_buffered &draw) {
        auto *indices = as_ogl_index_buffer(draw.indices);
        ogl_s_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices->buffer_id);
        const auto offset = static_cast<std::uintptr_t>(draw.first_index) * sizeof(uint32_t);
        glDrawElements(
            to_gl_mode(draw.type),
            static_cast<GLsizei>(draw.index_count),
            GL_UNSIGNED_INT,
            reinterpret_cast<const void *>(offset)
        );
        ogl_s_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
