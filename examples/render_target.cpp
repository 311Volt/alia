#include "alia/os/window.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/simplified_render_pass.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/events/event_queue.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
alia::gfx_backend requested_backend(int argc, char **argv) {
    if (argc < 2) return alia::gfx_backend::auto_;
    const std::string_view value(argv[1]);
    if (value == "d3d9") return alia::gfx_backend::d3d9;
    if (value == "opengl") return alia::gfx_backend::opengl;
    throw std::invalid_argument("backend must be d3d9 or opengl");
}
}

int main(int argc, char **argv) {
    try {
        alia::window win({900, 540}, {.title = "ALIA render target example", .resizable = true});
        auto device = alia::gfx_device::create(requested_backend(argc, argv));
        auto swapchain = device.create_swapchain({.target = win});
        constexpr alia::vec2i target_size{256, 256};
        const alia::vec2f target_size_f{256.0f, 256.0f};
        alia::texture offscreen(device, alia::pixel_format::bgra8888, target_size, 1, alia::texture_role::color, alia::texture_usage::render_target);
        alia::texture copied(device, alia::pixel_format::bgra8888, target_size);
        alia::event_queue events;
        events.register_source(&win.get_event_source());

        bool running = true;
        while (running) {
            win.poll();
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>()) running = false;
                else if (const auto *resize = event.get_if<alia::window_resize_event>()) swapchain.on_resize(resize->new_size);
                else if (const auto *key = event.get_if<alia::window_key_down_event>(); key && key->key == alia::key::escape) running = false;
            }
            auto frame = swapchain.begin_frame();
            {
                alia::simplified_render_pass pass(device, frame, offscreen, {.clear_color = alia::color(0.04f, 0.06f, 0.08f, 1.0f)});
                pass.fill_rect(alia::rect_f::pos_size({24.0f, 24.0f}, {96.0f, 96.0f}), alia::color(1.0f, 0.25f, 0.12f, 1.0f));
                pass.fill_rect(alia::rect_f::pos_size({136.0f, 52.0f}, {76.0f, 152.0f}), alia::color(0.12f, 0.82f, 0.55f, 0.85f));
                pass.draw_line({32.0f, 224.0f}, {224.0f, 32.0f}, alia::white, 5.0f);
                pass.pass().copy_to_texture(copied, alia::rect_i::pos_size({0, 0}, target_size));
            }
            {
                alia::simplified_render_pass pass(device, frame, {.clear_color = alia::color(0.08f, 0.09f, 0.11f, 1.0f)});
                pass.draw_textured_rect(alia::rect_f::pos_size({90.0f, 142.0f}, target_size_f), offscreen);
                pass.draw_textured_rect(alia::rect_f::pos_size({554.0f, 142.0f}, target_size_f), copied);
                pass.draw_rect(alia::rect_f::pos_size({90.0f, 142.0f}, target_size_f), alia::white, 2.0f);
                pass.draw_rect(alia::rect_f::pos_size({554.0f, 142.0f}, target_size_f), alia::white, 2.0f);
            }
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "render target example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
