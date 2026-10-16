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

## Audio subsystem

API-level behaviour is documented in `src/alia/audio/`. Counts are sample-frame
counts (`frame_count`): a sample point is one channel value at one instant; a
sample frame contains all channel points at that instant (AIFF terminology).

| Layer | Location | Role |
|-------|----------|------|
| L1 | `audio/*.hpp` public | Sample concepts/formats, clips/views, ring buffer, effects, context and streams |
| L2 | `audio/*.hpp` templates | Typed-to-erased clip views and callback shims |
| L3 | `audio/*.cpp` | Registry, lifetime, negotiation, format fallback and conversion trampolines |
| L4 | `audio_backend_interface` | Per-context table of `audio_backend_op` slots |

- Backend files live in `audio/backend_<name>/`; concrete handles and casts live
  in `<name>_ops.hpp`, and registration in `register_<name>_audio_backend.cpp`.
  The operation-adding recipe above applies with these audio names.
- `backend_op` and `unsupported_operation_exception` live in `core/backend_op.hpp`;
  `gfx_backend_op` and `audio_backend_op` are aliases.
- Backends never substitute formats or rates. Create delivers exactly the
  requested callback format or returns null. Host conversion (e.g. WASAPI
  AUTOCONVERTPCM) is allowed; fallback belongs to L3. L3 does not resample when opening streams.
  Rate conversion is an explicit effect (`hermite_resampler`) for use by mixers.
  It does no band limiting; downsamplers chain `chebyshev2_lowpass` before it.
- A null slot means hardware/platform support is absent, with an explanatory
  `reason_unsupported`; it never denotes unfinished implementation.
- Callbacks run on backend-owned threads with at most `max_callback_frames`.
  Provision all buffers/converters at open. The callback path must not allocate,
  take blocking locks, or emit events. Best-effort `try_lock` is used only by the
  microphone example; the audio runtime itself takes no callback-path locks.
- Stop and destroy synchronously quiesce callbacks. Neither may be called from
  a callback. Join the worker before destroying callback objects or buffers.
- Device loss latches `failed`, ends callbacks, and never reroutes automatically.
  A throwing user callback also latches failure and silences subsequent output.
- Contexts must outlive their streams, which hold raw pointers into the context.
  Moves preserve the heap-allocated backend table and stream runtime addresses.
- `ring_buffer` has fixed capacity and no synchronization. Only construction and
  explicit `reserve` allocate. Use external synchronization between threads.
- Effects are float32-only extension points, not backends. They provision state
  at construction and never allocate or lock in `process`, following the callback
  path rules. They use push semantics with exact frame queries; empty input may
  drain buffered output, and empty output never advances state.
- WASAPI uses shared mode and a worker-owned COM apartment for each stream.
  Optional MMCSS functions are loaded dynamically; no avrt/mmdevapi imports are
  required just to start a program on systems without WASAPI.
- Audio file codecs use the extension registry in `audio_io.hpp` and the
  single-threaded `audio_decoder` pull contract. Decoders return their constant
  native format; typed reader reads require an exact match. Readers never resample.
- Vorbis readers reorder channels into the SMPTE order documented in `sample.hpp`.
  The dr_libs dependency is a private submodule; codec types stay out of public APIs.
- An `audio_player` either owns an output stream or attaches to an `audio_mixer`.
  Streamed players decode and convert on a polling worker into a private SPSC
  float ring. The callback never locks,
  allocates or notifies; seek/quit commands and acknowledgements stay on control
  and worker threads. Quiesce the stream or detach the voice before joining the worker.
- Standalone players open streams at the source rate and reject a different
  requested rate. Attached players are resampled by their mixer.
- Mixers are flat: each owns one stream, with player slots and a one-shot pool.
  Slot ownership uses lock-free free/suspended/enabled/rendering/suspend-requested
  states. Control suspends with CAS and spin/yield; the callback releases ownership
  with release stores/CAS, including on exceptions. It never waits or notifies.
- Mixer provisioning happens on the control thread while a slot is free or
  suspended. Only the rendering callback may mutate enabled voice processing.
  Downsampling chains `chebyshev2_lowpass::for_downsampling` before
  `hermite_resampler`, using separate buffers and preserving unconsumed input.
  At EOF feed exactly three lookahead frames after the real input, then drain;
  the output length is ceil(source_frames * mix_rate / source_rate).
- Mixer channel order follows SMPTE. Stereo downmix adds centre and matching
  rear/side channels at 1/sqrt(2), back centre at 0.5, and drops LFE. Mono buses
  average that stereo downmix. Mono/stereo can feed FL/FR on larger buses;
  other unequal multichannel mappings are rejected. Downmixes are not normalized.
- Pan only affects stereo buses: mono uses constant power with unity at centre
  and sqrt(2) at hard pan; other sources use the same gains clamped to unity.
  Centre and hard-pan zeros are exact. There is no limiter or transport ramp.
- Contexts outlive mixers; mixers outlive attached players. One-shot clip storage
  outlives the mixer or the next synchronous `stop_one_shots()` call.

## OS subsystem

API-level behaviour is documented in `src/alia/os/`.

| Layer | Location | Role |
|-------|----------|------|
| L1 | `os/*.hpp` public | Contexts, windows, displays, monitors, clipboard and dialogs |
| L2 | Header templates/overloads | Typed bitmap views erase to `any_bitmap_view`; text-log formatting erases to text |
| L3 | `os/*.cpp`, common `io/*.cpp` | Registry, ownership, validation, default-context dispatch and closest-mode selection |
| L4 | `os_backend_interface` | One stable table of `os_backend_op` slots per context |

- Backends live in `os/backend_<name>/`, with concrete handles, casts and operation
  declarations in `<name>_ops.hpp` and registration in `register_<name>_os_backend.cpp`.
  There is no virtual window/cursor implementation in public headers.
- Contexts outlive their windows, cursors and text logs. Moving contexts preserves
  backend-table addresses. Moving windows preserves native handles and event sources
  and updates their owner pointers and tracked input associations.
- Create, use, poll and destroy a context and its children on the same thread.
  Convenience APIs use the lazily created thread-local `get_os_context()`.
- All native operations use `.get_or_throw()`. Null slots describe absent platform
  services, with `reason_unsupported`; they never represent unfinished code.
- L3 selects closest display modes. Backends apply an explicitly requested mode
  exactly or fail. X11 restores its saved RandR configuration when leaving exclusive
  fullscreen, losing focus, or destroying a window.
- X11 owns one Display connection per context. Polling dispatches all its windows
  and clipboard selections. Clipboard owners must keep polling to serve requests;
  transfers have a two-second timeout and a 16 MiB text limit.
- X11 probes RandR, XInput2, XKB, screen-saver and EWMH support at context creation.
  File pickers use the optional Zenity service; missing services have explicit reasons.

## The `slop` folder

The folder contains AI-generated design documents. They may, or may not be in any way relevant to the current state of the repo. They may, or may not be in any way aligned with the user's intent. Only intentionally read documents from that folder if explicitly asked to / pointed at by the user.
