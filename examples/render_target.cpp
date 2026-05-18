#include "alia/os/window.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/gfx/transform.hpp"
#include "alia/gfx/primitives.hpp"
#include "alia/events/event_queue.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        alia::window win(
            {900, 540},
            {
                .title = "ALIA render target example",
                .resizable = true,
            }
        );

        alia::gfx_device device = alia::gfx_device::create();
        auto swapchain = device.create_swapchain({.target = win});

        constexpr alia::vec2i target_size{256, 256};
        const alia::vec2f target_size_f{
            static_cast<float>(target_size.x),
            static_cast<float>(target_size.y)
        };
        alia::texture offscreen(
            device,
            alia::pixel_format::bgra8888,
            target_size,
            1,
            alia::texture_role::color,
            alia::texture_usage::render_target
        );
        alia::texture copied(
            device,
            alia::pixel_format::bgra8888,
            target_size
        );

        alia::event_queue events;
        events.register_source(&win.get_event_source());

        bool running = true;
        while (running) {
            win.poll();
            while (!events.empty()) {
                auto ev = events.pop();
                if (ev.get_if<alia::window_close_event>()) {
                    running = false;
                } else if (auto *e = ev.get_if<alia::window_resize_event>()) {
                    alia::on_resize(e->new_size);
                } else if (auto *e = ev.get_if<alia::window_key_down_event>()) {
                    if (e->key == alia::key::escape)
                        running = false;
                }
            }

            alia::set_current_projection(alia::transform::ortho_ui(target_size));
            {
                [[maybe_unused]] auto target = offscreen.as_render_target();
                alia::clear(alia::color(0.04f, 0.06f, 0.08f, 1.0f));
                alia::fill_rect(alia::rect_f::pos_size({24.0f, 24.0f}, {96.0f, 96.0f}), alia::color(1.0f, 0.25f, 0.12f, 1.0f));
                alia::fill_rect(alia::rect_f::pos_size({136.0f, 52.0f}, {76.0f, 152.0f}), alia::color(0.12f, 0.82f, 0.55f, 0.85f));
                alia::draw_line({32.0f, 224.0f}, {224.0f, 32.0f}, alia::white, 5.0f);
                alia::copy_render_target_to_texture(copied, alia::rect_i::pos_size({0, 0}, target_size));
            }

            alia::set_current_projection(alia::transform::ortho_ui(win.size()));
            alia::clear(alia::color(0.08f, 0.09f, 0.11f, 1.0f));
            alia::draw_textured_rect(alia::rect_f::pos_size({90.0f, 142.0f}, target_size_f), offscreen);
            alia::draw_textured_rect(alia::rect_f::pos_size({554.0f, 142.0f}, target_size_f), copied);
            alia::draw_rect(alia::rect_f::pos_size({90.0f, 142.0f}, target_size_f), alia::white, 2.0f);
            alia::draw_rect(alia::rect_f::pos_size({554.0f, 142.0f}, target_size_f), alia::white, 2.0f);
            alia::present();
        }
    } catch (const std::exception &e) {
        std::cerr << "render target example failed: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
