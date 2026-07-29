#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"
#include <cstring>
#include <vector>

namespace alia {

    static IDirect3DVertexDeclaration9 *
    get_or_compile(
        d3d9_device &device,
        const vertex_definition_view &definition
    ) {
        if (device.vertex_definitions.size() <= definition.index)
            device.vertex_definitions.resize(definition.index + 1);

        auto &slot = device.vertex_definitions[definition.index];
        if (slot)
            return slot->declaration;

        std::vector<D3DVERTEXELEMENT9> d3d_elems;
        for (const auto &e : definition.elements) {
            D3DVERTEXELEMENT9 d3d_e = {};
            d3d_e.Stream = 0;
            d3d_e.Offset = static_cast<WORD>(e.offset);
            d3d_e.Method = D3DDECLMETHOD_DEFAULT;

            switch (e.attribute) {
            case vertex_attr::position:   d3d_e.Usage = D3DDECLUSAGE_POSITION; d3d_e.UsageIndex = 0; break;
            case vertex_attr::normal:     d3d_e.Usage = D3DDECLUSAGE_NORMAL;   d3d_e.UsageIndex = 0; break;
            case vertex_attr::color_attr: d3d_e.Usage = D3DDECLUSAGE_COLOR;    d3d_e.UsageIndex = 0; break;
            case vertex_attr::tex_coord:  d3d_e.Usage = D3DDECLUSAGE_TEXCOORD; d3d_e.UsageIndex = 0; break;
            }
            switch (e.storage) {
            case vertex_storage::float_2: d3d_e.Type = D3DDECLTYPE_FLOAT2; break;
            case vertex_storage::float_3: d3d_e.Type = D3DDECLTYPE_FLOAT3; break;
            case vertex_storage::float_4: d3d_e.Type = D3DDECLTYPE_FLOAT4; break;
            }

            d3d_elems.push_back(d3d_e);
        }
        d3d_elems.push_back(D3DDECL_END());

        IDirect3DVertexDeclaration9 *decl = nullptr;
        if (FAILED(device.device->CreateVertexDeclaration(d3d_elems.data(), &decl)))
            return nullptr;

        slot.emplace(decl);
        return slot->declaration;
    }

    static D3DMATRIX make_identity_matrix() {
        D3DMATRIX m;
        std::memset(&m, 0, sizeof(m));
        m._11 = m._22 = m._33 = m._44 = 1.0f;
        return m;
    }

    static void apply_d3d9_sampler(IDirect3DDevice9 *device, DWORD stage, const sampler_state &s) {
        auto filt = [](texture_filter f) -> DWORD {
            return f == texture_filter::nearest ? D3DTEXF_POINT : D3DTEXF_LINEAR;
        };
        auto addr = [](texture_wrap w) -> DWORD {
            switch (w) {
            case texture_wrap::clamp:  return D3DTADDRESS_CLAMP;
            case texture_wrap::repeat: return D3DTADDRESS_WRAP;
            case texture_wrap::mirror: return D3DTADDRESS_MIRROR;
            }
            return D3DTADDRESS_CLAMP;
        };
        device->SetSamplerState(stage, D3DSAMP_MINFILTER, filt(s.min_filter));
        device->SetSamplerState(stage, D3DSAMP_MAGFILTER, filt(s.mag_filter));
        device->SetSamplerState(stage, D3DSAMP_MIPFILTER, filt(s.mip_filter));
        device->SetSamplerState(stage, D3DSAMP_ADDRESSU, addr(s.wrap_u));
        device->SetSamplerState(stage, D3DSAMP_ADDRESSV, addr(s.wrap_v));
    }

    static void apply_d3d9_vertex_color(IDirect3DDevice9 *device) {
        device->SetTexture(0, nullptr);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }

    static void apply_d3d9_texture_color(IDirect3DDevice9 *device, bool modulate) {
        if (modulate) {
            device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
            device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
            device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
            device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        } else {
            device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        }
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }

