# Graphics subsystem architecture

The graphics subsystem in `src/alia/gfx/` follows a four-layer split.

## Layers

| Layer | Location | Role |
|-------|----------|------|
| L1 | `*.hpp` (public) | User-facing API: `texture`, `cube_texture`, `gfx_device`, `swapchain`, `framebuffer_config`, `pipeline`, `dynamic_pipeline`, `frame`, and the primitive renderer family |
| L2 | `*.hpp` (detail/templates) | Type-erasure shims: `texture::lock<TPixel>` / `cube_texture::lock<TPixel>` → `lock_impl`, and `frame::draw<TVertex>` → L3 submission helpers |
| L3 | `*.cpp` (backend-agnostic) | Device/pipeline/frame lifetime and validation, texture format dispatch, and vertex-definition registry; calls into L4 |
| L4 | `graphics_backend_interface` | Single function-pointer table per device; one slot per backend operation |

## Key types

**Opaque handles** — `device_handle`, `texture_handle`, `swapchain_handle`, `pipeline_handle`, and buffer/program handles are empty base structs defined in `graphics_backend_interface.hpp`. Each backend inherits its concrete struct from the appropriate base (`ogl_texture : texture_handle`, `d3d9_texture : texture_handle`, etc.) and downcasts with `static_cast` inside its own `.cpp` files. These types never appear in public headers as concrete objects.

Both 2D and cube textures use `texture_handle`. Concrete texture records carry their kind, and shared texture operations dispatch on it. `texture_lock_info::face` identifies the locked cube face (zero for 2D); `render_target_info::target_face` identifies the selected render-target face.

**`graphics_backend_operation<R(Args...)>`** — wraps a raw function pointer with an optional `reason_unsupported` string. Call `.get_or_throw()` to retrieve the pointer (throws `unsupported_operation_exception` if null). Call `.is_supported()` to test without throwing. A null operation slot means the *hardware* does not support the feature (probed at device-creation time), not that the backend library lacks it.

**`graphics_backend_interface`** — one instance per `gfx_device`, heap-allocated inside it (`unique_ptr`) for pointer stability. Contains every backend operation as a `graphics_backend_operation` slot, the probed `gfx_device_caps`, and swapchain property/partial-present operations. Device-owned objects hold a raw `const graphics_backend_interface*` alias into their owning device's copy; the device must outlive them.

**Pipelines and effects** — `pipeline` is immutable and contains its vertex layout, effect/program selection, blend/depth/raster state. `dynamic_pipeline` has the same backend object but may change those fields; it is rebound before every draw, so mutations take effect at draw granularity. `pipeline_config::effect` is non-owning on both alternatives: `const basic_effect*` for fixed function and `shader_program*` for programmable drawing. The referenced object must outlive the pipeline. `basic_effect` owns mutable fixed-function world/projection matrices; shader programs retain their own constants and sampler records.

**Recorded geometry sources** — L4 `bind_vertex_buffer`, `bind_index_buffer`, and transient upload operations only record the current source. Actual API binding and layout application occur in `draw`/`draw_indexed`. Transient pointers remain valid through the next bind/upload of the same kind or `swapchain_end_frame`; L3 enforces this lifetime. Every backend clears this shadow state at frame end.

**`created_device`** — returned by each backend's factory function after the device is created and hardware capabilities are probed:
```cpp
struct created_device {
    device_handle *handle = nullptr;
    graphics_backend_interface iface;   // NOTE: field is "iface", not "interface"
                                        // ("interface" is a macro in MinGW COM headers)
};
```

**`gfx_backend_factory`** — registered at startup via `register_gfx_backend()`; holds a `created_device (*create)(const gfx_device_config&)` that selects an adapter/render method, creates the device, probes capabilities, and builds the full interface in one call.

## Backend directories

```
src/alia/gfx/
  graphics_backend_interface.hpp   — L4 types (opaque handles, operation slots, full interface struct)
  framebuffer_config.hpp / .cpp    — display option requests, actual properties, and required-option validation
  gfx_device.hpp / gfx_device.cpp  — L1/L3 for device, swapchain, and backend registry
  pipeline.hpp / pipeline.cpp      — L1/L3 immutable and dynamic pipeline objects
  frame.hpp / frame.cpp            — L1/L2/L3 frame lifetime, target/clear commands, and draw validation
  primitive_renderer.hpp          — header-only colored-primitive tessellation (batching + immediate)
  texture.hpp / texture.cpp        — L1/L2/L3 for texture (lock template, upload, download, clone)
  cube_texture.hpp / cube_texture.cpp — L1/L2/L3 for cube textures (face locks, upload, download, clone)
  text/                            — FreeType loading, gray8 glyph atlas cache, software rasterization (`create_text_bitmap` / `create_text_texture`), and free-function `draw_text`
  backend_d3d9/                    — L4 D3D9 implementation
    d3d9_ops.hpp                   — concrete structs + cast helpers + all op declarations
    register_d3d9_backend.cpp      — probes mipmap and cube-map caps, builds + registers interface
    device.cpp / swapchain.cpp / texture.cpp / pipeline.cpp
  backend_ogl/                     — L4 OpenGL implementation
    ogl_ops.hpp                    — concrete structs + cast helpers + all op declarations
    ogl_platform.hpp               — ogl_platform_ops (Win32 WGL hooks; OGL-internal, not cross-backend)
    register_ogl_backend.cpp       — probes GL/cube-map versions + wglGetProcAddress("glGenerateMipmap"),
                                     builds + registers interface
    device.cpp / swapchain.cpp / texture.cpp / pipeline.cpp
    win32_platform.cpp             — Win32 WGL surface/context management
  ../os/display.cpp                — platform-independent closest-video-mode selection
  ../os/monitor_win32.cpp          — Win32 monitor and video-mode enumeration
```

