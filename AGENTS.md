# Graphics subsystem architecture

API-level behaviour is documented next to the declarations in `src/alia/gfx/`. This file records only the rules that span the subsystem.

## Layers

| Layer | Location | Role |
|-------|----------|------|
| L1 | `*.hpp` (public) | User-facing API: device, swapchain, frame render state, textures, buffers, shaders, and draw helpers |
| L2 | `*.hpp` (detail/templates) | Type-erasure shims from typed templates (texture locks, `frame::draw<TVertex>` and its vertex layout) to L3 |
| L3 | `*.cpp` (backend-agnostic) | Object lifetime and validation, format dispatch, vertex-definition registry; calls into L4 |
| L4 | `graphics_backend_interface` | One function-pointer table per device, one `gfx_backend_op` slot per backend operation |

## Backends

- Each backend lives in `backend_<name>/`. `<name>_ops.hpp` holds its concrete structs, `as_<name>_*` cast helpers and op declarations; `register_<name>_backend.cpp` creates the device, probes capabilities and fills the interface.
- Backend objects inherit the opaque `*_handle` bases and are downcast with `static_cast` only inside that backend. Concrete backend types never appear in public headers.
- Backends never substitute pixel formats. `create_*texture` stores the format it is given; fallback lives in L3 (`detail::closest_texture_format`).
- A null op slot means the **hardware** lacks the feature, as probed at device creation, with `reason_unsupported` saying why. Never use it for "this backend doesn't implement X yet".
- Never name anything `interface`; it is a macro in MinGW's COM headers.

## Ownership

Everything created from a `gfx_device` holds raw pointers into it, so the device must outlive all of them. A shader selected on a frame must remain valid while selected or saved in a state scope.

## Draw helpers

Draw helpers (`draw_texture`, text drawing, primitive renderers) follow these rules:

- They never change frame render state or the viewport; they draw with what is current.
- Their params are aggregates. A texture slot, where needed, is a `detail::required_texture_slot` member, so every callsite must name it. Fixed-function drawing uses slot 0.
- They bind their texture immediately before drawing and leave the binding in place afterwards.
- Invalid input throws `std::invalid_argument` before anything is bound. Empty input returns without binding or drawing.
- They submit immediately, except batching types, which say so on the type.

## Adding a new backend operation

1. Add a `gfx_backend_op<R(Args...)>` slot to `graphics_backend_interface` in `graphics_backend_interface.hpp`.
2. Implement the function in each backend's appropriate `.cpp` file and declare it in `*_ops.hpp`.
3. Wire the slot in `register_*_backend.cpp`: set the `operation` pointer, or if the op is hardware-conditional, leave it null and set `reason_unsupported`.
4. Add the L3 free function or method that calls `.get_or_throw()`.

## The `slop` folder

The folder contains AI-generated design documents. They may, or may not be in any way relevant to the current state of the repo. They may, or may not be in any way aligned with the user's intent. Only intentionally read documents from that folder if explicitly asked to / pointed at by the user.
