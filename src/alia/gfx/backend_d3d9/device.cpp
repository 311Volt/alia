#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"
#include "../../os/monitor_win32.hpp"

#include <algorithm>

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

    d3d9_device *d3d9_create_device(const gfx_device_config &config) {
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

        UINT adapter = D3DADAPTER_DEFAULT;
        if (config.adapter >= 0) {
            void *wanted = nullptr;
            try { wanted = detail::win32_monitor_handle(config.adapter); } catch (...) {}
            for (UINT i = 0; wanted && i < d3d->GetAdapterCount(); ++i) {
                if (d3d->GetAdapterMonitor(i) == static_cast<HMONITOR>(wanted)) {
                    adapter = i;
                    break;
                }
            }
        }

        auto try_create = [&](D3DDEVTYPE type, IDirect3DDevice9 **out) {
            const DWORD first_flags = type == D3DDEVTYPE_HAL
                ? D3DCREATE_HARDWARE_VERTEXPROCESSING
                : D3DCREATE_SOFTWARE_VERTEXPROCESSING;
            HRESULT hr = d3d->CreateDevice(adapter, type, dummy, first_flags, &pp, out);
            if (FAILED(hr) && type == D3DDEVTYPE_HAL)
                hr = d3d->CreateDevice(
                    adapter, type, dummy, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, out);
            return hr;
        };

        const bool want_software = config.render.requested() &&
            config.render.value == render_method::software;
        D3DDEVTYPE device_type = want_software ? D3DDEVTYPE_REF : D3DDEVTYPE_HAL;
        IDirect3DDevice9 *device = nullptr;
        HRESULT hr = try_create(device_type, &device);
        if (FAILED(hr) && !config.render.required()) {
            device_type = want_software ? D3DDEVTYPE_HAL : D3DDEVTYPE_REF;
            hr = try_create(device_type, &device);
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
        dev->adapter = adapter;
        dev->device_type = device_type;
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
