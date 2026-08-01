#ifndef ALIA_GFX_BACKEND_OGL_OPS_HPP
#define ALIA_GFX_BACKEND_OGL_OPS_HPP

#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include <GL/gl.h>
#include <GL/glext.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "../graphics_backend_interface.hpp"
#include "ogl_platform.hpp"

namespace alia {
    struct ogl_compiled_vertex_definition {
        std::function<void(const void *, int, bool)> setup;
        std::function<void(bool)> teardown;
    };
    struct ogl_pipeline;
    struct ogl_vertex_buffer;
    struct ogl_index_buffer;
    struct ogl_device : device_handle {
        void *ctx = nullptr;
        std::vector<std::optional<ogl_compiled_vertex_definition>> vertex_definitions;
        ogl_pipeline *current_pipeline = nullptr;
        ogl_vertex_buffer *current_vb = nullptr;
        ogl_index_buffer *current_ib = nullptr;
        const void *transient_vertices = nullptr;
        int transient_vertex_bytes = 0;
        const uint32_t *transient_indices = nullptr;
        int transient_index_count = 0;
        bool pass_active = false;
        vec2i pass_size = {};
        bool pass_has_depth = false;
        const ogl_compiled_vertex_definition *applied_layout = nullptr;
        bool applied_shader_active = false;
        const void *applied_base = nullptr;
        GLuint pass_fbo = 0;
    };
    struct ogl_texture : texture_handle {
        GLuint tex_id = 0;
        int width = 0, height = 0, mip_levels = 1;
        pixel_format fmt = pixel_format::rgba8888;
        texture_role role = texture_role::color;
        texture_usage usage = texture_usage::sampling_only;
        sampler_state sampler = {};
        std::unique_ptr<std::byte[]> stage_buf;
        std::size_t stage_buf_bytes = 0;
    };
    struct ogl_vertex_buffer : vertex_buffer_handle { GLuint buffer_id = 0; int stride = 0, count = 0; buffer_usage usage = buffer_usage::static_mesh; };
    struct ogl_index_buffer : index_buffer_handle { GLuint buffer_id = 0; int count = 0; buffer_usage usage = buffer_usage::static_mesh; };
    struct ogl_swapchain : swapchain_handle { void *native = nullptr; void *surface = nullptr; void *ctx = nullptr; vec2i size = {}; };
    struct ogl_stored_shader_constant {
        shader_constant_slot slot = {};
        shader_constant_value_type type = shader_constant_value_type::float_1;
        std::vector<float> floats;
        std::vector<int> ints;
    };
    struct ogl_stored_shader_sampler { GLint location = -1; int unit = 0; texture_handle *texture = nullptr; };
    struct ogl_shader_program : shader_program_handle {
        GLuint program = 0;
        std::unordered_map<std::string, int> sampler_units;
        std::vector<ogl_stored_shader_constant> stored_constants;
        std::vector<ogl_stored_shader_sampler> stored_samplers;
    };
    struct ogl_pipeline : pipeline_handle {
        ogl_shader_program *shader = nullptr;
        const basic_effect *effect = nullptr;
        vertex_definition_view layout = {};
        blend_state blend = {};
        depth_state depth = {};
        raster_state raster = {};
    };

    inline ogl_device *as_ogl_device(device_handle *h) { return static_cast<ogl_device *>(h); }
    inline ogl_texture *as_ogl_texture(texture_handle *h) { return static_cast<ogl_texture *>(h); }
    inline const ogl_texture *as_ogl_texture(const texture_handle *h) { return static_cast<const ogl_texture *>(h); }
    inline ogl_vertex_buffer *as_ogl_vertex_buffer(vertex_buffer_handle *h) { return static_cast<ogl_vertex_buffer *>(h); }
    inline const ogl_vertex_buffer *as_ogl_vertex_buffer(const vertex_buffer_handle *h) { return static_cast<const ogl_vertex_buffer *>(h); }
    inline ogl_index_buffer *as_ogl_index_buffer(index_buffer_handle *h) { return static_cast<ogl_index_buffer *>(h); }
    inline const ogl_index_buffer *as_ogl_index_buffer(const index_buffer_handle *h) { return static_cast<const ogl_index_buffer *>(h); }
    inline ogl_swapchain *as_ogl_swapchain(swapchain_handle *h) { return static_cast<ogl_swapchain *>(h); }
    inline ogl_shader_program *as_ogl_shader_program(shader_program_handle *h) { return static_cast<ogl_shader_program *>(h); }
    inline ogl_pipeline *as_ogl_pipeline(pipeline_handle *h) { return static_cast<ogl_pipeline *>(h); }

