#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"
#include "../gfx_device.hpp"

namespace alia {

    static created_device create_d3d9_device_and_interface() {
        d3d9_device *raw = d3d9_create_device();
        if (!raw)
            return {nullptr, {}};

        // Probe hardware capabilities for conditional op support.
        D3DCAPS9 caps = {};
        raw->device->GetDeviceCaps(&caps);
        const bool can_autogen_mipmaps = (caps.Caps2 & D3DCAPS2_CANAUTOGENMIPMAP) != 0;

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

        if (can_autogen_mipmaps) {
            iface.texture_generate_mipmaps = {d3d9_texture_generate_mipmaps};
        } else {
            iface.texture_generate_mipmaps = {
                nullptr,
                "device lacks D3DCAPS2_CANAUTOGENMIPMAP"
            };
        }

        iface.create_swapchain   = {d3d9_create_swapchain};
        iface.destroy_swapchain  = {d3d9_destroy_swapchain};
        iface.swapchain_clear    = {d3d9_swapchain_clear};
        iface.swapchain_present  = {d3d9_swapchain_present};
        iface.swapchain_on_resize = {d3d9_swapchain_on_resize};

        iface.draw_prim                  = {d3d9_draw_prim};
        iface.draw_indexed_prim          = {d3d9_draw_indexed_prim};
        iface.draw_textured_prim         = {d3d9_draw_textured_prim};
        iface.draw_alpha_masked_prim     = {d3d9_draw_alpha_masked_prim};
        iface.draw_textured_indexed_prim = {d3d9_draw_textured_indexed_prim};

        return {raw, std::move(iface)};
    }

    void register_d3d9_backend() {
        register_gfx_backend({gfx_backend::d3d9, create_d3d9_device_and_interface});
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
