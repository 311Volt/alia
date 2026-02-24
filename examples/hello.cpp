#include "alia/os/window.hpp"         // window, window_*_event
#include "alia/gfx/gfx_device.hpp"    // gfx_device, swapchain, colored_vertex
#include "alia/events/event_queue.hpp" // event_queue


int main() {
    printf("a0\n");

    // ── Window ────────────────────────────────────────────────────────
    alia::window win(
        {800, 600},
        { .title = "Hello ALIA — colored triangle", .resizable = true });

    if (!win) return 1;

    // ── Graphics device (picks D3D9 first, OpenGL as fallback) ───────
    alia::gfx_device device = alia::gfx_device::create(alia::gfx_backend::opengl);
    alia::swapchain  swap   = alia::swapchain::create(device, win);

    // ── Event queue subscribed to the window ─────────────────────────
    alia::event_queue events;
    events.register_source(&win.get_event_source());

    // ── Triangle vertices in NDC space ────────────────────────────────
    //    position (x,y)  in [-1, 1];  color (r, g, b)
    alia::colored_vertex tri[3] = {
        {{ 0.0f,  0.6f}, {1.0f, 0.15f, 0.15f}},   // top  — red
        {{-0.6f, -0.5f}, {0.15f, 1.0f, 0.15f}},   // BL   — green
        {{ 0.6f, -0.5f}, {0.15f, 0.15f, 1.0f}},   // BR   — blue
    };

    bool running = true;

    while (running) {
        // ── OS message pump ───────────────────────────────────────────
        win.poll();

        // ── Event dispatch ────────────────────────────────────────────
        while (!events.empty()) {
            auto ev = events.pop();
            if (auto* e = ev.get_if<alia::window_close_event>()) {
                running = false;
            } else if (auto* e = ev.get_if<alia::window_resize_event>()) {
                swap.on_resize(e->new_size);
            } else if (auto* e = ev.get_if<alia::window_key_event>()) {
                // Escape quits
                if (e->pressed && e->vk_code == 27 /*VK_ESCAPE*/)
                    running = false;
            }
        }

        // ── Render ────────────────────────────────────────────────────
        swap.clear(alia::color::cornflower_blue);
        swap.draw_triangle(tri[0], tri[1], tri[2]);
        swap.present();
    }

    return 0;
}