    ogl_device *ogl_create_device(); void ogl_destroy_device(device_handle *);
    texture_handle *ogl_create_texture(device_handle *, pixel_format, vec2i, int, texture_role, texture_usage); void ogl_destroy_texture(texture_handle *);
    pixel_format ogl_texture_format(const texture_handle *); int ogl_texture_width(const texture_handle *); int ogl_texture_height(const texture_handle *); int ogl_texture_mip_levels(const texture_handle *);
    sampler_state ogl_texture_sampler(const texture_handle *); void ogl_texture_set_sampler(texture_handle *, const sampler_state &);
    bool ogl_texture_lock(texture_handle *, rect_i, int, texture_lock_mode, texture_lock_info &); void ogl_texture_unlock(texture_handle *, const texture_lock_info &, bool);
    void ogl_texture_generate_mipmaps(texture_handle *); texture_handle *ogl_texture_clone(const texture_handle *);
    bool ogl_copy_render_target_to_texture(device_handle *, texture_handle *, rect_i, vec2i, vec2i, int);
    vertex_buffer_handle *ogl_create_vertex_buffer(device_handle *, int, int, buffer_usage, const void *); void ogl_destroy_vertex_buffer(vertex_buffer_handle *);
    int ogl_vertex_buffer_count(const vertex_buffer_handle *); int ogl_vertex_buffer_stride(const vertex_buffer_handle *); buffer_usage ogl_vertex_buffer_usage(const vertex_buffer_handle *);
    bool ogl_vertex_buffer_lock(vertex_buffer_handle *, int, int, buffer_lock_mode, buffer_lock_info &); void ogl_vertex_buffer_unlock(vertex_buffer_handle *, const buffer_lock_info &, bool);
    index_buffer_handle *ogl_create_index_buffer(device_handle *, int, buffer_usage, const uint32_t *); void ogl_destroy_index_buffer(index_buffer_handle *);
    int ogl_index_buffer_count(const index_buffer_handle *); buffer_usage ogl_index_buffer_usage(const index_buffer_handle *);
    bool ogl_index_buffer_lock(index_buffer_handle *, int, int, buffer_lock_mode, buffer_lock_info &); void ogl_index_buffer_unlock(index_buffer_handle *, const buffer_lock_info &, bool);
    shader_program_handle *ogl_create_shader_program(device_handle *, const shader_program_desc &); void ogl_destroy_shader_program(shader_program_handle *);
    shader_constant_slot ogl_shader_lookup_constant(shader_program_handle *, std::string_view, shader_type); void ogl_shader_set_constant(shader_program_handle *, const shader_constant_slot &, const shader_constant_payload &);
    shader_sampler_slot ogl_shader_lookup_sampler(shader_program_handle *, std::string_view, shader_type); void ogl_shader_set_sampler(shader_program_handle *, const shader_sampler_slot &, texture_handle *);
    void ogl_apply_program_state(ogl_shader_program *);
    swapchain_handle *ogl_create_swapchain(device_handle *, void *, vec2i); void ogl_destroy_swapchain(swapchain_handle *);
    void ogl_swapchain_begin_frame(swapchain_handle *); void ogl_swapchain_end_frame(swapchain_handle *); void ogl_swapchain_present(swapchain_handle *); void ogl_swapchain_on_resize(swapchain_handle *, vec2i);
    pipeline_handle *ogl_create_pipeline(device_handle *, const pipeline_desc &); void ogl_destroy_pipeline(pipeline_handle *); void ogl_update_pipeline(pipeline_handle *, const pipeline_desc &); void ogl_bind_pipeline(device_handle *, pipeline_handle *);
    bool ogl_begin_render_pass(device_handle *, const render_pass_begin_info &); void ogl_end_render_pass(device_handle *); void ogl_set_viewport(device_handle *, const render_viewport &);
    void ogl_bind_vertex_buffer(device_handle *, vertex_buffer_handle *); void ogl_bind_index_buffer(device_handle *, index_buffer_handle *);
    void ogl_upload_transient_vertex_data(device_handle *, const void *, int); void ogl_upload_transient_index_data(device_handle *, std::span<const uint32_t>);
    void ogl_bind_resources(device_handle *, const texture_sampler_binding &); void ogl_draw(device_handle *, primitive_topology, int, int); void ogl_draw_indexed(device_handle *, primitive_topology, int, int, int);

