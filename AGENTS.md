# Graphics subsystem architecture

The graphics subsystem in `src/alia/gfx/` follows a four-layer split.

## Layers

| Layer | Location | Role |
|-------|----------|------|
| L1 | `*.hpp` (public) | User-facing API: `texture`, `swapchain`, `gfx_device`, free functions like `draw_prim`, `fill_rect` |
| L2 | `*.hpp` (detail/templates) | Type-erasure shims: `texture::lock<TPixel>` → `lock_impl`, vertex-type erasure in `primitives.hpp` |
| L3 | `*.cpp` (backend-agnostic) | Validation, format dispatch, current-device lookup; calls into L4 |
| L4 | `graphics_backend_interface` | Single function-pointer table per device; one slot per backend operation |

## Key types

**Opaque handles** — `device_handle`, `texture_handle`, `swapchain_handle` are empty base structs defined in `graphics_backend_interface.hpp`. Each backend inherits its concrete struct from the appropriate base (`ogl_texture : texture_handle`, `d3d9_texture : texture_handle`, etc.) and downcasts with `static_cast` inside its own `.cpp` files. These types never appear in public headers as concrete objects.

**`graphics_backend_operation<R(Args...)>`** — wraps a raw function pointer with an optional `reason_unsupported` string. Call `.get_or_throw()` to retrieve the pointer (throws `unsupported_operation_exception` if null). Call `.is_supported()` to test without throwing. A null operation slot means the *hardware* does not support the feature (probed at device-creation time), not that the backend library lacks it.

**`graphics_backend_interface`** — one instance per `gfx_device`, heap-allocated inside it (`unique_ptr`) for pointer stability. Contains every backend operation as a `graphics_backend_operation` slot. `texture` and `swapchain` objects each hold a raw `const graphics_backend_interface*` that aliases into their owning device's copy; the device must outlive them.

**`created_device`** — returned by each backend's factory function after the device is created and hardware capabilities are probed:
```cpp
struct created_device {
    device_handle *handle = nullptr;
    graphics_backend_interface iface;   // NOTE: field is "iface", not "interface"
                                        // ("interface" is a macro in MinGW COM headers)
};
```

**`gfx_backend_factory`** — registered at startup via `register_gfx_backend()`; holds a `created_device (*create)()` that creates the device and builds the full interface in one call.

## Backend directories

```
src/alia/gfx/
  graphics_backend_interface.hpp   — L4 types (opaque handles, operation slots, full interface struct)
  gfx_device.hpp / gfx_device.cpp  — L1/L3 for device, swapchain, draw free functions, backend registry
  texture.hpp / texture.cpp        — L1/L2/L3 for texture (lock template, upload, download, clone)
  primitives.hpp / primitives.cpp  — L1/L2/L3 for geometry helpers (fill_rect, draw_line, etc.)
  backend_d3d9/                    — L4 D3D9 implementation
    d3d9_ops.hpp                   — concrete structs + cast helpers + all op declarations
    register_d3d9_backend.cpp      — probes D3DCAPS2_CANAUTOGENMIPMAP, builds + registers interface
    device.cpp / swapchain.cpp / texture.cpp / primitives.cpp
  backend_ogl/                     — L4 OpenGL implementation
    ogl_ops.hpp                    — concrete structs + cast helpers + all op declarations
    ogl_platform.hpp               — ogl_platform_ops (Win32 WGL hooks; OGL-internal, not cross-backend)
    register_ogl_backend.cpp       — probes GL version + wglGetProcAddress("glGenerateMipmap"),
                                     builds + registers interface
    device.cpp / swapchain.cpp / texture.cpp / primitives.cpp
    win32_platform.cpp             — Win32 WGL surface/context management
```

## Hardware capability probing

`reason_unsupported` is set when the **hardware** cannot perform an operation, detected at device-creation time:

- **OGL**: `glGetString(GL_VERSION)` parsed for major/minor; `wglGetProcAddress("glGenerateMipmap")` attempted. If it returns null, `texture_generate_mipmaps.operation` is left null with `reason_unsupported` set.
- **D3D9**: `GetDeviceCaps` checked for `D3DCAPS2_CANAUTOGENMIPMAP`. If absent, `texture_generate_mipmaps` is marked unsupported.

Do **not** use `reason_unsupported` for "this backend doesn't implement X yet" — only for genuine hardware-level gaps.

## Adding a new backend operation

1. Add a `graphics_backend_operation<R(Args...)>` slot to `graphics_backend_interface` in `graphics_backend_interface.hpp`.
2. Implement the function in each backend's appropriate `.cpp` file and declare it in `*_ops.hpp`.
3. Wire the slot in `register_*_backend.cpp` (set `operation` pointer, or leave null + set `reason_unsupported` if hardware-conditional).
4. Add the L3 free function or method in `gfx_device.cpp` / `texture.cpp` that calls `.get_or_throw()`.
