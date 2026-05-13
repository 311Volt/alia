#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"

#include <algorithm>
#include <iterator>
#include <string>

namespace alia {

    PFNGLCREATESHADERPROC ogl_s_glCreateShader = nullptr;
    PFNGLSHADERSOURCEPROC ogl_s_glShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC ogl_s_glCompileShader = nullptr;
    PFNGLGETSHADERIVPROC ogl_s_glGetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC ogl_s_glGetShaderInfoLog = nullptr;
    PFNGLDELETESHADERPROC ogl_s_glDeleteShader = nullptr;
    PFNGLCREATEPROGRAMPROC ogl_s_glCreateProgram = nullptr;
    PFNGLATTACHSHADERPROC ogl_s_glAttachShader = nullptr;
    PFNGLBINDATTRIBLOCATIONPROC ogl_s_glBindAttribLocation = nullptr;
    PFNGLLINKPROGRAMPROC ogl_s_glLinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC ogl_s_glGetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC ogl_s_glGetProgramInfoLog = nullptr;
    PFNGLDELETEPROGRAMPROC ogl_s_glDeleteProgram = nullptr;
    PFNGLUSEPROGRAMPROC ogl_s_glUseProgram = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC ogl_s_glGetUniformLocation = nullptr;
    PFNGLUNIFORM1FPROC ogl_s_glUniform1f = nullptr;
    PFNGLUNIFORM2FPROC ogl_s_glUniform2f = nullptr;
    PFNGLUNIFORM3FPROC ogl_s_glUniform3f = nullptr;
    PFNGLUNIFORM4FPROC ogl_s_glUniform4f = nullptr;
    PFNGLUNIFORM1IPROC ogl_s_glUniform1i = nullptr;
    PFNGLUNIFORM2IPROC ogl_s_glUniform2i = nullptr;
    PFNGLUNIFORM3IPROC ogl_s_glUniform3i = nullptr;
    PFNGLUNIFORM4IPROC ogl_s_glUniform4i = nullptr;
    PFNGLUNIFORMMATRIX4FVPROC ogl_s_glUniformMatrix4fv = nullptr;
    PFNGLACTIVETEXTUREPROC ogl_s_glActiveTexture = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC ogl_s_glEnableVertexAttribArray = nullptr;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC ogl_s_glDisableVertexAttribArray = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC ogl_s_glVertexAttribPointer = nullptr;

    namespace {

        constexpr GLuint attr_position = 0;
        constexpr GLuint attr_color = 1;
        constexpr GLuint attr_tex_coord = 2;

        const shader_source *
        select_source(const shader_program_desc &desc, shader_type type) {
            for (const auto &src : desc.sources) {
                if (src.backend == gfx_backend::opengl && src.type == type)
                    return &src;
            }
            for (const auto &src : desc.sources) {
                if (src.backend == gfx_backend::auto_ && src.type == type)
                    return &src;
            }
            return nullptr;
        }

        std::string shader_log(GLuint shader) {
            GLint length = 0;
            ogl_s_glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            if (length <= 1)
                return {};
            std::string log(static_cast<std::size_t>(length), '\0');
            ogl_s_glGetShaderInfoLog(shader, length, nullptr, log.data());
            return log;
        }

        std::string program_log(GLuint program) {
            GLint length = 0;
            ogl_s_glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            if (length <= 1)
                return {};
            std::string log(static_cast<std::size_t>(length), '\0');
            ogl_s_glGetProgramInfoLog(program, length, nullptr, log.data());
            return log;
        }

        GLuint compile_shader(const shader_source &source) {
            const GLenum type = source.type == shader_type::vertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
            const GLuint shader = ogl_s_glCreateShader(type);
            if (!shader)
                throw shader_error("OpenGL shader compile failed: glCreateShader returned 0");

            const std::string source_text(source.source);
            const GLchar *source_ptr = source_text.c_str();
            const GLint source_len = static_cast<GLint>(source_text.size());
            ogl_s_glShaderSource(shader, 1, &source_ptr, &source_len);
            ogl_s_glCompileShader(shader);

            GLint compiled = GL_FALSE;
            ogl_s_glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (!compiled) {
                const std::string name =
                    source.debug_name.empty() ? "alia shader" : std::string(source.debug_name);
                std::string message = "OpenGL shader compile failed for " + name;
                const std::string log = shader_log(shader);
                if (!log.empty())
                    message += ":\n" + log;
                ogl_s_glDeleteShader(shader);
                throw shader_error(message);
            }
            return shader;
        }

        void apply_constant(const ogl_stored_shader_constant &c) {
            const GLint loc = static_cast<GLint>(c.slot.location);
            switch (c.type) {
            case shader_constant_value_type::float_1:
                ogl_s_glUniform1f(loc, c.floats[0]);
                break;
            case shader_constant_value_type::float_2:
                ogl_s_glUniform2f(loc, c.floats[0], c.floats[1]);
                break;
            case shader_constant_value_type::float_3:
                ogl_s_glUniform3f(loc, c.floats[0], c.floats[1], c.floats[2]);
                break;
            case shader_constant_value_type::float_4:
                ogl_s_glUniform4f(loc, c.floats[0], c.floats[1], c.floats[2], c.floats[3]);
                break;
            case shader_constant_value_type::int_1:
                ogl_s_glUniform1i(loc, c.ints[0]);
                break;
            case shader_constant_value_type::int_2:
                ogl_s_glUniform2i(loc, c.ints[0], c.ints[1]);
                break;
            case shader_constant_value_type::int_3:
                ogl_s_glUniform3i(loc, c.ints[0], c.ints[1], c.ints[2]);
                break;
            case shader_constant_value_type::int_4:
                ogl_s_glUniform4i(loc, c.ints[0], c.ints[1], c.ints[2], c.ints[3]);
                break;
            case shader_constant_value_type::matrix_4x4:
                ogl_s_glUniformMatrix4fv(loc, 1, GL_FALSE, c.floats.data());
                break;
            }
        }

