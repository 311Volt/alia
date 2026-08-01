#ifndef ALIA_GFX_BACKEND_D3D9_OPS_HPP
#define ALIA_GFX_BACKEND_D3D9_OPS_HPP

#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../graphics_backend_interface.hpp"

namespace alia {
    struct d3d9_compiled_vertex_definition {
        IDirect3DVertexDeclaration9 *declaration = nullptr;
        d3d9_compiled_vertex_definition() = default;
        explicit d3d9_compiled_vertex_definition(IDirect3DVertexDeclaration9 *d) noexcept : declaration(d) {}
        ~d3d9_compiled_vertex_definition() { if (declaration) declaration->Release(); }
        d3d9_compiled_vertex_definition(d3d9_compiled_vertex_definition &&o) noexcept : declaration(std::exchange(o.declaration, nullptr)) {}
        d3d9_compiled_vertex_definition &operator=(d3d9_compiled_vertex_definition &&o) noexcept {
            if (this != &o) { if (declaration) declaration->Release(); declaration = std::exchange(o.declaration, nullptr); }
            return *this;
        }
        d3d9_compiled_vertex_definition(const d3d9_compiled_vertex_definition &) = delete;
        d3d9_compiled_vertex_definition &operator=(const d3d9_compiled_vertex_definition &) = delete;
    };
    struct d3d9_pipeline;
    struct d3d9_vertex_buffer;
    struct d3d9_index_buffer;
    struct d3d9_device : device_handle {
        IDirect3D9 *d3d = nullptr;
        IDirect3DDevice9 *device = nullptr;
        HWND dummy = nullptr;
        D3DCAPS9 caps = {};
        std::vector<std::optional<d3d9_compiled_vertex_definition>> vertex_definitions;
        d3d9_pipeline *current_pipeline = nullptr;
        d3d9_vertex_buffer *current_vb = nullptr;
        d3d9_index_buffer *current_ib = nullptr;
        const void *transient_vertices = nullptr;
        int transient_vertex_bytes = 0;
        const uint32_t *transient_indices = nullptr;
        int transient_index_count = 0;
        bool pass_active = false;
        vec2i pass_size = {};
        bool pass_has_depth = false;
    };
    struct d3d9_texture : texture_handle {
        IDirect3DDevice9 *device = nullptr;
        IDirect3DTexture9 *texture = nullptr;
        int width = 0, height = 0, mip_levels = 1;
        bool autogen = false;
        pixel_format fmt = pixel_format::bgra8888;
        texture_role role = texture_role::color;
        texture_usage usage = texture_usage::sampling_only;
        sampler_state sampler = {};
        IDirect3DSurface9 *lock_surface = nullptr;
    };
    struct d3d9_vertex_buffer : vertex_buffer_handle {
        IDirect3DDevice9 *device = nullptr;
        IDirect3DVertexBuffer9 *buffer = nullptr;
        int stride = 0, count = 0;
        buffer_usage usage = buffer_usage::static_mesh;
    };
    struct d3d9_index_buffer : index_buffer_handle {
        IDirect3DDevice9 *device = nullptr;
        IDirect3DIndexBuffer9 *buffer = nullptr;
        int count = 0;
        buffer_usage usage = buffer_usage::static_mesh;
    };
    struct d3d9_swapchain : swapchain_handle {
        IDirect3DDevice9 *device = nullptr;
        IDirect3DSwapChain9 *swap_chain = nullptr;
        IDirect3DSurface9 *depth_stencil = nullptr;
        HWND hwnd = nullptr;
        vec2i size = {};
    };
    struct d3d9_stored_shader_constant {
        shader_constant_slot slot = {};
        shader_constant_value_type type = shader_constant_value_type::float_1;
        std::vector<float> floats;
        std::vector<int> ints;
    };
    struct d3d9_shader_program : shader_program_handle {
        IDirect3DDevice9 *device = nullptr;
        IDirect3DVertexShader9 *vertex_shader = nullptr;
        IDirect3DPixelShader9 *pixel_shader = nullptr;
        std::unordered_map<std::string, shader_constant_slot> constants;
        std::unordered_map<std::string, shader_sampler_slot> samplers;
        std::vector<d3d9_stored_shader_constant> stored_constants;
        std::unordered_map<int, texture_handle *> sampler_textures;
    };
    struct d3d9_pipeline : pipeline_handle {
        d3d9_shader_program *shader = nullptr;
        const basic_effect *effect = nullptr;
        vertex_definition_view layout = {};
        blend_state blend = {};
        depth_state depth = {};
        raster_state raster = {};
    };

