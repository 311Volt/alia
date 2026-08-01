#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"
#include "../gfx_device.hpp"

namespace alia {

    static created_device create_d3d9_device_and_interface() {
        d3d9_device *raw = d3d9_create_device();
        if (!raw)
            return {nullptr, {}};

        // Probe hardware capabilities for conditional op support.
        D3DCAPS9 caps = raw->caps;
        const bool can_autogen_mipmaps = (caps.Caps2 & D3DCAPS2_CANAUTOGENMIPMAP) != 0;
        const bool can_use_shaders =
            D3DSHADER_VERSION_MAJOR(caps.VertexShaderVersion) >= 2 &&
            D3DSHADER_VERSION_MAJOR(caps.PixelShaderVersion) >= 2;

        // Example of how reason_unsupported is used for a future backend-level op:
        // .some_future_op = { nullptr, "D3D9 backend does not support <op>" }

        graphics_backend_interface iface;
        iface.id = gfx_backend::d3d9;
        iface.pixel_center_offset = {-0.5f, -0.5f};

        iface.destroy_device      = {d3d9_destroy_device};

        iface.create_texture              = {d3d9_create_texture};
        iface.destroy_texture             = {d3d9_destroy_texture};
        iface.texture_format              = {d3d9_texture_format};
        iface.texture_width               = {d3d9_texture_width};
        iface.texture_height              = {d3d9_texture_height};
        iface.texture_mip_levels          = {d3d9_texture_mip_levels};
        iface.texture_sampler             = {d3d9_texture_sampler};
        iface.texture_set_sampler         = {d3d9_texture_set_sampler};
        iface.texture_lock                = {d3d9_texture_lock};
        iface.texture_unlock              = {d3d9_texture_unlock};
        iface.texture_clone               = {d3d9_texture_clone};
        iface.copy_render_target_to_texture = {d3d9_copy_render_target_to_texture};

        if (can_autogen_mipmaps) {
            iface.texture_generate_mipmaps = {d3d9_texture_generate_mipmaps};
        } else {
            iface.texture_generate_mipmaps = {
                nullptr,
                "device lacks D3DCAPS2_CANAUTOGENMIPMAP"
            };
        }

        iface.create_vertex_buffer  = {d3d9_create_vertex_buffer};
        iface.destroy_vertex_buffer = {d3d9_destroy_vertex_buffer};
        iface.vertex_buffer_count   = {d3d9_vertex_buffer_count};
        iface.vertex_buffer_stride  = {d3d9_vertex_buffer_stride};
        iface.vertex_buffer_usage   = {d3d9_vertex_buffer_usage};
        iface.vertex_buffer_lock    = {d3d9_vertex_buffer_lock};
        iface.vertex_buffer_unlock  = {d3d9_vertex_buffer_unlock};
        iface.create_index_buffer   = {d3d9_create_index_buffer};
        iface.destroy_index_buffer  = {d3d9_destroy_index_buffer};
        iface.index_buffer_count    = {d3d9_index_buffer_count};
        iface.index_buffer_usage    = {d3d9_index_buffer_usage};
        iface.index_buffer_lock     = {d3d9_index_buffer_lock};
        iface.index_buffer_unlock   = {d3d9_index_buffer_unlock};

        if (can_use_shaders) {
            iface.create_shader_program = {d3d9_create_shader_program};
        } else {
            iface.create_shader_program = {
                nullptr,
                "device lacks D3D9 vertex/pixel shader model 2.0 support"
            };
        }
        iface.destroy_shader_program = {d3d9_destroy_shader_program};
        iface.shader_lookup_constant = {d3d9_shader_lookup_constant};
        iface.shader_set_constant    = {d3d9_shader_set_constant};
        iface.shader_lookup_sampler  = {d3d9_shader_lookup_sampler};
        iface.shader_set_sampler     = {d3d9_shader_set_sampler};

        iface.create_swapchain   = {d3d9_create_swapchain};
        iface.destroy_swapchain  = {d3d9_destroy_swapchain};
        iface.swapchain_begin_frame = {d3d9_swapchain_begin_frame};
        iface.swapchain_end_frame   = {d3d9_swapchain_end_frame};
        iface.swapchain_present     = {d3d9_swapchain_present};
        iface.swapchain_on_resize   = {d3d9_swapchain_on_resize};

        iface.create_pipeline             = {d3d9_create_pipeline};
        iface.destroy_pipeline            = {d3d9_destroy_pipeline};
        iface.update_pipeline             = {d3d9_update_pipeline};
        iface.bind_pipeline               = {d3d9_bind_pipeline};
        iface.begin_render_pass           = {d3d9_begin_render_pass};
        iface.end_render_pass             = {d3d9_end_render_pass};
        iface.set_viewport                = {d3d9_set_viewport};
        iface.bind_vertex_buffer          = {d3d9_bind_vertex_buffer};
        iface.bind_index_buffer           = {d3d9_bind_index_buffer};
        iface.upload_transient_vertex_data = {d3d9_upload_transient_vertex_data};
        iface.upload_transient_index_data = {d3d9_upload_transient_index_data};
        iface.bind_resources              = {d3d9_bind_resources};
        iface.draw                        = {d3d9_draw};
        iface.draw_indexed                = {d3d9_draw_indexed};

        return {raw, std::move(iface)};
    }

    void register_d3d9_backend() {
        register_gfx_backend({gfx_backend::d3d9, create_d3d9_device_and_interface});
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
