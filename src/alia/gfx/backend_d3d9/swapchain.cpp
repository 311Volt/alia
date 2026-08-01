#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"

namespace alia {
    namespace {
        bool create_depth_stencil(d3d9_swapchain &swapchain) {
            if (SUCCEEDED(swapchain.device->CreateDepthStencilSurface(
                    static_cast<UINT>(swapchain.size.x), static_cast<UINT>(swapchain.size.y),
                    D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &swapchain.depth_stencil, nullptr)))
                return true;
            return SUCCEEDED(swapchain.device->CreateDepthStencilSurface(
                static_cast<UINT>(swapchain.size.x), static_cast<UINT>(swapchain.size.y),
                D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, TRUE, &swapchain.depth_stencil, nullptr));
        }
        bool create_native_swapchain(d3d9_swapchain &swapchain) {
            D3DPRESENT_PARAMETERS pp = {};
            pp.Windowed = TRUE;
            pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            pp.BackBufferFormat = D3DFMT_UNKNOWN;
            pp.BackBufferWidth = static_cast<UINT>(swapchain.size.x);
            pp.BackBufferHeight = static_cast<UINT>(swapchain.size.y);
            pp.hDeviceWindow = swapchain.hwnd;
            return SUCCEEDED(swapchain.device->CreateAdditionalSwapChain(&pp, &swapchain.swap_chain));
        }
    }

    swapchain_handle *d3d9_create_swapchain(device_handle *dev_h, void *native_handle, vec2i size) {
        auto *dev = as_d3d9_device(dev_h);
        auto *swapchain = new d3d9_swapchain;
        swapchain->device = dev->device;
        swapchain->hwnd = static_cast<HWND>(native_handle);
        swapchain->size = size;
        if (!create_native_swapchain(*swapchain) || !create_depth_stencil(*swapchain)) {
            if (swapchain->depth_stencil) swapchain->depth_stencil->Release();
            if (swapchain->swap_chain) swapchain->swap_chain->Release();
            delete swapchain;
            return nullptr;
        }
        return swapchain;
    }
    void d3d9_destroy_swapchain(swapchain_handle *h) {
        auto *swapchain = as_d3d9_swapchain(h);
        if (swapchain->depth_stencil) swapchain->depth_stencil->Release();
        if (swapchain->swap_chain) swapchain->swap_chain->Release();
        delete swapchain;
    }
    void d3d9_swapchain_begin_frame(swapchain_handle *h) { as_d3d9_swapchain(h)->device->BeginScene(); }
    void d3d9_swapchain_end_frame(swapchain_handle *h) { as_d3d9_swapchain(h)->device->EndScene(); }
    void d3d9_swapchain_present(swapchain_handle *h) { as_d3d9_swapchain(h)->swap_chain->Present(nullptr, nullptr, nullptr, nullptr, 0); }
    void d3d9_swapchain_on_resize(swapchain_handle *h, vec2i new_size) {
        auto *swapchain = as_d3d9_swapchain(h);
        swapchain->device->SetDepthStencilSurface(nullptr);
        if (swapchain->depth_stencil) { swapchain->depth_stencil->Release(); swapchain->depth_stencil = nullptr; }
        if (swapchain->swap_chain) { swapchain->swap_chain->Release(); swapchain->swap_chain = nullptr; }
        swapchain->size = new_size;
        if (!create_native_swapchain(*swapchain) || !create_depth_stencil(*swapchain))
            throw std::runtime_error("D3D9 failed to resize swapchain depth buffer");
    }
} // namespace alia

#endif
