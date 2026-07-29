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

    // ── Concrete device/texture/swapchain structs ─────────────────────────

    struct ogl_compiled_vertex_definition {
        std::function<void(const void *base, int stride, bool shader_active)>
            setup;
        std::function<void(bool shader_active)> teardown;
    };

    struct ogl_device : device_handle {
        void *ctx = nullptr; // opaque context handle (HGLRC on Win32)
        std::vector<std::optional<ogl_compiled_vertex_definition>>
            vertex_definitions;
    };

    struct ogl_texture : texture_handle {
        GLuint tex_id = 0;
        int width = 0;
        int height = 0;
        int mip_levels = 1;
        pixel_format fmt = pixel_format::rgba8888;
        texture_role role = texture_role::color;
        texture_usage usage = texture_usage::sampling_only;
        sampler_state sampler = {};

        // CPU staging buffer for lock/unlock
        std::unique_ptr<std::byte[]> stage_buf;
        std::size_t stage_buf_bytes = 0;
    };

    struct ogl_render_target_scope : render_target_scope_handle {
        GLint previous_framebuffer = 0;
        GLint previous_viewport[4] = {};
        GLuint framebuffer = 0;
    };

    struct ogl_vertex_buffer : vertex_buffer_handle {
        GLuint buffer_id = 0;
        int stride = 0;
        int count = 0;
        buffer_usage usage = buffer_usage::static_mesh;
    };

    struct ogl_index_buffer : index_buffer_handle {
        GLuint buffer_id = 0;
        int count = 0;
        buffer_usage usage = buffer_usage::static_mesh;
    };

    struct ogl_swapchain : swapchain_handle {
        void *native = nullptr;  // native window handle (for destroy_surface)
        void *surface = nullptr; // opaque surface handle (HDC on Win32)
        void *ctx = nullptr;     // non-owning ref to device context
        vec2i size = {};
    };

    struct ogl_stored_shader_constant {
        shader_constant_slot slot = {};
        shader_constant_value_type type = shader_constant_value_type::float_1;
        std::vector<float> floats;
        std::vector<int> ints;
    };

    struct ogl_shader_program : shader_program_handle {
        GLuint program = 0;
        std::unordered_map<std::string, int> sampler_units;
        std::vector<ogl_stored_shader_constant> stored_constants;
        std::unordered_map<int, texture_handle *> sampler_textures;
    };

    // ── Cast helpers ──────────────────────────────────────────────────────

    inline ogl_device *as_ogl_device(device_handle *h) {
        return static_cast<ogl_device *>(h);
    }
    inline ogl_texture *as_ogl_texture(texture_handle *h) {
        return static_cast<ogl_texture *>(h);
    }
    inline const ogl_texture *as_ogl_texture(const texture_handle *h) {
        return static_cast<const ogl_texture *>(h);
    }
    inline ogl_vertex_buffer *as_ogl_vertex_buffer(vertex_buffer_handle *h) {
        return static_cast<ogl_vertex_buffer *>(h);
    }
    inline const ogl_vertex_buffer *as_ogl_vertex_buffer(const vertex_buffer_handle *h) {
        return static_cast<const ogl_vertex_buffer *>(h);
    }
    inline ogl_index_buffer *as_ogl_index_buffer(index_buffer_handle *h) {
        return static_cast<ogl_index_buffer *>(h);
    }
    inline const ogl_index_buffer *as_ogl_index_buffer(const index_buffer_handle *h) {
        return static_cast<const ogl_index_buffer *>(h);
    }
    inline ogl_swapchain *as_ogl_swapchain(swapchain_handle *h) {
        return static_cast<ogl_swapchain *>(h);
    }
    inline ogl_shader_program *as_ogl_shader_program(shader_program_handle *h) {
        return static_cast<ogl_shader_program *>(h);
    }
    inline const ogl_shader_program *as_ogl_shader_program(const shader_program_handle *h) {
        return static_cast<const ogl_shader_program *>(h);
    }

    // ── Device ops ────────────────────────────────────────────────────────

    ogl_device *ogl_create_device();
    void ogl_destroy_device(device_handle *h);

    // ── Texture ops ───────────────────────────────────────────────────────

    texture_handle *ogl_create_texture(
        device_handle *dev,
        pixel_format fmt,
        vec2i size,
        int mip_levels,
        texture_role role,
        texture_usage usage
    );
    void ogl_destroy_texture(texture_handle *h);
    pixel_format ogl_texture_format(const texture_handle *h);
    int ogl_texture_width(const texture_handle *h);
    int ogl_texture_height(const texture_handle *h);
    int ogl_texture_mip_levels(const texture_handle *h);
    sampler_state ogl_texture_sampler(const texture_handle *h);
    void ogl_texture_set_sampler(texture_handle *h, const sampler_state &s);
    bool ogl_texture_lock(texture_handle *h, rect_i region, int level, texture_lock_mode mode, texture_lock_info &out);
    void ogl_texture_unlock(texture_handle *h, const texture_lock_info &info, bool wrote);
    void ogl_texture_generate_mipmaps(texture_handle *h);
    texture_handle *ogl_texture_clone(const texture_handle *h);
    render_target_scope_handle *ogl_texture_begin_render_target(device_handle *dev, texture_handle *h, int level);
    void ogl_texture_end_render_target(device_handle *dev, render_target_scope_handle *h);
    bool ogl_copy_render_target_to_texture(
        device_handle *dev,
        texture_handle *dst,
        rect_i src_rect,
        vec2i src_target_size,
        vec2i dst_pos,
        int dst_level
    );

    vertex_buffer_handle *ogl_create_vertex_buffer(
        device_handle *dev, int vertex_stride, int vertex_count, buffer_usage usage, const void *initial_data
    );
    void ogl_destroy_vertex_buffer(vertex_buffer_handle *h);
    int ogl_vertex_buffer_count(const vertex_buffer_handle *h);
    int ogl_vertex_buffer_stride(const vertex_buffer_handle *h);
    buffer_usage ogl_vertex_buffer_usage(const vertex_buffer_handle *h);
    bool ogl_vertex_buffer_lock(vertex_buffer_handle *h, int first_vertex, int vertex_count, buffer_lock_mode mode, buffer_lock_info &out);
    void ogl_vertex_buffer_unlock(vertex_buffer_handle *h, const buffer_lock_info &info, bool wrote);

    index_buffer_handle *ogl_create_index_buffer(
        device_handle *dev, int index_count, buffer_usage usage, const uint32_t *initial_data
    );
    void ogl_destroy_index_buffer(index_buffer_handle *h);
    int ogl_index_buffer_count(const index_buffer_handle *h);
    buffer_usage ogl_index_buffer_usage(const index_buffer_handle *h);
    bool ogl_index_buffer_lock(index_buffer_handle *h, int first_index, int index_count, buffer_lock_mode mode, buffer_lock_info &out);
    void ogl_index_buffer_unlock(index_buffer_handle *h, const buffer_lock_info &info, bool wrote);

    shader_program_handle *ogl_create_shader_program(device_handle *dev, const shader_program_desc &desc);
    void ogl_destroy_shader_program(shader_program_handle *h);
    shader_constant_slot ogl_shader_lookup_constant(shader_program_handle *h, std::string_view name, shader_type stage);
    void ogl_shader_set_constant(shader_program_handle *h, const shader_constant_slot &slot, const shader_constant_payload &payload);
    shader_sampler_slot ogl_shader_lookup_sampler(shader_program_handle *h, std::string_view name, shader_type stage);
    void ogl_shader_set_sampler(shader_program_handle *h, const shader_sampler_slot &slot, texture_handle *tex);
    void ogl_bind_shader_program(device_handle *dev, shader_program_handle *h);
    void ogl_apply_shader_constants(device_handle *dev, shader_program_handle *h);
    void ogl_apply_shader_samplers(device_handle *dev, shader_program_handle *h);

    // ── Swapchain ops ─────────────────────────────────────────────────────

    swapchain_handle *ogl_create_swapchain(device_handle *dev, void *native_handle, vec2i size);
    void ogl_destroy_swapchain(swapchain_handle *h);
    void ogl_swapchain_begin_frame(swapchain_handle *h);
    void ogl_swapchain_end_frame(swapchain_handle *h);
    void ogl_swapchain_present(swapchain_handle *h);
    void ogl_swapchain_on_resize(swapchain_handle *h, vec2i new_size);

    // ── Draw ops ──────────────────────────────────────────────────────────

    void ogl_set_viewport(device_handle *dev, const render_viewport &viewport);
    void ogl_clear_render_target(device_handle *dev, color c);
    void ogl_set_render_state(device_handle *dev, const render_state &state);
    void ogl_set_blend_state(device_handle *dev, const blend_state &state);
    void ogl_set_fixed_function_matrices(device_handle *dev, const fixed_function_matrices &matrices);
    void ogl_set_fixed_function_texture_mode(device_handle *dev, fixed_function_texture_mode mode);
    void ogl_bind_texture(device_handle *dev, const texture_binding &binding);
    void ogl_set_texture_sampler(device_handle *dev, const texture_sampler_binding &binding);
    void ogl_bind_vertex_input(device_handle *dev, const vertex_input_binding &binding);
    void ogl_unbind_vertex_input(device_handle *dev, const vertex_input_binding &binding);
    void ogl_draw_arrays_immediate(device_handle *dev, const draw_arrays_immediate &draw);
    void ogl_draw_elements_immediate(device_handle *dev, const draw_elements_immediate &draw);
    void ogl_draw_arrays_buffered(device_handle *dev, const draw_arrays_buffered &draw);
    void ogl_draw_elements_buffered(device_handle *dev, const draw_elements_buffered &draw);

    // ── glGenerateMipmap function pointer (loaded during device creation) ─
    extern PFNGLGENERATEMIPMAPPROC ogl_s_glGenerateMipmap;
    extern PFNGLCREATESHADERPROC ogl_s_glCreateShader;
    extern PFNGLSHADERSOURCEPROC ogl_s_glShaderSource;
    extern PFNGLCOMPILESHADERPROC ogl_s_glCompileShader;
    extern PFNGLGETSHADERIVPROC ogl_s_glGetShaderiv;
    extern PFNGLGETSHADERINFOLOGPROC ogl_s_glGetShaderInfoLog;
    extern PFNGLDELETESHADERPROC ogl_s_glDeleteShader;
    extern PFNGLCREATEPROGRAMPROC ogl_s_glCreateProgram;
    extern PFNGLATTACHSHADERPROC ogl_s_glAttachShader;
    extern PFNGLBINDATTRIBLOCATIONPROC ogl_s_glBindAttribLocation;
    extern PFNGLLINKPROGRAMPROC ogl_s_glLinkProgram;
    extern PFNGLGETPROGRAMIVPROC ogl_s_glGetProgramiv;
    extern PFNGLGETPROGRAMINFOLOGPROC ogl_s_glGetProgramInfoLog;
    extern PFNGLDELETEPROGRAMPROC ogl_s_glDeleteProgram;
    extern PFNGLUSEPROGRAMPROC ogl_s_glUseProgram;
    extern PFNGLGETUNIFORMLOCATIONPROC ogl_s_glGetUniformLocation;
    extern PFNGLUNIFORM1FPROC ogl_s_glUniform1f;
    extern PFNGLUNIFORM2FPROC ogl_s_glUniform2f;
    extern PFNGLUNIFORM3FPROC ogl_s_glUniform3f;
    extern PFNGLUNIFORM4FPROC ogl_s_glUniform4f;
    extern PFNGLUNIFORM1IPROC ogl_s_glUniform1i;
    extern PFNGLUNIFORM2IPROC ogl_s_glUniform2i;
    extern PFNGLUNIFORM3IPROC ogl_s_glUniform3i;
    extern PFNGLUNIFORM4IPROC ogl_s_glUniform4i;
    extern PFNGLUNIFORMMATRIX4FVPROC ogl_s_glUniformMatrix4fv;
    extern PFNGLACTIVETEXTUREPROC ogl_s_glActiveTexture;
    extern PFNGLENABLEVERTEXATTRIBARRAYPROC ogl_s_glEnableVertexAttribArray;
    extern PFNGLDISABLEVERTEXATTRIBARRAYPROC ogl_s_glDisableVertexAttribArray;
    extern PFNGLVERTEXATTRIBPOINTERPROC ogl_s_glVertexAttribPointer;
    extern PFNGLGENBUFFERSPROC ogl_s_glGenBuffers;
    extern PFNGLDELETEBUFFERSPROC ogl_s_glDeleteBuffers;
    extern PFNGLBINDBUFFERPROC ogl_s_glBindBuffer;
    extern PFNGLBUFFERDATAPROC ogl_s_glBufferData;
    extern PFNGLMAPBUFFERPROC ogl_s_glMapBuffer;
    extern PFNGLUNMAPBUFFERPROC ogl_s_glUnmapBuffer;
    extern PFNGLGENFRAMEBUFFERSPROC ogl_s_glGenFramebuffers;
    extern PFNGLDELETEFRAMEBUFFERSPROC ogl_s_glDeleteFramebuffers;
    extern PFNGLBINDFRAMEBUFFERPROC ogl_s_glBindFramebuffer;
    extern PFNGLFRAMEBUFFERTEXTURE2DPROC ogl_s_glFramebufferTexture2D;
    extern PFNGLCHECKFRAMEBUFFERSTATUSPROC ogl_s_glCheckFramebufferStatus;

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
#endif // ALIA_GFX_BACKEND_OGL_OPS_HPP
