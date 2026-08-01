#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace alia {
    namespace {
        const ogl_compiled_vertex_definition &get_or_compile(ogl_device &device, const vertex_definition_view &definition) {
            if (device.vertex_definitions.size() <= definition.index)
                device.vertex_definitions.resize(definition.index + 1);
            auto &slot = device.vertex_definitions[definition.index];
            if (slot) return *slot;
            struct action { vertex_attr attr; vertex_storage storage; int offset; };
            std::vector<action> actions;
            for (const auto &element : definition.elements) actions.push_back({element.attribute, element.storage, element.offset});
            const auto attribute_index = [](vertex_attr attr) -> GLuint {
                switch (attr) { case vertex_attr::position: return 0; case vertex_attr::color_attr: return 1; case vertex_attr::tex_coord: return 2; case vertex_attr::normal: return 3; }
                return 0;
            };
            const auto components = [](vertex_storage storage) { return storage == vertex_storage::float_2 ? 2 : storage == vertex_storage::float_3 ? 3 : 4; };
            auto setup = [actions, attribute_index, components](const void *base, int stride, bool shader) {
                for (const auto &action : actions) {
                    const void *ptr = reinterpret_cast<const void *>(reinterpret_cast<std::uintptr_t>(base) + static_cast<std::uintptr_t>(action.offset));
                    if (shader) {
                        const GLuint index = attribute_index(action.attr);
                        ogl_s_glEnableVertexAttribArray(index);
                        ogl_s_glVertexAttribPointer(index, components(action.storage), GL_FLOAT, GL_FALSE, stride, ptr);
                        continue;
                    }
                    switch (action.attr) {
                    case vertex_attr::position: glEnableClientState(GL_VERTEX_ARRAY); glVertexPointer(components(action.storage), GL_FLOAT, stride, ptr); break;
                    case vertex_attr::normal: glEnableClientState(GL_NORMAL_ARRAY); glNormalPointer(GL_FLOAT, stride, ptr); break;
                    case vertex_attr::color_attr: glEnableClientState(GL_COLOR_ARRAY); glColorPointer(4, GL_FLOAT, stride, ptr); break;
                    case vertex_attr::tex_coord: glEnableClientState(GL_TEXTURE_COORD_ARRAY); glTexCoordPointer(2, GL_FLOAT, stride, ptr); break;
                    }
                }
            };
            auto teardown = [actions, attribute_index](bool shader) {
                for (const auto &action : actions) {
                    if (shader) { ogl_s_glDisableVertexAttribArray(attribute_index(action.attr)); continue; }
                    switch (action.attr) {
                    case vertex_attr::position: glDisableClientState(GL_VERTEX_ARRAY); break;
                    case vertex_attr::normal: glDisableClientState(GL_NORMAL_ARRAY); break;
                    case vertex_attr::color_attr: glDisableClientState(GL_COLOR_ARRAY); break;
                    case vertex_attr::tex_coord: glDisableClientState(GL_TEXTURE_COORD_ARRAY); break;
                    }
                }
            };
            slot = std::make_unique<ogl_compiled_vertex_definition>(
                ogl_compiled_vertex_definition{std::move(setup), std::move(teardown)});
            return *slot;
        }
        GLenum to_gl(primitive_topology topology) {
            switch (topology) { case primitive_topology::triangle_list: return GL_TRIANGLES; case primitive_topology::triangle_strip: return GL_TRIANGLE_STRIP; case primitive_topology::triangle_fan: return GL_TRIANGLE_FAN; }
            return GL_TRIANGLES;
        }
        GLenum to_gl(blend_factor factor) { switch (factor) { case blend_factor::zero: return GL_ZERO; case blend_factor::one: return GL_ONE; case blend_factor::src_alpha: return GL_SRC_ALPHA; case blend_factor::inv_src_alpha: return GL_ONE_MINUS_SRC_ALPHA; } return GL_ONE; }
        GLenum to_gl(compare_func compare) {
            switch (compare) { case compare_func::never: return GL_NEVER; case compare_func::less: return GL_LESS; case compare_func::equal: return GL_EQUAL; case compare_func::less_equal: return GL_LEQUAL; case compare_func::greater: return GL_GREATER; case compare_func::not_equal: return GL_NOTEQUAL; case compare_func::greater_equal: return GL_GEQUAL; case compare_func::always: return GL_ALWAYS; }
            return GL_LEQUAL;
        }
        void bind_texture_unit(int unit, texture_handle *texture) {
            if (ogl_s_glActiveTexture) ogl_s_glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, texture ? as_ogl_texture(texture)->tex_id : 0);
        }
        void apply_sampler(const ogl_texture &texture, const sampler_state &state) {
            const auto filter = [](texture_filter value) { return value == texture_filter::nearest ? GL_NEAREST : GL_LINEAR; };
            const auto wrap = [](texture_wrap value) { switch (value) { case texture_wrap::clamp: return GL_CLAMP_TO_EDGE; case texture_wrap::repeat: return GL_REPEAT; case texture_wrap::mirror: return GL_MIRRORED_REPEAT; } return GL_CLAMP_TO_EDGE; };
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture.mip_levels <= 1 ? filter(state.min_filter) : (state.min_filter == texture_filter::nearest ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter(state.mag_filter));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap(state.wrap_u)); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap(state.wrap_v));
        }
        void apply_texture_op(texture_operation operation) {
            switch (operation) {
            case texture_operation::vertex_color: glDisable(GL_TEXTURE_2D); break;
            case texture_operation::replace: glEnable(GL_TEXTURE_2D); glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); glColor4f(1, 1, 1, 1); break;
            case texture_operation::modulate: glEnable(GL_TEXTURE_2D); glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); glColor4f(1, 1, 1, 1); break;
            case texture_operation::alpha_mask:
                glEnable(GL_TEXTURE_2D); glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE); glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR); glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE); glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA); glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR); glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA); glColor4f(1, 1, 1, 1); break;
            }
        }
        void clear_applied_layout(ogl_device &device) {
            if (device.applied_layout) device.applied_layout->teardown(device.applied_shader_active);
            device.applied_layout = nullptr; device.applied_base = nullptr; device.applied_shader_active = false;
        }
        void apply_layout(ogl_device &device, const void *base) {
            const auto &layout = get_or_compile(device, device.current_pipeline->layout);
            const bool shader = device.current_pipeline->shader != nullptr;
            if (&layout == device.applied_layout && shader == device.applied_shader_active && base == device.applied_base) return;
            clear_applied_layout(device);
            if (ogl_s_glBindBuffer) ogl_s_glBindBuffer(GL_ARRAY_BUFFER, device.current_vb ? device.current_vb->buffer_id : 0);
            layout.setup(base, device.current_pipeline->layout.stride, shader);
            device.applied_layout = &layout; device.applied_shader_active = shader; device.applied_base = base;
        }
        void prepare_draw(ogl_device &device) {
            if (device.current_pipeline->shader) {
                ogl_apply_program_state(device.current_pipeline->shader);
                return;
            }
            glMatrixMode(GL_PROJECTION); glLoadMatrixf(&device.current_pipeline->effect->projection.m[0][0]);
            glMatrixMode(GL_MODELVIEW); glLoadMatrixf(&device.current_pipeline->effect->world.m[0][0]);
        }
        bool fbo_available() { return ogl_s_glGenFramebuffers && ogl_s_glBindFramebuffer && ogl_s_glFramebufferTexture2D && ogl_s_glCheckFramebufferStatus; }
    }

    pipeline_handle *ogl_create_pipeline(device_handle *, const pipeline_desc &desc) { auto *pipeline = new ogl_pipeline; ogl_update_pipeline(pipeline, desc); return pipeline; }
    void ogl_destroy_pipeline(pipeline_handle *h) { delete as_ogl_pipeline(h); }
    void ogl_update_pipeline(pipeline_handle *h, const pipeline_desc &desc) {
        auto *pipeline = as_ogl_pipeline(h); pipeline->shader = desc.shader ? as_ogl_shader_program(desc.shader) : nullptr; pipeline->effect = desc.effect; pipeline->layout = desc.vertex_layout; pipeline->blend = desc.blend; pipeline->depth = desc.depth; pipeline->raster = desc.raster;
    }
    void ogl_bind_pipeline(device_handle *h, pipeline_handle *pipeline_h) {
        auto &device = *as_ogl_device(h); auto *pipeline = as_ogl_pipeline(pipeline_h);
        if (ogl_s_glUseProgram) ogl_s_glUseProgram(pipeline->shader ? pipeline->shader->program : 0);
        if (pipeline->depth.test_enabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glDepthMask(pipeline->depth.write_enabled ? GL_TRUE : GL_FALSE); glDepthFunc(to_gl(pipeline->depth.compare));
        if (pipeline->blend.enabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        glBlendFunc(to_gl(pipeline->blend.src), to_gl(pipeline->blend.dst));
        if (pipeline->raster.cull == cull_mode::none) glDisable(GL_CULL_FACE);
        else { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(pipeline->raster.cull == cull_mode::clockwise ? GL_CCW : GL_CW); }
        if (!pipeline->shader) {
            glDisable(GL_LIGHTING);
            apply_texture_op(pipeline->effect->texture_op);
        }
        device.current_pipeline = pipeline;
    }
    bool ogl_set_render_target(device_handle *h, const render_target_info &info) {
        auto &device = *as_ogl_device(h);
        if (info.target_texture) {
            if (!fbo_available()) return false;
            if (!device.target_fbo) ogl_s_glGenFramebuffers(1, &device.target_fbo);
            if (!device.target_fbo) return false;
            ogl_s_glBindFramebuffer(GL_FRAMEBUFFER, device.target_fbo);
            ogl_s_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, as_ogl_texture(info.target_texture)->tex_id, info.target_level);
            if (ogl_s_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { ogl_s_glBindFramebuffer(GL_FRAMEBUFFER, 0); return false; }
        } else if (fbo_available()) {
            ogl_s_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        glViewport(0, 0, info.target_size.x, info.target_size.y);
        return true;
    }
    bool ogl_clear(device_handle *, const std::optional<color> &clear_color, const std::optional<float> &clear_depth) {
        GLbitfield mask = 0;
        if (clear_color) { glClearColor(clear_color->r, clear_color->g, clear_color->b, clear_color->a); mask |= GL_COLOR_BUFFER_BIT; }
        if (clear_depth) { GLboolean write_mask = GL_FALSE; glGetBooleanv(GL_DEPTH_WRITEMASK, &write_mask); glDepthMask(GL_TRUE); glClearDepth(*clear_depth); mask |= GL_DEPTH_BUFFER_BIT; glClear(mask); glDepthMask(write_mask); mask = 0; }
        if (mask) glClear(mask);
        return true;
    }
    void ogl_reset_frame_state(ogl_device &device) {
        clear_applied_layout(device);
        if (ogl_s_glBindBuffer) { ogl_s_glBindBuffer(GL_ARRAY_BUFFER, 0); ogl_s_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }
        if (fbo_available()) ogl_s_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        device.current_pipeline = nullptr; device.current_vb = nullptr; device.current_ib = nullptr; device.transient_vertices = nullptr; device.transient_vertex_bytes = 0; device.transient_indices = nullptr; device.transient_index_count = 0;
    }
    void ogl_set_viewport(device_handle *, const render_viewport &viewport) { glViewport(viewport.origin.x, viewport.origin.y, viewport.size.x, viewport.size.y); glDepthRange(viewport.min_depth, viewport.max_depth); }
    void ogl_bind_vertex_buffer(device_handle *h, vertex_buffer_handle *buffer) { auto &device = *as_ogl_device(h); device.current_vb = buffer ? as_ogl_vertex_buffer(buffer) : nullptr; device.transient_vertices = nullptr; device.transient_vertex_bytes = 0; }
    void ogl_bind_index_buffer(device_handle *h, index_buffer_handle *buffer) { auto &device = *as_ogl_device(h); device.current_ib = buffer ? as_ogl_index_buffer(buffer) : nullptr; device.transient_indices = nullptr; device.transient_index_count = 0; }
    void ogl_upload_transient_vertex_data(device_handle *h, const void *data, int bytes) { auto &device = *as_ogl_device(h); device.current_vb = nullptr; device.transient_vertices = data; device.transient_vertex_bytes = bytes; }
    void ogl_upload_transient_index_data(device_handle *h, std::span<const uint32_t> indices) { auto &device = *as_ogl_device(h); device.current_ib = nullptr; device.transient_indices = indices.data(); device.transient_index_count = static_cast<int>(indices.size()); }
    void ogl_bind_resources(device_handle *, const texture_sampler_binding &binding) {
        bind_texture_unit(binding.slot, binding.texture);
        if (binding.texture) apply_sampler(*as_ogl_texture(binding.texture), binding.sampler);
        if (ogl_s_glActiveTexture) ogl_s_glActiveTexture(GL_TEXTURE0);
    }
    void ogl_draw(device_handle *h, primitive_topology topology, int vertex_count, int first_vertex) {
        auto &device = *as_ogl_device(h); if (vertex_count < 3) return;
        const void *base = device.current_vb ? nullptr : device.transient_vertices;
        apply_layout(device, base); prepare_draw(device); glDrawArrays(to_gl(topology), first_vertex, vertex_count);
    }
    void ogl_draw_indexed(device_handle *h, primitive_topology topology, int index_count, int first_index, int base_vertex) {
        auto &device = *as_ogl_device(h); if (index_count < 3) return;
        const int stride = device.current_pipeline->layout.stride;
        const void *base = device.current_vb ? reinterpret_cast<const void *>(static_cast<std::uintptr_t>(base_vertex * stride)) : static_cast<const std::byte *>(device.transient_vertices) + base_vertex * stride;
        apply_layout(device, base); prepare_draw(device);
        if (ogl_s_glBindBuffer) ogl_s_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, device.current_ib ? device.current_ib->buffer_id : 0);
        const void *indices = device.current_ib ? reinterpret_cast<const void *>(static_cast<std::uintptr_t>(first_index * sizeof(uint32_t))) : device.transient_indices + first_index;
        glDrawElements(to_gl(topology), index_count, GL_UNSIGNED_INT, indices);
    }
} // namespace alia

#endif
