#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "detail/d3d9_backend.hpp"
#include <cstring>
#include <memory>
#include <stdexcept>

namespace alia {

// ── Hidden dummy window ───────────────────────────────────────────────

static HWND create_dummy_hwnd() {
    static const wchar_t* cls = L"AliaDummy_D3D9";
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = DefWindowProcW;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = cls;
        RegisterClassExW(&wc);
        registered = true;
    }
    return CreateWindowExW(0, cls, L"", WS_OVERLAPPEDWINDOW,
                           0, 0, 1, 1,
                           nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}

// ── D3D9 device impl ──────────────────────────────────────────────────

d3d9_device_impl::~d3d9_device_impl() {
    if (device) { device->Release(); device = nullptr; }
    if (d3d)    { d3d->Release();    d3d    = nullptr; }
    if (dummy)  { DestroyWindow(dummy); dummy = nullptr; }
}

// ── D3D9 swapchain impl ───────────────────────────────────────────────

d3d9_swapchain_impl::~d3d9_swapchain_impl() {
    if (swap_chain) { swap_chain->Release(); swap_chain = nullptr; }
}

void d3d9_swapchain_impl::set_render_target_to_back_buffer() {
    IDirect3DSurface9* bb = nullptr;
    swap_chain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &bb);
    device->SetRenderTarget(0, bb);
    bb->Release();
}

static D3DMATRIX make_identity() {
    D3DMATRIX m;
    std::memset(&m, 0, sizeof(m));
    m._11 = m._22 = m._33 = m._44 = 1.0f;
    return m;
}

void d3d9_swapchain_impl::setup_render_states() {
    device->SetTransform(D3DTS_WORLD,      (const D3DMATRIX*)transform_);
    D3DMATRIX view = make_identity();
    device->SetTransform(D3DTS_VIEW,       &view);
    device->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)projection_);

    device->SetRenderState(D3DRS_LIGHTING,        FALSE);
    device->SetRenderState(D3DRS_CULLMODE,        D3DCULL_NONE);
    device->SetRenderState(D3DRS_ZENABLE,         FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    D3DVIEWPORT9 vp = {0, 0, (DWORD)size.x, (DWORD)size.y, 0.0f, 1.0f};
    device->SetViewport(&vp);
}

void d3d9_swapchain_impl::set_transform(std::span<const float, 16> m) { std::copy(m.begin(), m.end(), transform_); }
void d3d9_swapchain_impl::get_transform(std::span<float, 16> m) const { std::copy(std::begin(transform_), std::end(transform_), m.begin()); }
void d3d9_swapchain_impl::set_projection(std::span<const float, 16> m) { std::copy(m.begin(), m.end(), projection_); }
void d3d9_swapchain_impl::get_projection(std::span<float, 16> m) const { std::copy(std::begin(projection_), std::end(projection_), m.begin()); }

void d3d9_swapchain_impl::clear(color c) {
    set_render_target_to_back_buffer();
    setup_render_states();
    device->Clear(0, nullptr, D3DCLEAR_TARGET, to_d3d_color(c), 1.0f, 0);
    device->BeginScene();
}

void d3d9_swapchain_impl::present() {
    device->EndScene();
    swap_chain->Present(nullptr, nullptr, nullptr, nullptr, 0);
}

void d3d9_swapchain_impl::on_resize(vec2i new_size) {
    size = new_size;
    if (swap_chain) { swap_chain->Release(); swap_chain = nullptr; }

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed             = TRUE;
    pp.SwapEffect           = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat     = D3DFMT_UNKNOWN;
    pp.BackBufferWidth      = (UINT)new_size.x;
    pp.BackBufferHeight     = (UINT)new_size.y;
    pp.hDeviceWindow        = hwnd;
    device->CreateAdditionalSwapChain(&pp, &swap_chain);
}

// ── Factory functions ─────────────────────────────────────────────────

static std::unique_ptr<gfx_device_impl> create_d3d9_device() {
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) return nullptr;

    HWND dummy = create_dummy_hwnd();
    if (!dummy) { d3d->Release(); return nullptr; }

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed         = TRUE;
    pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferWidth  = 1;
    pp.BackBufferHeight = 1;
    pp.hDeviceWindow    = dummy;

    IDirect3DDevice9* device = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, dummy,
                                    D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                    &pp, &device);
    if (FAILED(hr)) {
        hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, dummy,
                               D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                               &pp, &device);
    }
    if (FAILED(hr)) {
        DestroyWindow(dummy);
        d3d->Release();
        return nullptr;
    }

    auto impl    = std::make_unique<d3d9_device_impl>();
    impl->d3d    = d3d;
    impl->device = device;
    impl->dummy  = dummy;
    return impl;
}

static std::unique_ptr<swapchain_impl> create_d3d9_swapchain(
    gfx_device_impl& dev, void* native_handle, vec2i initial_size)
{
    auto& d3d_dev = static_cast<d3d9_device_impl&>(dev);
    HWND hwnd = static_cast<HWND>(native_handle);

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed         = TRUE;
    pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferWidth  = (UINT)initial_size.x;
    pp.BackBufferHeight = (UINT)initial_size.y;
    pp.hDeviceWindow    = hwnd;

    IDirect3DSwapChain9* sc = nullptr;
    HRESULT hr = d3d_dev.device->CreateAdditionalSwapChain(&pp, &sc);
    if (FAILED(hr)) return nullptr;

    auto impl         = std::make_unique<d3d9_swapchain_impl>();
    impl->device      = d3d_dev.device;
    impl->swap_chain  = sc;
    impl->hwnd        = hwnd;
    impl->size        = initial_size;
    return impl;
}

// ── Registration ──────────────────────────────────────────────────────

void register_d3d9_backend() {
    register_gfx_backend({
        "d3d9",
        gfx_backend::d3d9,
        create_d3d9_device,
        create_d3d9_swapchain,
    });
}

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
