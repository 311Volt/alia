#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"
#include "../gfx_device.hpp"

#include <GL/gl.h>
#include <cstdint>
#include <cstring>

#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
#define WIN32_LEAN_AND_MEAN

    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    
#include <windows.h>
namespace alia {
    void register_win32_ogl_platform();
}
#endif

namespace alia {

    // ── GL version / extension probing ───────────────────────────────────
    // Called after the context is current so glGetString works.

    static bool parse_gl_version(int &major, int &minor) {
        const char *ver = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        if (!ver)
            return false;
        // Format: "major.minor ..."
        return std::sscanf(ver, "%d.%d", &major, &minor) == 2;
    }

    static bool has_gl_extension(const char *name) {
        const char *exts = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
        if (!exts)
            return false;
        const std::size_t len = std::strlen(name);
        const char *p = exts;
        while ((p = std::strstr(p, name)) != nullptr) {
            // Ensure it's a whole word (delimited by space or end-of-string)
            if ((p == exts || p[-1] == ' ') && (p[len] == ' ' || p[len] == '\0'))
                return true;
            p += len;
        }
        return false;
    }

    static void *load_gl_proc_raw(const char *name) {
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
        void *p = reinterpret_cast<void *>(wglGetProcAddress(name));
        if (p == reinterpret_cast<void *>(0x1) ||
            p == reinterpret_cast<void *>(0x2) ||
            p == reinterpret_cast<void *>(0x3) ||
            p == reinterpret_cast<void *>(static_cast<std::intptr_t>(-1)))
            return nullptr;
        return p;
#else
        (void)name;
        return nullptr;
#endif
    }

    template <typename T>
    static bool load_gl_proc(T &target, const char *name) {
        target = reinterpret_cast<T>(load_gl_proc_raw(name));
        return target != nullptr;
    }

    static bool load_shader_procs() {
        bool ok = true;
        ok = load_gl_proc(ogl_s_glCreateShader, "glCreateShader") && ok;
        ok = load_gl_proc(ogl_s_glShaderSource, "glShaderSource") && ok;
        ok = load_gl_proc(ogl_s_glCompileShader, "glCompileShader") && ok;
        ok = load_gl_proc(ogl_s_glGetShaderiv, "glGetShaderiv") && ok;
        ok = load_gl_proc(ogl_s_glGetShaderInfoLog, "glGetShaderInfoLog") && ok;
        ok = load_gl_proc(ogl_s_glDeleteShader, "glDeleteShader") && ok;
        ok = load_gl_proc(ogl_s_glCreateProgram, "glCreateProgram") && ok;
        ok = load_gl_proc(ogl_s_glAttachShader, "glAttachShader") && ok;
        ok = load_gl_proc(ogl_s_glBindAttribLocation, "glBindAttribLocation") && ok;
        ok = load_gl_proc(ogl_s_glLinkProgram, "glLinkProgram") && ok;
        ok = load_gl_proc(ogl_s_glGetProgramiv, "glGetProgramiv") && ok;
        ok = load_gl_proc(ogl_s_glGetProgramInfoLog, "glGetProgramInfoLog") && ok;
        ok = load_gl_proc(ogl_s_glDeleteProgram, "glDeleteProgram") && ok;
        ok = load_gl_proc(ogl_s_glUseProgram, "glUseProgram") && ok;
        ok = load_gl_proc(ogl_s_glGetUniformLocation, "glGetUniformLocation") && ok;
        ok = load_gl_proc(ogl_s_glUniform1f, "glUniform1f") && ok;
        ok = load_gl_proc(ogl_s_glUniform2f, "glUniform2f") && ok;
        ok = load_gl_proc(ogl_s_glUniform3f, "glUniform3f") && ok;
        ok = load_gl_proc(ogl_s_glUniform4f, "glUniform4f") && ok;
        ok = load_gl_proc(ogl_s_glUniform1i, "glUniform1i") && ok;
        ok = load_gl_proc(ogl_s_glUniform2i, "glUniform2i") && ok;
        ok = load_gl_proc(ogl_s_glUniform3i, "glUniform3i") && ok;
        ok = load_gl_proc(ogl_s_glUniform4i, "glUniform4i") && ok;
        ok = load_gl_proc(ogl_s_glUniformMatrix4fv, "glUniformMatrix4fv") && ok;
        ok = load_gl_proc(ogl_s_glActiveTexture, "glActiveTexture") && ok;
        ok = load_gl_proc(ogl_s_glEnableVertexAttribArray, "glEnableVertexAttribArray") && ok;
        ok = load_gl_proc(ogl_s_glDisableVertexAttribArray, "glDisableVertexAttribArray") && ok;
        ok = load_gl_proc(ogl_s_glVertexAttribPointer, "glVertexAttribPointer") && ok;
        return ok;
    }

