#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "detail/d3d9_backend.hpp"
#include <vector>

namespace alia {

// ── Vertex conversion helper ──────────────────────────────────────────

static std::vector<d3d9_vertex> to_d3d9(std::span<const colored_vertex> src) {
    std::vector<d3d9_vertex> out;
    out.reserve(src.size());
    for (const auto& v : src)
        out.push_back({v.position.x, v.position.y, -0.5f, to_d3d_color(v.col)});
    return out;
}

// ── Single triangle ───────────────────────────────────────────────────

void d3d9_swapchain_impl::draw_triangle(colored_vertex v0, colored_vertex v1, colored_vertex v2) {
    d3d9_vertex verts[3] = {
        {v0.position.x, v0.position.y, -0.5f, to_d3d_color(v0.col)},
        {v1.position.x, v1.position.y, -0.5f, to_d3d_color(v1.col)},
        {v2.position.x, v2.position.y, -0.5f, to_d3d_color(v2.col)},
    };
    device->SetFVF(d3d9_vertex::FVF);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, verts, sizeof(d3d9_vertex));
}

// ── Unindexed bulk ────────────────────────────────────────────────────

void d3d9_swapchain_impl::draw_triangles(std::span<const colored_vertex> vertices) {
    const UINT n = (UINT)vertices.size();
    if (n < 3) return;
    auto verts = to_d3d9(vertices);
    device->SetFVF(d3d9_vertex::FVF);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, n / 3, verts.data(), sizeof(d3d9_vertex));
}

void d3d9_swapchain_impl::draw_triangle_strip(std::span<const colored_vertex> vertices) {
    const UINT n = (UINT)vertices.size();
    if (n < 3) return;
    auto verts = to_d3d9(vertices);
    device->SetFVF(d3d9_vertex::FVF);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, n - 2, verts.data(), sizeof(d3d9_vertex));
}

void d3d9_swapchain_impl::draw_triangle_fan(std::span<const colored_vertex> vertices) {
    const UINT n = (UINT)vertices.size();
    if (n < 3) return;
    auto verts = to_d3d9(vertices);
    device->SetFVF(d3d9_vertex::FVF);
    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, n - 2, verts.data(), sizeof(d3d9_vertex));
}

// ── Indexed bulk ──────────────────────────────────────────────────────

void d3d9_swapchain_impl::draw_triangles(
    std::span<const colored_vertex> vertices, std::span<const uint32_t> indices)
{
    const UINT ni = (UINT)indices.size();
    const UINT nv = (UINT)vertices.size();
    if (ni < 3 || nv == 0) return;
    auto verts = to_d3d9(vertices);
    device->SetFVF(d3d9_vertex::FVF);
    device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, nv, ni / 3,
        indices.data(), D3DFMT_INDEX32, verts.data(), sizeof(d3d9_vertex));
}

void d3d9_swapchain_impl::draw_triangle_strip(
    std::span<const colored_vertex> vertices, std::span<const uint32_t> indices)
{
    const UINT ni = (UINT)indices.size();
    const UINT nv = (UINT)vertices.size();
    if (ni < 3 || nv == 0) return;
    auto verts = to_d3d9(vertices);
    device->SetFVF(d3d9_vertex::FVF);
    device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLESTRIP, 0, nv, ni - 2,
        indices.data(), D3DFMT_INDEX32, verts.data(), sizeof(d3d9_vertex));
}

void d3d9_swapchain_impl::draw_triangle_fan(
    std::span<const colored_vertex> vertices, std::span<const uint32_t> indices)
{
    const UINT ni = (UINT)indices.size();
    const UINT nv = (UINT)vertices.size();
    if (ni < 3 || nv == 0) return;
    auto verts = to_d3d9(vertices);
    device->SetFVF(d3d9_vertex::FVF);
    device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLEFAN, 0, nv, ni - 2,
        indices.data(), D3DFMT_INDEX32, verts.data(), sizeof(d3d9_vertex));
}

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
