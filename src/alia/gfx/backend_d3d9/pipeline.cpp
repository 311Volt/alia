#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"

#include <cstring>
#include <vector>

namespace alia {
    namespace {
        constexpr int index_size(index_format format) {
            return format == index_format::u16 ? sizeof(uint16_t) : sizeof(uint32_t);
        }

        IDirect3DVertexDeclaration9 *get_or_compile(d3d9_device &device, const vertex_definition_view &definition) {
            if (device.vertex_definitions.size() <= definition.index)
                device.vertex_definitions.resize(definition.index + 1);
            auto &slot = device.vertex_definitions[definition.index];
            if (slot) return slot->declaration;
            std::vector<D3DVERTEXELEMENT9> elements;
            for (const auto &e : definition.elements) {
                D3DVERTEXELEMENT9 out{};
                out.Stream = 0; out.Offset = static_cast<WORD>(e.offset); out.Method = D3DDECLMETHOD_DEFAULT; out.UsageIndex = 0;
                switch (e.attribute) {
                case vertex_attr::position: out.Usage = D3DDECLUSAGE_POSITION; break;
                case vertex_attr::normal: out.Usage = D3DDECLUSAGE_NORMAL; break;
                case vertex_attr::color_attr: out.Usage = D3DDECLUSAGE_COLOR; break;
                case vertex_attr::tex_coord: out.Usage = D3DDECLUSAGE_TEXCOORD; break;
                }
                switch (e.storage) {
                case vertex_storage::float_2: out.Type = D3DDECLTYPE_FLOAT2; break;
                case vertex_storage::float_3: out.Type = D3DDECLTYPE_FLOAT3; break;
                case vertex_storage::float_4: out.Type = D3DDECLTYPE_FLOAT4; break;
                }
                elements.push_back(out);
            }
            elements.push_back(D3DDECL_END());
            IDirect3DVertexDeclaration9 *declaration = nullptr;
            if (FAILED(device.device->CreateVertexDeclaration(elements.data(), &declaration))) return nullptr;
            slot.emplace(declaration);
            return declaration;
        }
        int primitive_count(primitive_topology topology, int count) {
            if (count < 3) return 0;
            switch (topology) {
            case primitive_topology::triangle_list: return count / 3;
            case primitive_topology::triangle_strip:
            case primitive_topology::triangle_fan: return count - 2;
            }
            return 0;
        }
        D3DPRIMITIVETYPE to_d3d(primitive_topology topology) {
            switch (topology) {
            case primitive_topology::triangle_list: return D3DPT_TRIANGLELIST;
            case primitive_topology::triangle_strip: return D3DPT_TRIANGLESTRIP;
            case primitive_topology::triangle_fan: return D3DPT_TRIANGLEFAN;
            }
            return D3DPT_TRIANGLELIST;
        }
        DWORD to_d3d(blend_factor factor) {
            switch (factor) {
            case blend_factor::zero: return D3DBLEND_ZERO;
            case blend_factor::one: return D3DBLEND_ONE;
            case blend_factor::src_alpha: return D3DBLEND_SRCALPHA;
            case blend_factor::inv_src_alpha: return D3DBLEND_INVSRCALPHA;
            }
            return D3DBLEND_ONE;
        }
        DWORD to_d3d(cull_mode mode) {
            switch (mode) {
            case cull_mode::none: return D3DCULL_NONE;
            case cull_mode::clockwise: return D3DCULL_CW;
            case cull_mode::counter_clockwise: return D3DCULL_CCW;
            }
            return D3DCULL_NONE;
        }
        D3DCMPFUNC to_d3d(compare_func compare) {
            switch (compare) {
            case compare_func::never: return D3DCMP_NEVER;
            case compare_func::less: return D3DCMP_LESS;
            case compare_func::equal: return D3DCMP_EQUAL;
            case compare_func::less_equal: return D3DCMP_LESSEQUAL;
            case compare_func::greater: return D3DCMP_GREATER;
            case compare_func::not_equal: return D3DCMP_NOTEQUAL;
            case compare_func::greater_equal: return D3DCMP_GREATEREQUAL;
            case compare_func::always: return D3DCMP_ALWAYS;
            }
            return D3DCMP_LESSEQUAL;
        }
        void apply_sampler(
            IDirect3DDevice9 *device,
            DWORD slot,
            const d3d9_texture &texture,
            const sampler_state &state
        ) {
            const auto filter = [](texture_filter f) { return f == texture_filter::nearest ? D3DTEXF_POINT : D3DTEXF_LINEAR; };
            const auto wrap = [](texture_wrap w) {
                switch (w) { case texture_wrap::clamp: return D3DTADDRESS_CLAMP; case texture_wrap::repeat: return D3DTADDRESS_WRAP; case texture_wrap::mirror: return D3DTADDRESS_MIRROR; }
                return D3DTADDRESS_CLAMP;
            };
            device->SetSamplerState(slot, D3DSAMP_MINFILTER, filter(state.min_filter));
            device->SetSamplerState(slot, D3DSAMP_MAGFILTER, filter(state.mag_filter));
            device->SetSamplerState(slot, D3DSAMP_MIPFILTER, filter(state.mip_filter));
            device->SetSamplerState(slot, D3DSAMP_ADDRESSU, wrap(state.wrap_u));
            device->SetSamplerState(slot, D3DSAMP_ADDRESSV, wrap(state.wrap_v));
            if (texture.cube)
                device->SetSamplerState(slot, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
        }
        void apply_texture_op(IDirect3DDevice9 *device, texture_operation op) {
            switch (op) {
            case texture_operation::vertex_color:
                device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1); device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
                device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
                break;
            case texture_operation::modulate:
                device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE); device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE); device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE); device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE); device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                break;
            case texture_operation::alpha_mask:
                device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1); device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
                device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE); device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE); device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                break;
            }
            device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
            device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        }
        void prepare_draw(d3d9_device &device) {
            if (device.state.shader) {
                d3d9_apply_program_state(device.device, as_d3d9_shader_program(device.state.shader));
                return;
            }
            apply_texture_op(device.device, device.state.texture_op);
            device.device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX *>(&device.transforms.world.m[0][0]));
            device.device->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX *>(&device.transforms.view.m[0][0]));
            device.device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX *>(&device.transforms.projection.m[0][0]));
        }
    }

    void d3d9_set_render_state(device_handle *h, const render_state &state) {
        auto &device = *as_d3d9_device(h);
        if (state.vertex_layout.stride > 0)
            device.device->SetVertexDeclaration(get_or_compile(device, state.vertex_layout));
        if (state.shader) {
            const auto *shader = as_d3d9_shader_program(state.shader);
            device.device->SetVertexShader(shader->vertex_shader);
            device.device->SetPixelShader(shader->pixel_shader);
        } else {
            device.device->SetVertexShader(nullptr); device.device->SetPixelShader(nullptr);
        }
        device.device->SetRenderState(D3DRS_ZENABLE, state.depth.test_enabled ? D3DZB_TRUE : D3DZB_FALSE);
        device.device->SetRenderState(D3DRS_ZWRITEENABLE, state.depth.write_enabled ? TRUE : FALSE);
        device.device->SetRenderState(D3DRS_ZFUNC, to_d3d(state.depth.compare));
        device.device->SetRenderState(D3DRS_ALPHABLENDENABLE, state.blend.enabled ? TRUE : FALSE);
        device.device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        device.device->SetRenderState(D3DRS_SRCBLEND, to_d3d(state.blend.src));
        device.device->SetRenderState(D3DRS_DESTBLEND, to_d3d(state.blend.dst));
        device.device->SetRenderState(D3DRS_CULLMODE, to_d3d(state.raster.cull));
        device.device->SetRenderState(D3DRS_LIGHTING, state.lighting ? TRUE : FALSE);
        // D3D9 can still blend fixed-function fog after a ps_2_0 shader.
        device.device->SetRenderState(D3DRS_FOGENABLE, state.fog && !state.shader ? TRUE : FALSE);
        device.device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
        device.device->SetRenderState(D3DRS_LOCALVIEWER, TRUE);
        device.device->SetRenderState(D3DRS_COLORVERTEX, state.vertex_color_material ? TRUE : FALSE);
        const DWORD color_source = state.vertex_color_material ? D3DMCS_COLOR1 : D3DMCS_MATERIAL;
        device.device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, color_source);
        device.device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, color_source);
        device.device->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
        device.device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
        const auto &specular = device.surface_material.specular;
        const bool has_specular = specular.r != 0.0f || specular.g != 0.0f || specular.b != 0.0f;
        device.device->SetRenderState(D3DRS_SPECULARENABLE, state.lighting && has_specular ? TRUE : FALSE);
        device.state = state;
    }
    void d3d9_set_transforms(device_handle *h, const transform_state &transforms) {
        as_d3d9_device(h)->transforms = transforms;
    }
    bool d3d9_set_render_target(device_handle *h, const render_target_info &info) {
        auto &device = *as_d3d9_device(h);
        HRESULT result = E_FAIL;
        if (info.swapchain) {
            auto *swapchain = as_d3d9_swapchain(info.swapchain);
            IDirect3DSurface9 *backbuffer = nullptr;
            if (SUCCEEDED(swapchain->swap_chain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer))) {
                result = device.device->SetRenderTarget(0, backbuffer);
                backbuffer->Release();
            }
            if (SUCCEEDED(result)) result = device.device->SetDepthStencilSurface(swapchain->depth_stencil);
        } else if (info.target_texture) {
            IDirect3DSurface9 *surface = nullptr;
            auto *texture = as_d3d9_texture(info.target_texture);
            if (SUCCEEDED(d3d9_get_level_surface(
                    *texture, info.target_face, info.target_level, &surface))) {
                result = device.device->SetRenderTarget(0, surface);
                surface->Release();
            }
            if (SUCCEEDED(result)) result = device.device->SetDepthStencilSurface(nullptr);
        }
        if (FAILED(result)) return false;
        d3d9_set_viewport(h, {{}, info.target_size, 0.0f, 1.0f});
        return true;
    }
    bool d3d9_clear(device_handle *h, const std::optional<color> &clear_color, const std::optional<float> &clear_depth) {
        auto &device = *as_d3d9_device(h);
        DWORD clear_flags = 0;
        DWORD clear_color_value = 0;
        if (clear_color) { clear_flags |= D3DCLEAR_TARGET; clear_color_value = to_d3d_color(*clear_color); }
        if (clear_depth) clear_flags |= D3DCLEAR_ZBUFFER;
        if (clear_flags && FAILED(device.device->Clear(0, nullptr, clear_flags, clear_color_value, clear_depth.value_or(1.0f), 0))) return false;
        return true;
    }
    void d3d9_reset_frame_state(d3d9_device &device) {
        device.device->SetStreamSource(0, nullptr, 0, 0); device.device->SetIndices(nullptr);
        device.state = {}; device.transforms = {};
        device.surface_material = {};
        for (int index = 0; index != device.enabled_light_count; ++index)
            device.device->LightEnable(static_cast<DWORD>(index), FALSE);
        device.enabled_light_count = 0;
        device.device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device.device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
        device.device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device.device->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
        device.device->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
        device.current_vb = nullptr; device.current_ib = nullptr;
        device.transient_vertices = nullptr; device.transient_vertex_bytes = 0; device.transient_indices = nullptr; device.transient_index_count = 0;
    }
    void d3d9_set_viewport(device_handle *h, const render_viewport &viewport) {
        D3DVIEWPORT9 value{static_cast<DWORD>(viewport.origin.x), static_cast<DWORD>(viewport.origin.y), static_cast<DWORD>(viewport.size.x), static_cast<DWORD>(viewport.size.y), viewport.min_depth, viewport.max_depth};
        as_d3d9_device(h)->device->SetViewport(&value);
    }
    void d3d9_bind_vertex_buffer(device_handle *h, vertex_buffer_handle *buffer) {
        auto &device = *as_d3d9_device(h); device.current_vb = buffer ? as_d3d9_vertex_buffer(buffer) : nullptr; device.transient_vertices = nullptr; device.transient_vertex_bytes = 0;
    }
    void d3d9_bind_index_buffer(device_handle *h, index_buffer_handle *buffer) {
        auto &device = *as_d3d9_device(h); device.current_ib = buffer ? as_d3d9_index_buffer(buffer) : nullptr; device.transient_indices = nullptr; device.transient_index_count = 0;
    }
    void d3d9_upload_transient_vertex_data(device_handle *h, const void *data, int bytes) {
        auto &device = *as_d3d9_device(h); device.current_vb = nullptr; device.transient_vertices = data; device.transient_vertex_bytes = bytes;
    }
    void d3d9_upload_transient_index_data(device_handle *h, const void *indices, int index_count, index_format format) {
        auto &device = *as_d3d9_device(h); device.current_ib = nullptr; device.transient_indices = indices; device.transient_index_count = index_count; device.transient_index_format = format;
    }
    void d3d9_bind_resources(device_handle *h, const texture_sampler_binding &binding) {
        auto *device = as_d3d9_device(h)->device; auto *texture = binding.texture ? as_d3d9_texture(binding.texture) : nullptr;
        device->SetTexture(
            static_cast<DWORD>(binding.slot),
            texture ? d3d9_base_texture(*texture) : nullptr);
        if (texture)
            apply_sampler(
                device, static_cast<DWORD>(binding.slot), *texture,
                binding.sampler);
    }
    void d3d9_draw(device_handle *h, primitive_topology topology, int vertex_count, int first_vertex) {
        auto &device = *as_d3d9_device(h); prepare_draw(device); const int count = primitive_count(topology, vertex_count); if (!count) return;
        const int stride = device.state.vertex_layout.stride;
        if (device.current_vb) {
            device.device->SetStreamSource(0, device.current_vb->buffer, 0, static_cast<UINT>(stride));
            device.device->DrawPrimitive(to_d3d(topology), static_cast<UINT>(first_vertex), static_cast<UINT>(count));
        } else {
            const auto *base = static_cast<const std::byte *>(device.transient_vertices) + first_vertex * stride;
            device.device->DrawPrimitiveUP(to_d3d(topology), static_cast<UINT>(count), base, static_cast<UINT>(stride));
        }
    }
    void d3d9_draw_indexed(device_handle *h, primitive_topology topology, int index_count, int first_index, int base_vertex) {
        auto &device = *as_d3d9_device(h); prepare_draw(device); const int count = primitive_count(topology, index_count); if (!count) return;
        const int stride = device.state.vertex_layout.stride;
        if (device.current_vb) {
            device.device->SetStreamSource(0, device.current_vb->buffer, 0, static_cast<UINT>(stride));
            device.device->SetIndices(device.current_ib->buffer);
            device.device->DrawIndexedPrimitive(to_d3d(topology), base_vertex, 0, static_cast<UINT>(device.current_vb->count - base_vertex), static_cast<UINT>(first_index), static_cast<UINT>(count));
        } else {
            const auto *indices = static_cast<const std::byte *>(device.transient_indices) + first_index * index_size(device.transient_index_format);
            device.device->DrawIndexedPrimitiveUP(to_d3d(topology), 0, static_cast<UINT>(device.transient_vertex_bytes / stride - base_vertex), static_cast<UINT>(count), indices, to_d3d(device.transient_index_format), static_cast<const std::byte *>(device.transient_vertices) + base_vertex * stride, static_cast<UINT>(stride));
        }
    }
} // namespace alia

#endif
