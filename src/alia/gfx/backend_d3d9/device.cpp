#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"

namespace alia {

    static HWND create_dummy_hwnd() {
        static const wchar_t *cls = L"AliaDummy_D3D9";
        static bool registered = false;
        if (!registered) {
            WNDCLASSEXW wc = {};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = cls;
            RegisterClassExW(&wc);
            registered = true;
        }
        return CreateWindowExW(
            0, cls, L"", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr
        );
    }

    d3d9_device *d3d9_create_device() {
        IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d)
            return nullptr;

        HWND dummy = create_dummy_hwnd();
        if (!dummy) {
            d3d->Release();
            return nullptr;
        }

        D3DPRESENT_PARAMETERS pp = {};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.BackBufferFormat = D3DFMT_UNKNOWN;
        pp.BackBufferWidth = 1;
        pp.BackBufferHeight = 1;
        pp.hDeviceWindow = dummy;

        IDirect3DDevice9 *device = nullptr;
        HRESULT hr = d3d->CreateDevice(
            D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, dummy,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device
        );
        if (FAILED(hr)) {
            hr = d3d->CreateDevice(
                D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, dummy,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device
            );
        }
        if (FAILED(hr)) {
            DestroyWindow(dummy);
            d3d->Release();
            return nullptr;
        }

        auto *dev = new d3d9_device;
        dev->d3d = d3d;
        dev->device = device;
        dev->dummy = dummy;
        device->GetDeviceCaps(&dev->caps);
        return dev;
    }

    void d3d9_destroy_device(device_handle *h) {
        auto *dev = as_d3d9_device(h);
        dev->vertex_definitions.clear();
        if (dev->device) {
            dev->device->Release();
            dev->device = nullptr;
        }
        if (dev->d3d) {
            dev->d3d->Release();
            dev->d3d = nullptr;
        }
        if (dev->dummy) {
            DestroyWindow(dev->dummy);
            dev->dummy = nullptr;
        }
        delete dev;
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
