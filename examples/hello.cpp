#include "alia/os/window.hpp"         // window, window_*_event
#include "alia/gfx/gfx_device.hpp"    // gfx_device, swapchain, colored_vertex
#include "alia/gfx/transform.hpp"
#include "alia/gfx/primitives.hpp"
#include "alia/events/event_queue.hpp" // event_queue

#include <print>
#include <iostream>

int main() {
    alia::window win(
        {800, 600},
        { 
            .title = "Hello ALIA — colored triangle", 
            .resizable = true 
        }
    );
    alia::gfx_device device = alia::gfx_device::create(alia::gfx_backend::opengl);
    alia::swapchain  swap   = alia::swapchain::create(device, win);

    alia::set_current_projection(
        alia::transform::ortho(0, 800, 600, 0));

    alia::event_queue events;
    events.register_source(&win.get_event_source());

    alia::colored_vertex tri[3] = {
        {{ 400.0f, 100.0f}, {1.0f, 0.15f, 0.15f}},   // top  — red
        {{ 100.0f, 500.0f}, {0.15f, 1.0f, 0.15f}},   // BL   — green
        {{ 700.0f, 500.0f}, {0.15f, 0.15f, 1.0f}},   // BR   — blue
    };

    bool running = true;

    while (running) {
        win.poll();

        while (!events.empty()) {
            auto ev = events.pop();
            std::cout << std::format("[{:.6f}] event: {}\n", ev.meta.timestamp, ev.meta.event_type_id);
            if (auto* e = ev.get_if<alia::window_close_event>()) {
                running = false;
            } else if (auto* e = ev.get_if<alia::window_resize_event>()) {
                swap.on_resize(e->new_size);
            } else if (auto* e = ev.get_if<alia::window_key_down_event>()) {
                if (e->key == alia::key::escape)
                    running = false;
            }
        }

        alia::clear(alia::color::cornflower_blue);
        alia::draw_triangle(tri[0], tri[1], tri[2]);
        
        alia::fill_rect(alia::rect_f::pos_size({50, 50}, {100, 100}), alia::color(1, 1, 0, 0.5f));
        alia::draw_rect(alia::rect_f::pos_size({200, 50}, {100, 100}), alia::color(0, 1, 1, 1), 5.0f);
        alia::draw_line({50, 200}, {300, 250}, alia::color(1, 0, 1, 1), 3.0f);

        alia::present();
    }

    return 0;
}
