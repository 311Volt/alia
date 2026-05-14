#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"

#include <GL/glext.h>

#include <cstdint>

namespace alia {

    PFNGLGENBUFFERSPROC ogl_s_glGenBuffers = nullptr;
    PFNGLDELETEBUFFERSPROC ogl_s_glDeleteBuffers = nullptr;
    PFNGLBINDBUFFERPROC ogl_s_glBindBuffer = nullptr;
    PFNGLBUFFERDATAPROC ogl_s_glBufferData = nullptr;
    PFNGLMAPBUFFERPROC ogl_s_glMapBuffer = nullptr;
    PFNGLUNMAPBUFFERPROC ogl_s_glUnmapBuffer = nullptr;

    namespace {

        GLenum to_gl_usage(buffer_usage usage) {
            return usage == buffer_usage::dynamic_mesh ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
        }

        GLenum to_gl_access(buffer_lock_mode mode) {
            switch (mode) {
            case buffer_lock_mode::read_only:  return GL_READ_ONLY;
            case buffer_lock_mode::write_only: return GL_WRITE_ONLY;
            case buffer_lock_mode::read_write: return GL_READ_WRITE;
            }
            return GL_READ_WRITE;
        }

    } // namespace

    vertex_buffer_handle *ogl_create_vertex_buffer(
        device_handle *,
        int vertex_stride,
        int vertex_count,
        buffer_usage usage,
        const void *initial_data
    ) {
        if (vertex_stride <= 0 || vertex_count <= 0 || !ogl_s_glGenBuffers)
            return nullptr;

        GLuint id = 0;
        ogl_s_glGenBuffers(1, &id);
        if (!id)
            return nullptr;

        const int size_bytes = vertex_stride * vertex_count;
        ogl_s_glBindBuffer(GL_ARRAY_BUFFER, id);
        ogl_s_glBufferData(GL_ARRAY_BUFFER, size_bytes, initial_data, to_gl_usage(usage));
        ogl_s_glBindBuffer(GL_ARRAY_BUFFER, 0);

        auto *out = new ogl_vertex_buffer;
        out->buffer_id = id;
        out->stride = vertex_stride;
        out->count = vertex_count;
        out->usage = usage;
        return out;
    }

    void ogl_destroy_vertex_buffer(vertex_buffer_handle *h) {
        auto *buffer = as_ogl_vertex_buffer(h);
        if (buffer->buffer_id)
            ogl_s_glDeleteBuffers(1, &buffer->buffer_id);
        delete buffer;
    }

    int ogl_vertex_buffer_count(const vertex_buffer_handle *h) {
        return as_ogl_vertex_buffer(h)->count;
    }

    int ogl_vertex_buffer_stride(const vertex_buffer_handle *h) {
        return as_ogl_vertex_buffer(h)->stride;
    }

    buffer_usage ogl_vertex_buffer_usage(const vertex_buffer_handle *h) {
        return as_ogl_vertex_buffer(h)->usage;
    }

    bool ogl_vertex_buffer_lock(
        vertex_buffer_handle *h,
        int first_vertex,
        int vertex_count,
        buffer_lock_mode mode,
        buffer_lock_info &out
    ) {
        auto *buffer = as_ogl_vertex_buffer(h);
        if (first_vertex < 0 || vertex_count <= 0 || first_vertex + vertex_count > buffer->count)
            return false;

        const int offset_bytes = first_vertex * buffer->stride;
        const int size_bytes = vertex_count * buffer->stride;

        ogl_s_glBindBuffer(GL_ARRAY_BUFFER, buffer->buffer_id);
        auto *mapped = static_cast<std::byte *>(ogl_s_glMapBuffer(GL_ARRAY_BUFFER, to_gl_access(mode)));
        if (!mapped) {
            ogl_s_glBindBuffer(GL_ARRAY_BUFFER, 0);
            return false;
        }

        out.offset_bytes = offset_bytes;
        out.size_bytes = size_bytes;
        out.data = mapped + offset_bytes;
        return true;
    }

    void ogl_vertex_buffer_unlock(vertex_buffer_handle *h, const buffer_lock_info &, bool) {
        auto *buffer = as_ogl_vertex_buffer(h);
        ogl_s_glBindBuffer(GL_ARRAY_BUFFER, buffer->buffer_id);
        ogl_s_glUnmapBuffer(GL_ARRAY_BUFFER);
        ogl_s_glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    index_buffer_handle *ogl_create_index_buffer(
        device_handle *,
        int index_count,
        buffer_usage usage,
        const uint32_t *initial_data
    ) {
        if (index_count <= 0 || !ogl_s_glGenBuffers)
            return nullptr;

        GLuint id = 0;
        ogl_s_glGenBuffers(1, &id);
        if (!id)
            return nullptr;

        const int size_bytes = index_count * static_cast<int>(sizeof(uint32_t));
        ogl_s_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
        ogl_s_glBufferData(GL_ELEMENT_ARRAY_BUFFER, size_bytes, initial_data, to_gl_usage(usage));
        ogl_s_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        auto *out = new ogl_index_buffer;
        out->buffer_id = id;
        out->count = index_count;
        out->usage = usage;
        return out;
    }

    void ogl_destroy_index_buffer(index_buffer_handle *h) {
        auto *buffer = as_ogl_index_buffer(h);
        if (buffer->buffer_id)
            ogl_s_glDeleteBuffers(1, &buffer->buffer_id);
        delete buffer;
    }

    int ogl_index_buffer_count(const index_buffer_handle *h) {
        return as_ogl_index_buffer(h)->count;
    }

    buffer_usage ogl_index_buffer_usage(const index_buffer_handle *h) {
        return as_ogl_index_buffer(h)->usage;
    }

    bool ogl_index_buffer_lock(
        index_buffer_handle *h,
        int first_index,
        int index_count,
        buffer_lock_mode mode,
        buffer_lock_info &out
    ) {
        auto *buffer = as_ogl_index_buffer(h);
        if (first_index < 0 || index_count <= 0 || first_index + index_count > buffer->count)
            return false;

        const int offset_bytes = first_index * static_cast<int>(sizeof(uint32_t));
        const int size_bytes = index_count * static_cast<int>(sizeof(uint32_t));

        ogl_s_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->buffer_id);
        auto *mapped = static_cast<std::byte *>(ogl_s_glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, to_gl_access(mode)));
        if (!mapped) {
            ogl_s_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            return false;
        }

        out.offset_bytes = offset_bytes;
        out.size_bytes = size_bytes;
        out.data = mapped + offset_bytes;
        return true;
    }

    void ogl_index_buffer_unlock(index_buffer_handle *h, const buffer_lock_info &, bool) {
        auto *buffer = as_ogl_index_buffer(h);
        ogl_s_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->buffer_id);
        ogl_s_glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        ogl_s_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