Each OpenGL swapchain owns an `HGLRC` whose objects are shared with the device's root dummy-window context. Render-to-texture FBOs are per context because FBOs are not shared. A window's first swapchain fixes its pixel format; later swapchains for that same window reuse it.

## Hardware capability probing

`reason_unsupported` is set when the **hardware** cannot perform an operation, detected at device-creation time:

- **OGL**: `glGetString(GL_VERSION)` parsed for major/minor; `wglGetProcAddress("glGenerateMipmap")` attempted. If it returns null, `texture_generate_mipmaps.operation` is left null with `reason_unsupported` set.
- **D3D9**: `GetDeviceCaps` checked for `D3DCAPS2_CANAUTOGENMIPMAP`. If absent, `texture_generate_mipmaps` is marked unsupported.
- **Cube textures**: OpenGL requires 1.3+ or `GL_ARB_texture_cube_map`; D3D9 requires `D3DPTEXTURECAPS_CUBEMAP`. If absent, `create_cube_texture` is marked unsupported.

Do **not** use `reason_unsupported` for "this backend doesn't implement X yet" — only for genuine hardware-level gaps.

## Frames and primitive renderers

`swapchain::begin_frame()` creates a move-only `frame`; a frame ends with `frame::present()`, `frame::present(region)`, or, without presenting, its destructor. It initially targets the swapchain backbuffer without clearing. `set_target()` selects the backbuffer (with depth only when the negotiated framebuffer has it); `set_target(texture, level)` selects a render-target texture mip (without depth); `set_target(cube_texture, face, level)` selects one cube face (also without depth); `clear()` is an independent command. A pipeline must be selected before the first draw.

Depth clears and depth-enabled pipelines are valid only while the backbuffer is selected. Dynamic pipeline depth changes are checked again just before draw. Resource bindings persist for the frame across pipeline switches; when selecting a texture target, frame-managed bindings of that same texture are defensively removed. Shader-owned samplers must still avoid sampling the current render target.

Primitive renderers own no device, effect, pipeline, or transform state. The caller binds a colored-vertex pipeline and owns its `basic_effect` world/projection matrices; re-derive the projection after every `set_target()` with `device.ortho_ui()` for UI or `device.perspective_fov()` for 3D. Tessellators append absolute indices to any `primitive_sink`. Because the draw API uses deducing-this, call it on the concrete renderer type: a `generic_primitive_renderer&` is not a sink, while treating an immediate renderer as `primitive_renderer&` silently loses auto-flush. Polyline and rectangle outlines support miter and bevel joins with non-overlapping adjacent geometry for well-formed input. Batched `primitive_renderer` geometry is submitted only by `flush(frame)`; forgetting to flush silently drops it.

Text drawing requires the caller to bind a `full_vertex` pipeline whose `basic_effect` uses `texture_operation::alpha_mask` and owns the projection. `draw_text` rebinds texture slot 0, whose binding persists for the frame, so callers relying on that slot must rebind it afterwards. `create_text_bitmap` rasterizes on the CPU into a gray8 `text_bitmap` carrying the offset of its top-left pixel from the layout origin; `create_text_texture` uploads it as an alpha-mask texture. Nothing caches: a caller whose string changes re-creates the texture. Drawing submits immediately: one indexed draw per atlas page touched for `hardware_glyph_buffer`, or one quad for a `text_texture`. Both `hardware_glyph_buffer` and `text_texture` must not outlive their device.

## Adding a new backend operation

1. Add a `graphics_backend_operation<R(Args...)>` slot to `graphics_backend_interface` in `graphics_backend_interface.hpp`. Cube creation and face locking use `create_cube_texture` and `cube_texture_lock`; the swapchain readback operations are `swapchain_properties` and `swapchain_present_region`.
2. Implement the function in each backend's appropriate `.cpp` file and declare it in `*_ops.hpp`.
3. Wire the slot in `register_*_backend.cpp` (set `operation` pointer, or leave null + set `reason_unsupported` if hardware-conditional).
4. Add the L3 free function or method in `gfx_device.cpp` / `texture.cpp` that calls `.get_or_throw()`.

## The `slop` folder

They contain AI-generated design documents. They may, or may not be in any way relevant to the current state of the repo. They may, or may not be in any way aligned with the user's intent. Only intentionally read documents from that folder if explicitly asked to / pointed at by the user.