    inline d3d9_device *as_d3d9_device(device_handle *h) { return static_cast<d3d9_device *>(h); }
    inline d3d9_texture *as_d3d9_texture(texture_handle *h) { return static_cast<d3d9_texture *>(h); }
    inline const d3d9_texture *as_d3d9_texture(const texture_handle *h) { return static_cast<const d3d9_texture *>(h); }
    inline d3d9_vertex_buffer *as_d3d9_vertex_buffer(vertex_buffer_handle *h) { return static_cast<d3d9_vertex_buffer *>(h); }
    inline const d3d9_vertex_buffer *as_d3d9_vertex_buffer(const vertex_buffer_handle *h) { return static_cast<const d3d9_vertex_buffer *>(h); }
    inline d3d9_index_buffer *as_d3d9_index_buffer(index_buffer_handle *h) { return static_cast<d3d9_index_buffer *>(h); }
    inline const d3d9_index_buffer *as_d3d9_index_buffer(const index_buffer_handle *h) { return static_cast<const d3d9_index_buffer *>(h); }
    inline d3d9_swapchain *as_d3d9_swapchain(swapchain_handle *h) { return static_cast<d3d9_swapchain *>(h); }
    inline d3d9_shader_program *as_d3d9_shader_program(shader_program_handle *h) { return static_cast<d3d9_shader_program *>(h); }
    inline d3d9_pipeline *as_d3d9_pipeline(pipeline_handle *h) { return static_cast<d3d9_pipeline *>(h); }
    inline DWORD to_d3d_color(color c) {
        auto clamp = [](float v) { return static_cast<BYTE>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
        return D3DCOLOR_RGBA(clamp(c.r), clamp(c.g), clamp(c.b), clamp(c.a));
    }

    d3d9_device *d3d9_create_device(); void d3d9_destroy_device(device_handle *);
    texture_handle *d3d9_create_texture(device_handle *, pixel_format, vec2i, int, texture_role, texture_usage);
    void d3d9_destroy_texture(texture_handle *); pixel_format d3d9_texture_format(const texture_handle *);
    int d3d9_texture_width(const texture_handle *); int d3d9_texture_height(const texture_handle *); int d3d9_texture_mip_levels(const texture_handle *);
    sampler_state d3d9_texture_sampler(const texture_handle *); void d3d9_texture_set_sampler(texture_handle *, const sampler_state &);
    bool d3d9_texture_lock(texture_handle *, rect_i, int, texture_lock_mode, texture_lock_info &); void d3d9_texture_unlock(texture_handle *, const texture_lock_info &, bool);
    void d3d9_texture_generate_mipmaps(texture_handle *); texture_handle *d3d9_texture_clone(const texture_handle *);
    bool d3d9_copy_render_target_to_texture(device_handle *, texture_handle *, rect_i, vec2i, vec2i, int);
    vertex_buffer_handle *d3d9_create_vertex_buffer(device_handle *, int, int, buffer_usage, const void *); void d3d9_destroy_vertex_buffer(vertex_buffer_handle *);
    int d3d9_vertex_buffer_count(const vertex_buffer_handle *); int d3d9_vertex_buffer_stride(const vertex_buffer_handle *); buffer_usage d3d9_vertex_buffer_usage(const vertex_buffer_handle *);
    bool d3d9_vertex_buffer_lock(vertex_buffer_handle *, int, int, buffer_lock_mode, buffer_lock_info &); void d3d9_vertex_buffer_unlock(vertex_buffer_handle *, const buffer_lock_info &, bool);
    index_buffer_handle *d3d9_create_index_buffer(device_handle *, int, buffer_usage, const uint32_t *); void d3d9_destroy_index_buffer(index_buffer_handle *);
    int d3d9_index_buffer_count(const index_buffer_handle *); buffer_usage d3d9_index_buffer_usage(const index_buffer_handle *);
    bool d3d9_index_buffer_lock(index_buffer_handle *, int, int, buffer_lock_mode, buffer_lock_info &); void d3d9_index_buffer_unlock(index_buffer_handle *, const buffer_lock_info &, bool);
    shader_program_handle *d3d9_create_shader_program(device_handle *, const shader_program_desc &); void d3d9_destroy_shader_program(shader_program_handle *);
    shader_constant_slot d3d9_shader_lookup_constant(shader_program_handle *, std::string_view, shader_type); void d3d9_shader_set_constant(shader_program_handle *, const shader_constant_slot &, const shader_constant_payload &);
    shader_sampler_slot d3d9_shader_lookup_sampler(shader_program_handle *, std::string_view, shader_type); void d3d9_shader_set_sampler(shader_program_handle *, const shader_sampler_slot &, texture_handle *);
    void d3d9_apply_program_state(IDirect3DDevice9 *, d3d9_shader_program *);
    swapchain_handle *d3d9_create_swapchain(device_handle *, void *, vec2i); void d3d9_destroy_swapchain(swapchain_handle *);
    void d3d9_swapchain_begin_frame(swapchain_handle *); void d3d9_swapchain_end_frame(swapchain_handle *); void d3d9_swapchain_present(swapchain_handle *); void d3d9_swapchain_on_resize(swapchain_handle *, vec2i);
    pipeline_handle *d3d9_create_pipeline(device_handle *, const pipeline_desc &); void d3d9_destroy_pipeline(pipeline_handle *); void d3d9_update_pipeline(pipeline_handle *, const pipeline_desc &); void d3d9_bind_pipeline(device_handle *, pipeline_handle *);
    bool d3d9_begin_render_pass(device_handle *, const render_pass_begin_info &); void d3d9_end_render_pass(device_handle *); void d3d9_set_viewport(device_handle *, const render_viewport &);
    void d3d9_bind_vertex_buffer(device_handle *, vertex_buffer_handle *); void d3d9_bind_index_buffer(device_handle *, index_buffer_handle *);
    void d3d9_upload_transient_vertex_data(device_handle *, const void *, int); void d3d9_upload_transient_index_data(device_handle *, std::span<const uint32_t>);
    void d3d9_bind_resources(device_handle *, const texture_sampler_binding &); void d3d9_draw(device_handle *, primitive_topology, int, int); void d3d9_draw_indexed(device_handle *, primitive_topology, int, int, int);
} // namespace alia
#endif
#endif
