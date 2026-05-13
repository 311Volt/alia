#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"
#include "../gfx_device.hpp"
#include <cstring>

namespace alia {

    static D3DMATRIX make_identity_matrix() {
        D3DMATRIX m;
        std::memset(&m, 0, sizeof(m));
        m._11 = m._22 = m._33 = m._44 = 1.0f;
        return m;
    }

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

    void d3d9_swapchain_clear(swapchain_handle *h, color c) {
        auto *sc = as_d3d9_swapchain(h);

        IDirect3DSurface9 *bb = nullptr;
        sc->swap_chain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &bb);
        sc->device->SetRenderTarget(0, bb);
        bb->Release();

        float transform[16], projection[16];
        get_current_transform_matrix(std::span<float, 16>(transform, 16));
        get_current_projection_matrix(std::span<float, 16>(projection, 16));
        sc->device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX *>(transform));
        D3DMATRIX view = make_identity_matrix();
        sc->device->SetTransform(D3DTS_VIEW, &view);
        sc->device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX *>(projection));

        sc->device->SetRenderState(D3DRS_LIGHTING, FALSE);
        sc->device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        sc->device->SetRenderState(D3DRS_ZENABLE, FALSE);
        sc->device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

        D3DVIEWPORT9 vp = {0, 0, static_cast<DWORD>(sc->size.x), static_cast<DWORD>(sc->size.y), 0.0f, 1.0f};
        sc->device->SetViewport(&vp);

        sc->device->Clear(0, nullptr, D3DCLEAR_TARGET, to_d3d_color(c), 1.0f, 0);
        sc->device->BeginScene();
    }

    void d3d9_swapchain_present(swapchain_handle *h) {
        auto *sc = as_d3d9_swapchain(h);
        sc->device->EndScene();
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
