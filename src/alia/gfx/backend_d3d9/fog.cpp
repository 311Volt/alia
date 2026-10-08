#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"

#include <bit>

namespace alia {
    namespace {
        D3DFOGMODE native_fog_mode(fog_mode mode) {
            switch (mode) {
            case fog_mode::none: return D3DFOG_NONE;
            case fog_mode::linear: return D3DFOG_LINEAR;
            case fog_mode::exp: return D3DFOG_EXP;
            case fog_mode::exp2: return D3DFOG_EXP2;
            }
            return D3DFOG_NONE;
        }
    }

    void d3d9_set_fog(device_handle *h, const fog_state &fog) {
        auto &device = *as_d3d9_device(h);
        // Vertex fog uses eye-space distance, independently of the projection.
        device.device->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
        device.device->SetRenderState(D3DRS_FOGVERTEXMODE, native_fog_mode(fog.mode));
        device.device->SetRenderState(D3DRS_FOGCOLOR, to_d3d_color(fog.col));
        device.device->SetRenderState(D3DRS_FOGSTART, std::bit_cast<DWORD>(fog.start));
        device.device->SetRenderState(D3DRS_FOGEND, std::bit_cast<DWORD>(fog.end));
        device.device->SetRenderState(D3DRS_FOGDENSITY, std::bit_cast<DWORD>(fog.density));
        device.device->SetRenderState(D3DRS_RANGEFOGENABLE,
            (device.caps.RasterCaps & D3DPRASTERCAPS_FOGRANGE) != 0 ? TRUE : FALSE);
    }
} // namespace alia

#endif
