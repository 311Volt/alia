#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"

namespace alia {

    swapchain_handle *d3d9_create_swapchain(device_handle *dev_h, void *native_handle, vec2i size) {
        auto *dev = as_d3d9_device(dev_h);
        HWND hwnd = static_cast<HWND>(native_handle);

        D3DPRESENT_PARAMETERS pp = {};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.BackBufferFormat = D3DFMT_UNKNOWN;
        pp.BackBufferWidth = static_cast<UINT>(size.x);
        pp.BackBufferHeight = static_cast<UINT>(size.y);
        pp.hDeviceWindow = hwnd;

        IDirect3DSwapChain9 *sc = nullptr;
        if (FAILED(dev->device->CreateAdditionalSwapChain(&pp, &sc)))
            return nullptr;

        auto *s = new d3d9_swapchain;
        s->device = dev->device;
        s->swap_chain = sc;
        s->hwnd = hwnd;
        s->size = size;
        return s;
    }

    void d3d9_destroy_swapchain(swapchain_handle *h) {
        auto *sc = as_d3d9_swapchain(h);
        if (sc->swap_chain)
            sc->swap_chain->Release();
        delete sc;
    }

    void d3d9_swapchain_begin_frame(swapchain_handle *h) {
        auto *sc = as_d3d9_swapchain(h);

        IDirect3DSurface9 *bb = nullptr;
        sc->swap_chain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &bb);
        sc->device->SetRenderTarget(0, bb);
        bb->Release();

        sc->device->BeginScene();
    }

    void d3d9_swapchain_end_frame(swapchain_handle *h) {
        auto *sc = as_d3d9_swapchain(h);
        sc->device->EndScene();
    }

    void d3d9_swapchain_present(swapchain_handle *h) {
        auto *sc = as_d3d9_swapchain(h);
        sc->swap_chain->Present(nullptr, nullptr, nullptr, nullptr, 0);
    }

    void d3d9_swapchain_on_resize(swapchain_handle *h, vec2i new_size) {
        auto *sc = as_d3d9_swapchain(h);
        sc->size = new_size;
        if (sc->swap_chain) {
            sc->swap_chain->Release();
            sc->swap_chain = nullptr;
        }

        D3DPRESENT_PARAMETERS pp = {};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.BackBufferFormat = D3DFMT_UNKNOWN;
        pp.BackBufferWidth = static_cast<UINT>(new_size.x);
        pp.BackBufferHeight = static_cast<UINT>(new_size.y);
        pp.hDeviceWindow = sc->hwnd;
        sc->device->CreateAdditionalSwapChain(&pp, &sc->swap_chain);
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