    static bool load_buffer_procs() {
        bool ok = true;
        ok = load_gl_proc(ogl_s_glGenBuffers, "glGenBuffers") && ok;
        ok = load_gl_proc(ogl_s_glDeleteBuffers, "glDeleteBuffers") && ok;
        ok = load_gl_proc(ogl_s_glBindBuffer, "glBindBuffer") && ok;
        ok = load_gl_proc(ogl_s_glBufferData, "glBufferData") && ok;
        ok = load_gl_proc(ogl_s_glMapBuffer, "glMapBuffer") && ok;
        ok = load_gl_proc(ogl_s_glUnmapBuffer, "glUnmapBuffer") && ok;
        return ok;
    }

    static bool load_framebuffer_procs() {
        bool ok = true;
        ok = (load_gl_proc(ogl_s_glGenFramebuffers, "glGenFramebuffers") ||
              load_gl_proc(ogl_s_glGenFramebuffers, "glGenFramebuffersEXT")) && ok;
        ok = (load_gl_proc(ogl_s_glDeleteFramebuffers, "glDeleteFramebuffers") ||
              load_gl_proc(ogl_s_glDeleteFramebuffers, "glDeleteFramebuffersEXT")) && ok;
        ok = (load_gl_proc(ogl_s_glBindFramebuffer, "glBindFramebuffer") ||
              load_gl_proc(ogl_s_glBindFramebuffer, "glBindFramebufferEXT")) && ok;
        ok = (load_gl_proc(ogl_s_glFramebufferTexture2D, "glFramebufferTexture2D") ||
              load_gl_proc(ogl_s_glFramebufferTexture2D, "glFramebufferTexture2DEXT")) && ok;
        ok = (load_gl_proc(ogl_s_glCheckFramebufferStatus, "glCheckFramebufferStatus") ||
              load_gl_proc(ogl_s_glCheckFramebufferStatus, "glCheckFramebufferStatusEXT")) && ok;
        return ok;
    }