    static void apply_d3d9_alpha_mask_color(IDirect3DDevice9 *device) {
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }

    static D3DPRIMITIVETYPE to_d3d_prim(prim_type type) {
        switch (type) {
        case prim_type::triangle_list:  return D3DPT_TRIANGLELIST;
        case prim_type::triangle_strip: return D3DPT_TRIANGLESTRIP;
        case prim_type::triangle_fan:   return D3DPT_TRIANGLEFAN;
        }
        return D3DPT_TRIANGLELIST;
    }

    static DWORD to_d3d_cull(cull_mode mode) {
        switch (mode) {
        case cull_mode::none:              return D3DCULL_NONE;
        case cull_mode::clockwise:         return D3DCULL_CW;
        case cull_mode::counter_clockwise: return D3DCULL_CCW;
        }
        return D3DCULL_NONE;
    }

    static DWORD to_d3d_blend_factor(blend_factor factor) {
        switch (factor) {
        case blend_factor::zero:          return D3DBLEND_ZERO;
        case blend_factor::one:           return D3DBLEND_ONE;
        case blend_factor::src_alpha:     return D3DBLEND_SRCALPHA;
        case blend_factor::inv_src_alpha: return D3DBLEND_INVSRCALPHA;
        }
        return D3DBLEND_ONE;
    }

    static DWORD to_d3d_blend_op(blend_op op) {
        switch (op) {
        case blend_op::add: return D3DBLENDOP_ADD;
        }
        return D3DBLENDOP_ADD;
    }

    void d3d9_set_viewport(device_handle *dev_h, const render_viewport &viewport) {
        auto *device = as_d3d9_device(dev_h)->device;
        D3DVIEWPORT9 vp = {
            static_cast<DWORD>(viewport.origin.x),
            static_cast<DWORD>(viewport.origin.y),
            static_cast<DWORD>(viewport.size.x),
            static_cast<DWORD>(viewport.size.y),
            viewport.min_depth,
            viewport.max_depth
        };
        device->SetViewport(&vp);
    }

    void d3d9_clear_render_target(device_handle *dev_h, color c) {
        auto *device = as_d3d9_device(dev_h)->device;
        device->Clear(0, nullptr, D3DCLEAR_TARGET, to_d3d_color(c), 1.0f, 0);
    }

    void d3d9_set_render_state(device_handle *dev_h, const render_state &state) {
        auto *device = as_d3d9_device(dev_h)->device;
        device->SetRenderState(D3DRS_ZENABLE, state.depth_test_enabled ? D3DZB_TRUE : D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, state.depth_write_enabled ? TRUE : FALSE);
        device->SetRenderState(D3DRS_LIGHTING, state.fixed_function_lighting_enabled ? TRUE : FALSE);
        device->SetRenderState(D3DRS_CULLMODE, to_d3d_cull(state.cull));
    }