    extern PFNGLGENERATEMIPMAPPROC ogl_s_glGenerateMipmap;
    extern PFNGLCREATESHADERPROC ogl_s_glCreateShader; extern PFNGLSHADERSOURCEPROC ogl_s_glShaderSource; extern PFNGLCOMPILESHADERPROC ogl_s_glCompileShader; extern PFNGLGETSHADERIVPROC ogl_s_glGetShaderiv; extern PFNGLGETSHADERINFOLOGPROC ogl_s_glGetShaderInfoLog; extern PFNGLDELETESHADERPROC ogl_s_glDeleteShader;
    extern PFNGLCREATEPROGRAMPROC ogl_s_glCreateProgram; extern PFNGLATTACHSHADERPROC ogl_s_glAttachShader; extern PFNGLBINDATTRIBLOCATIONPROC ogl_s_glBindAttribLocation; extern PFNGLLINKPROGRAMPROC ogl_s_glLinkProgram; extern PFNGLGETPROGRAMIVPROC ogl_s_glGetProgramiv; extern PFNGLGETPROGRAMINFOLOGPROC ogl_s_glGetProgramInfoLog; extern PFNGLDELETEPROGRAMPROC ogl_s_glDeleteProgram; extern PFNGLUSEPROGRAMPROC ogl_s_glUseProgram;
    extern PFNGLGETUNIFORMLOCATIONPROC ogl_s_glGetUniformLocation; extern PFNGLUNIFORM1FPROC ogl_s_glUniform1f; extern PFNGLUNIFORM2FPROC ogl_s_glUniform2f; extern PFNGLUNIFORM3FPROC ogl_s_glUniform3f; extern PFNGLUNIFORM4FPROC ogl_s_glUniform4f; extern PFNGLUNIFORM1IPROC ogl_s_glUniform1i; extern PFNGLUNIFORM2IPROC ogl_s_glUniform2i; extern PFNGLUNIFORM3IPROC ogl_s_glUniform3i; extern PFNGLUNIFORM4IPROC ogl_s_glUniform4i; extern PFNGLUNIFORMMATRIX4FVPROC ogl_s_glUniformMatrix4fv;
    extern PFNGLACTIVETEXTUREPROC ogl_s_glActiveTexture; extern PFNGLENABLEVERTEXATTRIBARRAYPROC ogl_s_glEnableVertexAttribArray; extern PFNGLDISABLEVERTEXATTRIBARRAYPROC ogl_s_glDisableVertexAttribArray; extern PFNGLVERTEXATTRIBPOINTERPROC ogl_s_glVertexAttribPointer;
    extern PFNGLGENBUFFERSPROC ogl_s_glGenBuffers; extern PFNGLDELETEBUFFERSPROC ogl_s_glDeleteBuffers; extern PFNGLBINDBUFFERPROC ogl_s_glBindBuffer; extern PFNGLBUFFERDATAPROC ogl_s_glBufferData; extern PFNGLMAPBUFFERPROC ogl_s_glMapBuffer; extern PFNGLUNMAPBUFFERPROC ogl_s_glUnmapBuffer;
    extern PFNGLGENFRAMEBUFFERSPROC ogl_s_glGenFramebuffers; extern PFNGLDELETEFRAMEBUFFERSPROC ogl_s_glDeleteFramebuffers; extern PFNGLBINDFRAMEBUFFERPROC ogl_s_glBindFramebuffer; extern PFNGLFRAMEBUFFERTEXTURE2DPROC ogl_s_glFramebufferTexture2D; extern PFNGLCHECKFRAMEBUFFERSTATUSPROC ogl_s_glCheckFramebufferStatus;
} // namespace alia
#endif
#endif