        void bind_texture_unit(int unit, texture_handle *texture) {
            auto *ogl_texture = texture ? as_ogl_texture(texture) : nullptr;
            ogl_s_glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
            glBindTexture(GL_TEXTURE_2D, ogl_texture ? ogl_texture->tex_id : 0);
        }

    } // namespace

    shader_program_handle *ogl_create_shader_program(device_handle *, const shader_program_desc &desc) {
        const shader_source *vertex_source = select_source(desc, shader_type::vertex);
        const shader_source *pixel_source = select_source(desc, shader_type::pixel);
        if (!vertex_source || !pixel_source)
            throw shader_error("OpenGL shader program requires matching vertex and pixel sources");

        GLuint vertex_shader = 0;
        GLuint pixel_shader = 0;
        try {
            vertex_shader = compile_shader(*vertex_source);
            pixel_shader = compile_shader(*pixel_source);
        } catch (...) {
            if (vertex_shader)
                ogl_s_glDeleteShader(vertex_shader);
            if (pixel_shader)
                ogl_s_glDeleteShader(pixel_shader);
            throw;
        }
        const GLuint program_id = ogl_s_glCreateProgram();
        if (!program_id) {
            ogl_s_glDeleteShader(vertex_shader);
            ogl_s_glDeleteShader(pixel_shader);
            throw shader_error("OpenGL shader link failed: glCreateProgram returned 0");
        }

        ogl_s_glAttachShader(program_id, vertex_shader);
        ogl_s_glAttachShader(program_id, pixel_shader);
        ogl_s_glBindAttribLocation(program_id, attr_position, "a_position");
        ogl_s_glBindAttribLocation(program_id, attr_color, "a_color");
        ogl_s_glBindAttribLocation(program_id, attr_tex_coord, "a_tex_coord");
        ogl_s_glLinkProgram(program_id);

        ogl_s_glDeleteShader(vertex_shader);
        ogl_s_glDeleteShader(pixel_shader);

        GLint linked = GL_FALSE;
        ogl_s_glGetProgramiv(program_id, GL_LINK_STATUS, &linked);
        if (!linked) {
            std::string message = "OpenGL shader link failed";
            const std::string log = program_log(program_id);
            if (!log.empty())
                message += ":\n" + log;
            ogl_s_glDeleteProgram(program_id);
            throw shader_error(message);
        }

        auto *program = new ogl_shader_program;
        program->program = program_id;
        for (const auto &binding : desc.sampler_bindings) {
            if (!binding.name.empty() && binding.slot >= 0)
                program->sampler_units.emplace(std::string(binding.name), binding.slot);
        }
        return program;
    }

    void ogl_destroy_shader_program(shader_program_handle *h) {
        auto *program = as_ogl_shader_program(h);
        if (program->program)
            ogl_s_glDeleteProgram(program->program);
        delete program;
    }

    shader_constant_slot
    ogl_shader_lookup_constant(shader_program_handle *h, std::string_view name, shader_type stage) {
        auto *program = as_ogl_shader_program(h);
        const std::string uniform_name(name);
        const GLint location = ogl_s_glGetUniformLocation(program->program, uniform_name.c_str());
        if (location < 0)
            return {};
        return {true, stage, location, 1};
    }

    void ogl_shader_set_constant(
        shader_program_handle *h,
        const shader_constant_slot &slot,
        const shader_constant_payload &payload
    ) {
        auto *program = as_ogl_shader_program(h);
        auto it = std::find_if(
            program->stored_constants.begin(),
            program->stored_constants.end(),
            [&](const ogl_stored_shader_constant &c) {
                return c.slot.location == slot.location;
            }
        );
        if (it == program->stored_constants.end()) {
            program->stored_constants.push_back({});
            it = std::prev(program->stored_constants.end());
        }

        it->slot = slot;
        it->type = payload.type;
        it->floats.assign(payload.floats.begin(), payload.floats.end());
        it->ints.assign(payload.ints.begin(), payload.ints.end());
    }

    shader_sampler_slot
    ogl_shader_lookup_sampler(shader_program_handle *h, std::string_view name, shader_type stage) {
        auto *program = as_ogl_shader_program(h);
        const std::string uniform_name(name);
        const GLint location = ogl_s_glGetUniformLocation(program->program, uniform_name.c_str());
        if (location < 0)
            return {};

        int unit = 0;
        if (auto it = program->sampler_units.find(uniform_name); it != program->sampler_units.end())
            unit = it->second;
        return {true, stage, location, unit};
    }

    void ogl_shader_set_sampler(shader_program_handle *h, const shader_sampler_slot &slot, texture_handle *tex) {
        auto *program = as_ogl_shader_program(h);
        program->sampler_textures[slot.slot] = tex;
        ogl_s_glUseProgram(program->program);
        ogl_s_glUniform1i(static_cast<GLint>(slot.location), slot.slot);
    }

    void ogl_apply_shader_program(shader_program_handle *h, texture_handle *primary_texture) {
        if (!h) {
            if (ogl_s_glUseProgram)
                ogl_s_glUseProgram(0);
            return;
        }

        auto *program = as_ogl_shader_program(h);
        ogl_s_glUseProgram(program->program);

        for (const auto &constant : program->stored_constants)
            apply_constant(constant);

        for (const auto &[unit, texture] : program->sampler_textures)
            bind_texture_unit(unit, texture);
        if (primary_texture)
            bind_texture_unit(0, primary_texture);
        ogl_s_glActiveTexture(GL_TEXTURE0);
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