    static created_device create_ogl_device_and_interface() {
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
        register_win32_ogl_platform();
#endif

        ogl_device *raw = ogl_create_device();
        if (!raw)
            return {nullptr, {}};

        // Probe GL capabilities — context is current after ogl_create_device()
        int gl_major = 0, gl_minor = 0;
        parse_gl_version(gl_major, gl_minor);

        // Try to load glGenerateMipmap (GL 3.0+ core, or via EXT_framebuffer_object)
        const bool has_generate_mipmap =
            (gl_major > 3 || (gl_major == 3 && gl_minor >= 0)) &&
            has_gl_extension("GL_EXT_framebuffer_object");

        // For GL 3.0+ core, glGenerateMipmap is always available
        const bool gl30_or_later = (gl_major > 3 || (gl_major == 3 && gl_minor >= 0));
        const bool gl20_or_later = (gl_major > 2 || (gl_major == 2 && gl_minor >= 0));
        const bool gl15_or_later = (gl_major > 1 || (gl_major == 1 && gl_minor >= 5));

        if (gl30_or_later || has_generate_mipmap) {
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
            ogl_s_glGenerateMipmap =
                reinterpret_cast<PFNGLGENERATEMIPMAPPROC>(
                    wglGetProcAddress("glGenerateMipmap")
                );
            if (!ogl_s_glGenerateMipmap) {
                // Try the EXT suffixed version
                ogl_s_glGenerateMipmap =
                    reinterpret_cast<PFNGLGENERATEMIPMAPPROC>(
                        wglGetProcAddress("glGenerateMipmapEXT")
                    );
            }
#endif
        }

        const bool has_shaders = gl20_or_later && load_shader_procs();
        const bool has_buffers = gl15_or_later && load_buffer_procs();
        const bool has_framebuffers =
            (gl30_or_later || has_gl_extension("GL_EXT_framebuffer_object")) &&
            load_framebuffer_procs();

        graphics_backend_interface iface;
        iface.id = gfx_backend::opengl;
        iface.pixel_center_offset = {0.0f, 0.0f};

        iface.destroy_device = {ogl_destroy_device};

        iface.create_texture           = {ogl_create_texture};
        iface.destroy_texture          = {ogl_destroy_texture};
        iface.texture_format           = {ogl_texture_format};
        iface.texture_width            = {ogl_texture_width};
        iface.texture_height           = {ogl_texture_height};
        iface.texture_mip_levels       = {ogl_texture_mip_levels};
        iface.texture_sampler          = {ogl_texture_sampler};
        iface.texture_set_sampler      = {ogl_texture_set_sampler};
        iface.texture_lock             = {ogl_texture_lock};
        iface.texture_unlock           = {ogl_texture_unlock};
        iface.texture_clone            = {ogl_texture_clone};
        iface.copy_render_target_to_texture = {ogl_copy_render_target_to_texture};

        if (ogl_s_glGenerateMipmap) {
            iface.texture_generate_mipmaps = {ogl_texture_generate_mipmaps};
        } else {
            iface.texture_generate_mipmaps = {
                nullptr,
                "glGenerateMipmap requires GL 3.0+ or GL_EXT_framebuffer_object"
            };
        }

        if (has_buffers) {
            iface.create_vertex_buffer  = {ogl_create_vertex_buffer};
            iface.destroy_vertex_buffer = {ogl_destroy_vertex_buffer};
            iface.vertex_buffer_count   = {ogl_vertex_buffer_count};
            iface.vertex_buffer_stride  = {ogl_vertex_buffer_stride};
            iface.vertex_buffer_usage   = {ogl_vertex_buffer_usage};
            iface.vertex_buffer_lock    = {ogl_vertex_buffer_lock};
            iface.vertex_buffer_unlock  = {ogl_vertex_buffer_unlock};
            iface.create_index_buffer   = {ogl_create_index_buffer};
            iface.destroy_index_buffer  = {ogl_destroy_index_buffer};
            iface.index_buffer_count    = {ogl_index_buffer_count};
            iface.index_buffer_usage    = {ogl_index_buffer_usage};
            iface.index_buffer_lock     = {ogl_index_buffer_lock};
            iface.index_buffer_unlock   = {ogl_index_buffer_unlock};
        } else {
            const char *reason = "vertex and index buffers require OpenGL 1.5 buffer object entry points";
            iface.create_vertex_buffer  = {nullptr, reason};
            iface.destroy_vertex_buffer = {nullptr, reason};
            iface.vertex_buffer_count   = {nullptr, reason};
            iface.vertex_buffer_stride  = {nullptr, reason};
            iface.vertex_buffer_usage   = {nullptr, reason};
            iface.vertex_buffer_lock    = {nullptr, reason};
            iface.vertex_buffer_unlock  = {nullptr, reason};
            iface.create_index_buffer   = {nullptr, reason};
            iface.destroy_index_buffer  = {nullptr, reason};
            iface.index_buffer_count    = {nullptr, reason};
            iface.index_buffer_usage    = {nullptr, reason};
            iface.index_buffer_lock     = {nullptr, reason};
            iface.index_buffer_unlock   = {nullptr, reason};
        }

        if (has_shaders) {
            iface.create_shader_program = {ogl_create_shader_program};
        } else {
            iface.create_shader_program = {
                nullptr,
                "shader programs require OpenGL 2.0 and shader entry points"
            };
        }
        iface.destroy_shader_program = {ogl_destroy_shader_program};
        iface.shader_lookup_constant = {ogl_shader_lookup_constant};
        iface.shader_set_constant    = {ogl_shader_set_constant};
        iface.shader_lookup_sampler  = {ogl_shader_lookup_sampler};
        iface.shader_set_sampler     = {ogl_shader_set_sampler};

        iface.create_swapchain       = {ogl_create_swapchain};
        iface.destroy_swapchain      = {ogl_destroy_swapchain};
        iface.swapchain_begin_frame  = {ogl_swapchain_begin_frame};
        iface.swapchain_end_frame    = {ogl_swapchain_end_frame};
        iface.swapchain_present      = {ogl_swapchain_present};
        iface.swapchain_on_resize    = {ogl_swapchain_on_resize};

        iface.create_pipeline              = {ogl_create_pipeline};
        iface.destroy_pipeline             = {ogl_destroy_pipeline};
        iface.update_pipeline              = {ogl_update_pipeline};
        iface.bind_pipeline                = {ogl_bind_pipeline};
        iface.begin_render_pass            = {ogl_begin_render_pass};
        iface.end_render_pass              = {ogl_end_render_pass};
        iface.set_viewport                 = {ogl_set_viewport};
        iface.bind_vertex_buffer           = {ogl_bind_vertex_buffer};
        iface.bind_index_buffer            = {ogl_bind_index_buffer};
        iface.upload_transient_vertex_data = {ogl_upload_transient_vertex_data};
        iface.upload_transient_index_data  = {ogl_upload_transient_index_data};
        iface.bind_resources               = {ogl_bind_resources};
        iface.draw                         = {ogl_draw};
        iface.draw_indexed                 = {ogl_draw_indexed};

        return {raw, std::move(iface)};
    }

    void register_ogl_backend() {
        register_gfx_backend({gfx_backend::opengl, create_ogl_device_and_interface});
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
