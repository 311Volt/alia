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
#include <string>
#include <unordered_map>
#include <memory>
#include <span>
#include <vector>

#include "../graphics_backend_interface.hpp"

namespace alia {

    // ── Concrete device/texture/swapchain structs ─────────────────────────

    struct d3d9_device : device_handle {
        IDirect3D9 *d3d = nullptr;
        IDirect3DDevice9 *device = nullptr;
        HWND dummy = nullptr;
        D3DCAPS9 caps = {};
    };

    struct d3d9_texture : texture_handle {
        IDirect3DDevice9 *device = nullptr; // non-owning
        IDirect3DTexture9 *texture = nullptr;
        int width = 0;
        int height = 0;
        int mip_levels = 1;
        bool autogen = false;
        pixel_format fmt = pixel_format::bgra8888;
        texture_role role = texture_role::color;
        texture_usage usage = texture_usage::sampling_only;
        sampler_state sampler = {};
        IDirect3DSurface9 *lock_surface = nullptr;
    };

    struct d3d9_render_target_scope : render_target_scope_handle {
        IDirect3DSurface9 *previous_target = nullptr;
        D3DVIEWPORT9 previous_viewport = {};
    };

    struct d3d9_vertex_buffer : vertex_buffer_handle {
        IDirect3DDevice9 *device = nullptr; // non-owning
        IDirect3DVertexBuffer9 *buffer = nullptr;
        int stride = 0;
        int count = 0;
        buffer_usage usage = buffer_usage::static_mesh;
    };

    struct d3d9_index_buffer : index_buffer_handle {
        IDirect3DDevice9 *device = nullptr; // non-owning
        IDirect3DIndexBuffer9 *buffer = nullptr;
        int count = 0;
        buffer_usage usage = buffer_usage::static_mesh;
    };

    struct d3d9_swapchain : swapchain_handle {
        IDirect3DDevice9 *device = nullptr; // non-owning
        IDirect3DSwapChain9 *swap_chain = nullptr;
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
        IDirect3DDevice9 *device = nullptr; // non-owning
        IDirect3DVertexShader9 *vertex_shader = nullptr;
        IDirect3DPixelShader9 *pixel_shader = nullptr;
        std::unordered_map<std::string, shader_constant_slot> constants;
        std::unordered_map<std::string, shader_sampler_slot> samplers;
        std::vector<d3d9_stored_shader_constant> stored_constants;
        std::unordered_map<int, texture_handle *> sampler_textures;
    };

    // ── Cast helpers ──────────────────────────────────────────────────────

    inline d3d9_device *as_d3d9_device(device_handle *h) {
        return static_cast<d3d9_device *>(h);
    }
    inline const d3d9_device *as_d3d9_device(const device_handle *h) {
        return static_cast<const d3d9_device *>(h);
    }
    inline d3d9_texture *as_d3d9_texture(texture_handle *h) {
        return static_cast<d3d9_texture *>(h);
    }
    inline const d3d9_texture *as_d3d9_texture(const texture_handle *h) {
        return static_cast<const d3d9_texture *>(h);
    }
    inline d3d9_vertex_buffer *as_d3d9_vertex_buffer(vertex_buffer_handle *h) {
        return static_cast<d3d9_vertex_buffer *>(h);
    }
    inline const d3d9_vertex_buffer *as_d3d9_vertex_buffer(const vertex_buffer_handle *h) {
        return static_cast<const d3d9_vertex_buffer *>(h);
    }
    inline d3d9_index_buffer *as_d3d9_index_buffer(index_buffer_handle *h) {
        return static_cast<d3d9_index_buffer *>(h);
    }
    inline const d3d9_index_buffer *as_d3d9_index_buffer(const index_buffer_handle *h) {
        return static_cast<const d3d9_index_buffer *>(h);
    }
    inline d3d9_swapchain *as_d3d9_swapchain(swapchain_handle *h) {
        return static_cast<d3d9_swapchain *>(h);
    }
    inline d3d9_shader_program *as_d3d9_shader_program(shader_program_handle *h) {
        return static_cast<d3d9_shader_program *>(h);
    }
    inline const d3d9_shader_program *as_d3d9_shader_program(const shader_program_handle *h) {
        return static_cast<const d3d9_shader_program *>(h);
    }

    // ── Helper ────────────────────────────────────────────────────────────

    inline DWORD to_d3d_color(color c) {
        auto clamp = [](float v) -> BYTE {
            return static_cast<BYTE>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
        };
        return D3DCOLOR_RGBA(clamp(c.r), clamp(c.g), clamp(c.b), clamp(c.a));
    }

    // ── Device ops ────────────────────────────────────────────────────────

    d3d9_device *d3d9_create_device();
    void d3d9_destroy_device(device_handle *h);

    // ── Texture ops ───────────────────────────────────────────────────────

