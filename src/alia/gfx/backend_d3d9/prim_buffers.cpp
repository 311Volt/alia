#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"

#include <cstring>

namespace alia {

    namespace {

        DWORD buffer_usage_flags(buffer_usage usage) {
            return usage == buffer_usage::dynamic_mesh ? D3DUSAGE_DYNAMIC : 0u;
        }

        D3DPOOL buffer_pool(buffer_usage usage) {
            return usage == buffer_usage::dynamic_mesh ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
        }

        DWORD lock_flags(buffer_usage usage, buffer_lock_mode mode, int offset_bytes, int size_bytes, int total_bytes) {
            DWORD flags = mode == buffer_lock_mode::read_only ? D3DLOCK_READONLY : 0u;
            if (usage == buffer_usage::dynamic_mesh &&
                mode == buffer_lock_mode::write_only &&
                offset_bytes == 0 &&
                size_bytes == total_bytes) {
                flags |= D3DLOCK_DISCARD;
            }
            return flags;
        }

    } // namespace

    vertex_buffer_handle *d3d9_create_vertex_buffer(
        device_handle *dev_h,
        int vertex_stride,
        int vertex_count,
        buffer_usage usage,
        const void *initial_data
    ) {
        if (vertex_stride <= 0 || vertex_count <= 0)
            return nullptr;

        auto *dev = as_d3d9_device(dev_h);
        const int size_bytes = vertex_stride * vertex_count;

        IDirect3DVertexBuffer9 *buffer = nullptr;
        if (FAILED(dev->device->CreateVertexBuffer(
                static_cast<UINT>(size_bytes),
                buffer_usage_flags(usage),
                0,
                buffer_pool(usage),
                &buffer,
                nullptr
            )) || !buffer)
            return nullptr;

        if (initial_data) {
            void *dst = nullptr;
            if (FAILED(buffer->Lock(
                    0,
                    static_cast<UINT>(size_bytes),
                    &dst,
                    lock_flags(usage, buffer_lock_mode::write_only, 0, size_bytes, size_bytes)
                ))) {
                buffer->Release();
                return nullptr;
            }
            std::memcpy(dst, initial_data, static_cast<std::size_t>(size_bytes));
            buffer->Unlock();
        }

        auto *out = new d3d9_vertex_buffer;
        out->device = dev->device;
        out->buffer = buffer;
        out->stride = vertex_stride;
        out->count = vertex_count;
        out->usage = usage;
        return out;
    }

    void d3d9_destroy_vertex_buffer(vertex_buffer_handle *h) {
        auto *buffer = as_d3d9_vertex_buffer(h);
        if (buffer->buffer)
            buffer->buffer->Release();
        delete buffer;
    }

    int d3d9_vertex_buffer_count(const vertex_buffer_handle *h) {
        return as_d3d9_vertex_buffer(h)->count;
    }

    int d3d9_vertex_buffer_stride(const vertex_buffer_handle *h) {
        return as_d3d9_vertex_buffer(h)->stride;
    }

    buffer_usage d3d9_vertex_buffer_usage(const vertex_buffer_handle *h) {
        return as_d3d9_vertex_buffer(h)->usage;
    }

    bool d3d9_vertex_buffer_lock(
        vertex_buffer_handle *h,
        int first_vertex,
        int vertex_count,
        buffer_lock_mode mode,
        buffer_lock_info &out
    ) {
        auto *buffer = as_d3d9_vertex_buffer(h);
        if (first_vertex < 0 || vertex_count <= 0 || first_vertex + vertex_count > buffer->count)
            return false;

        const int offset_bytes = first_vertex * buffer->stride;
        const int size_bytes = vertex_count * buffer->stride;
        const int total_bytes = buffer->count * buffer->stride;

        void *data = nullptr;
        if (FAILED(buffer->buffer->Lock(
                static_cast<UINT>(offset_bytes),
                static_cast<UINT>(size_bytes),
                &data,
                lock_flags(buffer->usage, mode, offset_bytes, size_bytes, total_bytes)
            )))
            return false;

        out.offset_bytes = offset_bytes;
        out.size_bytes = size_bytes;
        out.data = static_cast<std::byte *>(data);
        return true;
    }

    void d3d9_vertex_buffer_unlock(vertex_buffer_handle *h, const buffer_lock_info &, bool) {
        as_d3d9_vertex_buffer(h)->buffer->Unlock();
    }

    index_buffer_handle *d3d9_create_index_buffer(
        device_handle *dev_h,
        int index_count,
        buffer_usage usage,
        const uint32_t *initial_data
    ) {
        if (index_count <= 0)
            return nullptr;

        auto *dev = as_d3d9_device(dev_h);
        const int size_bytes = index_count * static_cast<int>(sizeof(uint32_t));

        IDirect3DIndexBuffer9 *buffer = nullptr;
        if (FAILED(dev->device->CreateIndexBuffer(
                static_cast<UINT>(size_bytes),
                buffer_usage_flags(usage),
                D3DFMT_INDEX32,
                buffer_pool(usage),
                &buffer,
                nullptr
            )) || !buffer)
            return nullptr;

        if (initial_data) {
            void *dst = nullptr;
            if (FAILED(buffer->Lock(
                    0,
                    static_cast<UINT>(size_bytes),
                    &dst,
                    lock_flags(usage, buffer_lock_mode::write_only, 0, size_bytes, size_bytes)
                ))) {
                buffer->Release();
                return nullptr;
            }
            std::memcpy(dst, initial_data, static_cast<std::size_t>(size_bytes));
            buffer->Unlock();
        }

        auto *out = new d3d9_index_buffer;
        out->device = dev->device;
        out->buffer = buffer;
        out->count = index_count;
        out->usage = usage;
        return out;
    }

    void d3d9_destroy_index_buffer(index_buffer_handle *h) {
        auto *buffer = as_d3d9_index_buffer(h);
        if (buffer->buffer)
            buffer->buffer->Release();
        delete buffer;
    }

    int d3d9_index_buffer_count(const index_buffer_handle *h) {
        return as_d3d9_index_buffer(h)->count;
    }

    buffer_usage d3d9_index_buffer_usage(const index_buffer_handle *h) {
        return as_d3d9_index_buffer(h)->usage;
    }

    bool d3d9_index_buffer_lock(
        index_buffer_handle *h,
        int first_index,
        int index_count,
        buffer_lock_mode mode,
        buffer_lock_info &out
    ) {
        auto *buffer = as_d3d9_index_buffer(h);
        if (first_index < 0 || index_count <= 0 || first_index + index_count > buffer->count)
            return false;

        const int offset_bytes = first_index * static_cast<int>(sizeof(uint32_t));
        const int size_bytes = index_count * static_cast<int>(sizeof(uint32_t));
        const int total_bytes = buffer->count * static_cast<int>(sizeof(uint32_t));

        void *data = nullptr;
        if (FAILED(buffer->buffer->Lock(
                static_cast<UINT>(offset_bytes),
                static_cast<UINT>(size_bytes),
                &data,
                lock_flags(buffer->usage, mode, offset_bytes, size_bytes, total_bytes)
            )))
            return false;

        out.offset_bytes = offset_bytes;
        out.size_bytes = size_bytes;
        out.data = static_cast<std::byte *>(data);
        return true;
    }

    void d3d9_index_buffer_unlock(index_buffer_handle *h, const buffer_lock_info &, bool) {
        as_d3d9_index_buffer(h)->buffer->Unlock();
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
