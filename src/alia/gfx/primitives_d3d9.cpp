#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "detail/d3d9_backend.hpp"
#include <unordered_map>
#include <typeindex>
#include <any>
#include <vector>

namespace alia {

// ── Compiled vertex declaration cache ────────────────────────────────

struct d3d9_compiled_vtx {
    IDirect3DVertexDeclaration9* decl = nullptr;
};

static std::unordered_map<std::type_index, std::any>& vtx_cache() {
    static std::unordered_map<std::type_index, std::any> cache;
    return cache;
}

// Single-slot cache: avoids the map lookup when the same type is used consecutively.
static std::type_index              s_last_type{typeid(void)};
static IDirect3DVertexDeclaration9* s_last_decl = nullptr;

static IDirect3DVertexDeclaration9* get_or_compile(
    IDirect3DDevice9* device,
    std::type_index vtx_type,
    std::span<const vertex_element> elements)
{
    if (vtx_type == s_last_type)
        return s_last_decl;

    auto& cache = vtx_cache();
    auto it = cache.find(vtx_type);
    if (it != cache.end()) {
        s_last_type = vtx_type;
        s_last_decl = std::any_cast<d3d9_compiled_vtx&>(it->second).decl;
        return s_last_decl;
    }

    // Build D3DVERTEXELEMENT9 array from vertex_element descriptors
    std::vector<D3DVERTEXELEMENT9> d3d_elems;
    for (const auto& e : elements) {
        D3DVERTEXELEMENT9 d3d_e = {};
        d3d_e.Stream = 0;
        d3d_e.Offset = static_cast<WORD>(e.offset);
        d3d_e.Method = D3DDECLMETHOD_DEFAULT;

        switch (e.attribute) {
        case vertex_attr::position:
            d3d_e.Usage = D3DDECLUSAGE_POSITION;
            d3d_e.UsageIndex = 0;
            break;
        case vertex_attr::color_attr:
            d3d_e.Usage = D3DDECLUSAGE_COLOR;
            d3d_e.UsageIndex = 0;
            break;
        case vertex_attr::tex_coord:
            d3d_e.Usage = D3DDECLUSAGE_TEXCOORD;
            d3d_e.UsageIndex = 0;
            break;
        }

        switch (e.storage) {
        case vertex_storage::float_2: d3d_e.Type = D3DDECLTYPE_FLOAT2; break;
        case vertex_storage::float_3: d3d_e.Type = D3DDECLTYPE_FLOAT3; break;
        case vertex_storage::float_4: d3d_e.Type = D3DDECLTYPE_FLOAT4; break;
        }

        d3d_elems.push_back(d3d_e);
    }
    d3d_elems.push_back(D3DDECL_END());

    IDirect3DVertexDeclaration9* decl = nullptr;
    HRESULT hr = device->CreateVertexDeclaration(d3d_elems.data(), &decl);
    if (FAILED(hr))
        return nullptr;

    cache[vtx_type] = d3d9_compiled_vtx{decl};
    s_last_type = vtx_type;
    s_last_decl = decl;
    return decl;
}

// ── Sampler state application ─────────────────────────────────────────

static void apply_d3d9_sampler(IDirect3DDevice9* device, DWORD stage, const sampler_state& s) {
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
    device->SetSamplerState(stage, D3DSAMP_ADDRESSU,  addr(s.wrap_u));
    device->SetSamplerState(stage, D3DSAMP_ADDRESSV,  addr(s.wrap_v));
}

// ── Helpers ──────────────────────────────────────────────────────────

static D3DPRIMITIVETYPE to_d3d_prim(prim_type type) {
    switch (type) {
    case prim_type::triangle_list:  return D3DPT_TRIANGLELIST;
    case prim_type::triangle_strip: return D3DPT_TRIANGLESTRIP;
    case prim_type::triangle_fan:   return D3DPT_TRIANGLEFAN;
    }
    return D3DPT_TRIANGLELIST;
}

static UINT compute_prim_count(prim_type type, int vertex_count) {
    switch (type) {
    case prim_type::triangle_list:  return static_cast<UINT>(vertex_count) / 3;
    case prim_type::triangle_strip: return static_cast<UINT>(vertex_count) - 2;
    case prim_type::triangle_fan:   return static_cast<UINT>(vertex_count) - 2;
    }
    return 0;
}

// ── Drawing ──────────────────────────────────────────────────────────

void d3d9_swapchain_impl::draw_prim(
    prim_type type,
    const void* vertices, int count, int stride,
    std::type_index vtx_type,
    std::span<const vertex_element> elements)
{
    if (count < 3) return;

    auto* decl = get_or_compile(device, vtx_type, elements);
    if (!decl) return;

    device->SetVertexDeclaration(decl);
    device->DrawPrimitiveUP(to_d3d_prim(type),
        compute_prim_count(type, count),
        vertices,
        static_cast<UINT>(stride));
}

void d3d9_swapchain_impl::draw_indexed_prim(
    prim_type type,
    const void* vertices, int count, int stride,
    std::span<const uint32_t> indices,
    std::type_index vtx_type,
    std::span<const vertex_element> elements)
{
    const UINT ni = static_cast<UINT>(indices.size());
    if (ni < 3 || count == 0) return;

    auto* decl = get_or_compile(device, vtx_type, elements);
    if (!decl) return;

    device->SetVertexDeclaration(decl);
    device->DrawIndexedPrimitiveUP(to_d3d_prim(type),
        0,
        static_cast<UINT>(count),
        compute_prim_count(type, static_cast<int>(ni)),
        indices.data(),
        D3DFMT_INDEX32,
        vertices,
        static_cast<UINT>(stride));
}

void d3d9_swapchain_impl::draw_textured_prim(
    prim_type type,
    const void* vertices, int count, int stride,
    std::type_index vtx_type,
    std::span<const vertex_element> elements,
    texture_impl* tex)
{
    if (count < 3 || !tex) return;
    auto* d3d_tex = static_cast<d3d9_texture_impl*>(tex);
    auto* decl = get_or_compile(device, vtx_type, elements);
    if (!decl) return;

    device->SetTexture(0, d3d_tex->texture_);
    apply_d3d9_sampler(device, 0, d3d_tex->sampler_);
    device->SetVertexDeclaration(decl);
    device->DrawPrimitiveUP(to_d3d_prim(type),
        compute_prim_count(type, count), vertices, static_cast<UINT>(stride));
    device->SetTexture(0, nullptr);
}

void d3d9_swapchain_impl::draw_textured_indexed_prim(
    prim_type type,
    const void* vertices, int count, int stride,
    std::span<const uint32_t> indices,
    std::type_index vtx_type,
    std::span<const vertex_element> elements,
    texture_impl* tex)
{
    const UINT ni = static_cast<UINT>(indices.size());
    if (ni < 3 || count == 0 || !tex) return;
    auto* d3d_tex = static_cast<d3d9_texture_impl*>(tex);
    auto* decl = get_or_compile(device, vtx_type, elements);
    if (!decl) return;

    device->SetTexture(0, d3d_tex->texture_);
    apply_d3d9_sampler(device, 0, d3d_tex->sampler_);
    device->SetVertexDeclaration(decl);
    device->DrawIndexedPrimitiveUP(to_d3d_prim(type),
        0, static_cast<UINT>(count),
        compute_prim_count(type, static_cast<int>(ni)),
        indices.data(), D3DFMT_INDEX32,
        vertices, static_cast<UINT>(stride));
    device->SetTexture(0, nullptr);
}

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