    texture_handle *d3d9_create_texture(
        device_handle *dev,
        pixel_format fmt,
        vec2i size,
        int mip_levels,
        texture_role role,
        texture_usage usage
    );
    void d3d9_destroy_texture(texture_handle *h);
    pixel_format d3d9_texture_format(const texture_handle *h);
    int d3d9_texture_width(const texture_handle *h);
    int d3d9_texture_height(const texture_handle *h);
    int d3d9_texture_mip_levels(const texture_handle *h);
    sampler_state d3d9_texture_sampler(const texture_handle *h);
    void d3d9_texture_set_sampler(texture_handle *h, const sampler_state &s);
    bool d3d9_texture_lock(texture_handle *h, rect_i region, int level, texture_lock_mode mode, texture_lock_info &out);
    void d3d9_texture_unlock(texture_handle *h, const texture_lock_info &info, bool wrote);
    void d3d9_texture_generate_mipmaps(texture_handle *h);
    texture_handle *d3d9_texture_clone(const texture_handle *h);
    render_target_scope_handle *d3d9_texture_begin_render_target(device_handle *dev, texture_handle *h, int level);
    void d3d9_texture_end_render_target(device_handle *dev, render_target_scope_handle *h);
    bool d3d9_copy_render_target_to_texture(
        device_handle *dev,
        texture_handle *dst,
        rect_i src_rect,
        vec2i src_target_size,
        vec2i dst_pos,
        int dst_level
    );

    // -- Primitive buffer ops ----------------------------------------------

    vertex_buffer_handle *d3d9_create_vertex_buffer(
        device_handle *dev, int vertex_stride, int vertex_count, buffer_usage usage, const void *initial_data
    );
    void d3d9_destroy_vertex_buffer(vertex_buffer_handle *h);
    int d3d9_vertex_buffer_count(const vertex_buffer_handle *h);
    int d3d9_vertex_buffer_stride(const vertex_buffer_handle *h);
    buffer_usage d3d9_vertex_buffer_usage(const vertex_buffer_handle *h);
    bool d3d9_vertex_buffer_lock(vertex_buffer_handle *h, int first_vertex, int vertex_count, buffer_lock_mode mode, buffer_lock_info &out);
    void d3d9_vertex_buffer_unlock(vertex_buffer_handle *h, const buffer_lock_info &info, bool wrote);

    index_buffer_handle *d3d9_create_index_buffer(
        device_handle *dev, int index_count, buffer_usage usage, const uint32_t *initial_data
    );
    void d3d9_destroy_index_buffer(index_buffer_handle *h);
    int d3d9_index_buffer_count(const index_buffer_handle *h);
    buffer_usage d3d9_index_buffer_usage(const index_buffer_handle *h);
    bool d3d9_index_buffer_lock(index_buffer_handle *h, int first_index, int index_count, buffer_lock_mode mode, buffer_lock_info &out);
    void d3d9_index_buffer_unlock(index_buffer_handle *h, const buffer_lock_info &info, bool wrote);

    // â”€â”€ Shader ops â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    shader_program_handle *d3d9_create_shader_program(device_handle *dev, const shader_program_desc &desc);
    void d3d9_destroy_shader_program(shader_program_handle *h);
    shader_constant_slot d3d9_shader_lookup_constant(shader_program_handle *h, std::string_view name, shader_type stage);
    void d3d9_shader_set_constant(shader_program_handle *h, const shader_constant_slot &slot, const shader_constant_payload &payload);
    shader_sampler_slot d3d9_shader_lookup_sampler(shader_program_handle *h, std::string_view name, shader_type stage);
    void d3d9_shader_set_sampler(shader_program_handle *h, const shader_sampler_slot &slot, texture_handle *tex);
    void d3d9_bind_shader_program(device_handle *dev, shader_program_handle *h);
    void d3d9_apply_shader_constants(device_handle *dev, shader_program_handle *h);
    void d3d9_apply_shader_samplers(device_handle *dev, shader_program_handle *h);

    // ── Swapchain ops ─────────────────────────────────────────────────────

    swapchain_handle *d3d9_create_swapchain(device_handle *dev, void *native_handle, vec2i size);
    void d3d9_destroy_swapchain(swapchain_handle *h);
    void d3d9_swapchain_begin_frame(swapchain_handle *h);
    void d3d9_swapchain_end_frame(swapchain_handle *h);
    void d3d9_swapchain_present(swapchain_handle *h);
    void d3d9_swapchain_on_resize(swapchain_handle *h, vec2i new_size);

    // ── Draw ops ──────────────────────────────────────────────────────────

    void d3d9_set_viewport(device_handle *dev, const render_viewport &viewport);
    void d3d9_clear_render_target(device_handle *dev, color c);
    void d3d9_set_render_state(device_handle *dev, const render_state &state);
    void d3d9_set_blend_state(device_handle *dev, const blend_state &state);
    void d3d9_set_fixed_function_matrices(device_handle *dev, const fixed_function_matrices &matrices);
    void d3d9_set_fixed_function_texture_mode(device_handle *dev, fixed_function_texture_mode mode);
    void d3d9_bind_texture(device_handle *dev, const texture_binding &binding);
    void d3d9_set_texture_sampler(device_handle *dev, const texture_sampler_binding &binding);
    void d3d9_bind_vertex_input(device_handle *dev, const vertex_input_binding &binding);
    void d3d9_unbind_vertex_input(device_handle *dev, const vertex_input_binding &binding);
    void d3d9_draw_arrays_immediate(device_handle *dev, const draw_arrays_immediate &draw);
    void d3d9_draw_elements_immediate(device_handle *dev, const draw_elements_immediate &draw);
    void d3d9_draw_arrays_buffered(device_handle *dev, const draw_arrays_buffered &draw);
    void d3d9_draw_elements_buffered(device_handle *dev, const draw_elements_buffered &draw);

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
#endif // ALIA_GFX_BACKEND_D3D9_OPS_HPP