    void d3d9_set_blend_state(device_handle *dev_h, const blend_state &state) {
        auto *device = as_d3d9_device(dev_h)->device;
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, state.enabled ? TRUE : FALSE);
        if (!state.enabled)
            return;
        device->SetRenderState(D3DRS_BLENDOP, to_d3d_blend_op(state.op));
        device->SetRenderState(D3DRS_SRCBLEND, to_d3d_blend_factor(state.src));
        device->SetRenderState(D3DRS_DESTBLEND, to_d3d_blend_factor(state.dst));
    }

    void d3d9_set_fixed_function_matrices(device_handle *dev_h, const fixed_function_matrices &matrices) {
        auto *device = as_d3d9_device(dev_h)->device;
        device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX *>(matrices.world.data()));
        D3DMATRIX view = make_identity_matrix();
        device->SetTransform(D3DTS_VIEW, &view);
        device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX *>(matrices.projection.data()));
    }

    void d3d9_set_fixed_function_texture_mode(device_handle *dev_h, fixed_function_texture_mode mode) {
        auto *device = as_d3d9_device(dev_h)->device;
        switch (mode) {
        case fixed_function_texture_mode::vertex_color:
            apply_d3d9_vertex_color(device);
            break;
        case fixed_function_texture_mode::texture_replace:
            apply_d3d9_texture_color(device, false);
            break;
        case fixed_function_texture_mode::texture_modulate_vertex_color:
            apply_d3d9_texture_color(device, true);
            break;
        case fixed_function_texture_mode::alpha_mask:
            apply_d3d9_alpha_mask_color(device);
            break;
        }
    }

    void d3d9_bind_texture(device_handle *dev_h, const texture_binding &binding) {
        auto *device = as_d3d9_device(dev_h)->device;
        auto *texture = binding.texture ? as_d3d9_texture(binding.texture) : nullptr;
        device->SetTexture(static_cast<DWORD>(binding.slot), texture ? texture->texture : nullptr);
    }

    void d3d9_set_texture_sampler(device_handle *dev_h, const texture_sampler_binding &binding) {
        auto *device = as_d3d9_device(dev_h)->device;
        apply_d3d9_sampler(device, static_cast<DWORD>(binding.slot), binding.sampler);
    }

    void d3d9_bind_vertex_input(device_handle *dev_h, const vertex_input_binding &binding) {
        auto &device = *as_d3d9_device(dev_h);
        IDirect3DVertexDeclaration9 *decl = get_or_compile(device, binding.definition);
        device.device->SetVertexDeclaration(decl);
        if (binding.buffer) {
            auto *buffer = as_d3d9_vertex_buffer(binding.buffer);
            device.device->SetStreamSource(
                0,
                buffer->buffer,
                static_cast<UINT>(binding.vertex_offset_bytes),
                static_cast<UINT>(binding.definition.stride)
            );
        } else {
            device.device->SetStreamSource(0, nullptr, 0, 0);
        }
    }

    void d3d9_unbind_vertex_input(device_handle *dev_h, const vertex_input_binding &binding) {
        if (!binding.buffer)
            return;
        auto *device = as_d3d9_device(dev_h)->device;
        device->SetStreamSource(0, nullptr, 0, 0);
    }

    void d3d9_draw_arrays_immediate(device_handle *dev_h, const draw_arrays_immediate &draw) {
        auto *device = as_d3d9_device(dev_h)->device;
        device->DrawPrimitiveUP(
            to_d3d_prim(draw.type),
            static_cast<UINT>(draw.primitive_count),
            draw.vertices,
            static_cast<UINT>(draw.vertex_stride)
        );
    }

    void d3d9_draw_elements_immediate(device_handle *dev_h, const draw_elements_immediate &draw) {
        auto *device = as_d3d9_device(dev_h)->device;
        device->DrawIndexedPrimitiveUP(
            to_d3d_prim(draw.type),
            0,
            static_cast<UINT>(draw.vertex_count),
            static_cast<UINT>(draw.primitive_count),
            draw.indices.data(),
            D3DFMT_INDEX32,
            draw.vertices,
            static_cast<UINT>(draw.vertex_stride)
        );
    }

    void d3d9_draw_arrays_buffered(device_handle *dev_h, const draw_arrays_buffered &draw) {
        auto *device = as_d3d9_device(dev_h)->device;
        device->DrawPrimitive(
            to_d3d_prim(draw.type),
            static_cast<UINT>(draw.first_vertex),
            static_cast<UINT>(draw.primitive_count)
        );
    }

    void d3d9_draw_elements_buffered(device_handle *dev_h, const draw_elements_buffered &draw) {
        auto *device = as_d3d9_device(dev_h)->device;
        auto *indices = as_d3d9_index_buffer(draw.indices);
        device->SetIndices(indices->buffer);
        device->DrawIndexedPrimitive(
            to_d3d_prim(draw.type),
            0,
            0,
            static_cast<UINT>(draw.vertex_count),
            static_cast<UINT>(draw.first_index),
            static_cast<UINT>(draw.primitive_count)
        );
        device->SetIndices(nullptr);
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
