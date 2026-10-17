# Architecture

## Scope of this file

This file holds high-level design only: layering, ownership, threading and conventions that apply across a whole subsystem or the library. When editing it:

- Document the behaviour of one type, function, backend or algorithm in a comment next to its declaration or definition, not here. This includes limits, constants, formulas and platform quirks.
- Prefer extending or generalizing an existing rule over adding a new one. If a rule now holds for every subsystem, move it to "All subsystems".
- Keep each rule to one or two sentences. If a rule needs more, the detail belongs in the code.

## All subsystems

### Stability

The API, public or internal, is still completely unstable. Change or remove anything outright; never keep functions, aliases or shims for compatibility.

### Layers

Every subsystem is split into the same four layers:

| Layer | Location | Role |
|-------|----------|------|
| L1 | `*.hpp` (public) | User-facing API |
| L2 | `*.hpp` (detail/templates) | Type-erasure shims from typed templates to L3 |
| L3 | `*.cpp` (backend-agnostic) | Object lifetime and validation, selection and fallback, registries; calls into L4 |
| L4 | `*_backend_interface` | One function-pointer table per root object, one op slot per backend operation |

Each subsystem has one root object, which owns its L4 table:

| Subsystem | Root object | L4 table | Registration file |
|-----------|-------------|----------|-------------------|
| `gfx/` | `gfx_device` | `graphics_backend_interface` | `register_<name>_backend.cpp` |
| `audio/` | `audio_context` | `audio_backend_interface` | `register_<name>_audio_backend.cpp` |
| `os/` | `os_context` | `os_backend_interface` | `register_<name>_os_backend.cpp` |

### Backends

- Each backend lives in `<subsystem>/backend_<name>/`. `<name>_ops.hpp` holds its concrete structs, `as_<name>_*` cast helpers and op declarations; the registration file creates the root object, probes capabilities and fills the L4 table.
- Backend objects inherit the opaque `*_handle` bases and are downcast with `static_cast` only inside that backend. Concrete backend types and third-party library types never appear in public headers.
- Op slots are declared directly with the types from `core/backend_op.hpp`. A `backend_op` is one every backend always provides regardless of hardware or version, and L3 calls it directly; any other op is an `optional_backend_op`, which L3 calls through `.get_or_throw()`.
- A null `optional_backend_op` slot means the hardware or platform lacks the feature, as probed at root creation, or the backend does not implement it yet; `reason_unsupported` says which.
- Backends never substitute formats or modes. They deliver exactly what was requested or fail; fallback lives in L3 (`detail::closest_texture_format`, `detail::audio_format_fallbacks`, `choose_closest_mode`).
- Never name anything `interface`; it is a macro in MinGW's COM headers.

### Ownership

Everything created from a root object holds raw pointers into it, so the root must outlive all of them. Moving a root keeps its heap-allocated L4 table and native handles in place, so its children stay valid.

### Adding a new backend operation

1. Add an op slot to the subsystem's L4 table: `backend_op<R(Args...)>` if every backend can always provide it, otherwise `optional_backend_op<R(Args...)>`.
2. Implement the function in each backend's appropriate `.cpp` file and declare it in `<name>_ops.hpp`.
3. Wire the slot in the registration file: set the `operation` pointer, or for an `optional_backend_op` the backend cannot provide, leave it null and set `reason_unsupported`.
4. Add the L3 free function or method that calls the slot, through `.get_or_throw()` if it is an `optional_backend_op`.

## Graphics

### Layers

| Layer | Contents |
|-------|----------|
| L1 | Device, swapchain, frame render state, textures, buffers, shaders, and draw helpers |
| L2 | Texture locks, `frame::draw<TVertex>` and its vertex layout |
| L3 | Format dispatch, vertex-definition registry |

### Ownership

A shader selected on a frame must remain valid while selected or saved in a state scope.

### Draw helpers

Draw helpers (`draw_texture`, text drawing, primitive renderers) follow these rules:

- They never change frame render state or the viewport; they draw with what is current.
- Their params are aggregates. A texture slot, where needed, is a `detail::required_texture_slot` member, so every callsite must name it. Fixed-function drawing uses slot 0.
- They bind their texture immediately before drawing and leave the binding in place afterwards.
- Invalid input throws `std::invalid_argument` before anything is bound. Empty input returns without binding or drawing.
- They submit immediately, except batching types, which say so on the type.

## Audio

### Layers

| Layer | Contents |
|-------|----------|
| L1 | Sample formats, clips, ring buffer, effects, codecs, context, streams, player and mixer |
| L2 | Typed-to-erased clip views and stream callback shims |
| L3 | Format negotiation and fallback, conversion trampolines, codec registry |

### Terms

- Counts are sample-frame counts (`frame_count`), never sample-point counts; see `sample.hpp`.
- Multichannel data uses SMPTE channel order everywhere. Codecs reorder into it when decoding.

### Realtime path

The realtime path is everything that runs inside a stream callback: user callbacks, `audio_effect::process`, and player and mixer rendering.

- Callbacks run on backend-owned threads with at most `max_callback_frames`. Everything they touch is provisioned on a control thread before they can run.
- The realtime path never allocates, takes blocking locks, waits, notifies or emits events. It shares state with control threads through lock-free atomics.
- Stop and destroy quiesce callbacks synchronously and must not be called from a callback. Objects that own a stream declare it as their last member, so it is quiesced before workers are joined or callback memory is freed.

### Rates

Streams convert point and channel formats in L3 but never resample. Rate conversion only happens through effects (`chebyshev2_lowpass::for_downsampling`, then `hermite_resampler`), as the mixer does.

## OS

### Layers

| Layer | Contents |
|-------|----------|
| L1 | Contexts, windows, displays, monitors, clipboard and dialogs |
| L2 | Typed bitmap views erase to `any_bitmap_view`; text-log formatting erases to text |
| L3 | Default-context dispatch and closest-mode selection; keyboard and mouse L3 lives in `io/*.cpp` |

### Threading

A context and everything created from it are created, used, polled and destroyed on one thread. Overloads without a context argument use the lazily created, thread-local `get_os_context()`.

## The `slop` folder

The folder contains AI-generated design documents. They may, or may not be in any way relevant to the current state of the repo. They may, or may not be in any way aligned with the user's intent. Only intentionally read documents from that folder if explicitly asked to / pointed at by the user.
