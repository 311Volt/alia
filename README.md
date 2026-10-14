# alia

Welcome!

This is the **Advanced Layer for Interactive Applications**, or **ALIA** for short. The goal of this library is to deliver a set of C++23 templates for programming interactive applications (particularly video games) in a way that takes maximum advantage of programming techniques made possible by recent C++ standards.

# Status and plans

The project is in a very early stage. Do NOT use it yet.

### Planned backends

  - Graphics
    - DirectX 9
    - OpenGL
    - Software
    - later: modern APIs (D3D12, Vulkan, Metal)
  - OS 
    - Win32
    - Linux/X11
    - macOS
    - Android, iPhone OS?
  - Audio
    - DirectSound
    - ALSA
    - PipeWire

# Goals

 - Maximum portability and compatibility
 - "Simple by default, powerful when needed" as the most important design principle
 - Flexibility: can be configured to use any combination of compatible backends for each module


# Is it for me?

This library is for you if you're looking to:
 - program some games the old-fashioned way
 - make your own game engine
 - test out technical ideas outside of constraints of a game engine
  
ALIA only serves to abstract away the underlying platform and is not a game engine. You are responsible for the structure of your program.

# Examples

TODO

# OS contexts and X11

`alia/os/os_context.hpp` provides a move-only OS context and backend selection:

```cpp
auto os = alia::os_context::create(alia::os_backend::x11);
alia::window win(os, {800, 600}, {.title = "ALIA", .resizable = true});
// Call os.poll() regularly on this thread, then consume queued window events.
```

Destroy windows, custom mouse cursors and text logs before their context. Moving
the context preserves their backend references. Existing `window(size, options)`
and free OS functions use a lazy context on the calling thread; `auto_` tries
Win32 then X11 among compiled backends. Explicit contexts also provide monitor
and display-mode queries, and clipboard functions have context-taking overloads.

`ALIA_COMPILE_PLATFORM_BACKEND_X11` enables Xlib, RandR, Xi, Xcursor and Xss
dependencies. Their development headers and libraries must be installed. At runtime
the backend probes server extensions and window-manager capabilities. Unsupported
services throw `unsupported_operation_exception` with an explanation. X11 window
geometry uses native pixels, and changes requested through a window manager become
visible when its events are polled. Exclusive modes are applied exactly; RandR
framebuffers expand when necessary and the saved configuration is restored on
exit or focus loss when the remaining active displays permit it.

X11 clipboard text uses UTF-8 and the CLIPBOARD selection. Keep polling the owning
context while other applications read it. Transfers support the INCR protocol,
with a two-second timeout and a 16 MiB limit. Text remains available while the
context owns the selection; shutdown does not transfer ownership to a clipboard
manager. Set the application's character locale before context creation for XIM
composition and font-set text rendering.

File pickers use Zenity when it is found in PATH at context creation. Without it,
file-picker slots report an unavailable desktop service. Zenity controls hidden-file
visibility through its own UI; requesting `show_hidden = true` explicitly reports
that this option cannot be forced. Message boxes and text logs use Xlib directly.

`alia_example_os_context x11` demonstrates OS windows, keyboard/text events,
resizing, fullscreen and clipboard access. It does not create a graphics device.
The `alia_example_mouse` example also supports X11. OpenGL context/surface support
is a separate graphics-backend concern; this OS backend does not add GLX.

# Timing and FPS measurement

Include `alia/core/timing.hpp` for measurement helpers, or `alia/alia.hpp` for the
umbrella API. All public time values are `double` seconds. `get_time()` uses a
monotonic clock and is also the time source for event metadata.

`frame_clock` tracks calls to `tick()`, returning the tick number (starting at 1),
the actual delta since construction/reset or the preceding tick, and elapsed time
since construction/reset. It does not clamp long deltas or impose a simulation step.

`fps_counter` records completed frames only when you call `count_frame()`. Construct
or reset it immediately before the measured work. Call it after presentation to
include the rendering and presentation costs in your measurement:

```cpp
alia::frame_clock clock;
alia::fps_counter counter(0.25); // Report at least every quarter second.

while (running) {
    win.poll();
    // Handle queued events here.
    const auto timing = clock.tick();
    // Update using timing.delta; timing.elapsed and timing.tick are also available.

    auto frame = swapchain.begin_frame();
    // Clear and draw here.
    frame.present();

    if (counter.count_frame()) {
        const auto report = counter.interval();
        const auto fps = report.fps();
        const auto milliseconds = 1000.0 * report.mean_frame_time();
        // Update your title or FPS display using this report.
    }
}
const auto whole_run = counter.total();
```

The default reporting interval is one second. `interval()` returns the last
completed report and remains unchanged until the next report. `total()` covers
reset through the latest counted frame. Both getters inspect recorded values
without sampling the clock. Long gaps contribute their full duration and produce
one report. FPS is the frame count divided by elapsed time; mean frame time is the
elapsed time divided by the count. Empty or zero-duration statistics return zero.

The basic counter has fixed-size, allocation-free storage. Use
`sliding_fps_counter(window_seconds, interval_seconds)` when you also want
`sliding()` statistics. Its timestamp history grows with the frames in the window.
It retains the boundary preceding that window so statistics represent whole frame
intervals; their span can exceed the requested window by one frame interval. A
long stalled frame contributes its entire duration. Calling `count_frame()` or
`reset()` through an `fps_counter&` also updates this subclass's history.

Clock and counter construction, updates, and resets accept an optional explicit
timestamp for replay or deterministic testing. Timestamps must be finite and
nondecreasing between resets, with a finite elapsed span; equal timestamps are
valid. A reset may rebase to an earlier time. Intervals/windows must be finite and
positive. Invalid inputs throw `std::invalid_argument` before changing state.
Measurement helpers need external synchronization if shared across threads.

# Periodic event timers

Include `alia/events/timer.hpp` and `alia/events/event_queue.hpp`. A timer starts
stopped with count zero. Register its event source before starting it:

```cpp
alia::timer timer(1.0 / 60.0);
alia::event_queue events;
events.register_source(&timer.get_event_source());
timer.start();

if (events.wait_for_event(1.0)) {
    const auto event = events.pop();
    if (const auto* tick = event.get_if<alia::timer_event>()) {
        // tick->count identifies the tick; event.meta.source identifies the source.
    }
}
timer.stop();
```

All timers share a lazily created background scheduler using absolute monotonic
deadlines. Every elapsed period increments the count, including when no queues
are registered. Every overdue tick produces a separate event for each registered
queue; there is no coalescing or catch-up cap. Event timestamps record emission
time. Scheduling is subject to the operating system's wake-up granularity.

`start()` begins a full interval when stopped. `resume()` preserves the remaining
interval saved by `stop()`. Both do nothing while running and preserve the count;
use `set_count(0)` to reset it. `stop()` settles ticks due at its cutoff and finishes
emission before returning, retaining already queued events. `set_period()` first
settles ticks under the old period, then begins a fresh interval; while stopped it
replaces the saved delay. The period must be finite, positive, and representable
by `steady_clock`, at least one clock tick. Fractional clock ticks round up.

Moving a timer preserves its source address and queue registrations. Destruction
waits for emission and unregisters the source; the last timer owner shuts down
and joins the shared worker. Queued payloads remain valid after destruction, but
their metadata source pointers must not be dereferenced. Control/query operations
are synchronized; callers must synchronize movement and destruction with other
users. A moved-from timer supports destruction and move assignment only.

`event_queue::wait_for_event()` does not pump native window messages. Window
applications must still call `win.poll()` regularly, including in timer-driven
loops. The `timer` example demonstrates queue registration and tick handling
without a window.
