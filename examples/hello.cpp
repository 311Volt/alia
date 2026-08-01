#include "alia/os/window.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/painter.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
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
        alia::window win({800, 600}, {.title = "Hello ALIA — pipelines", .resizable = true});
        auto device = alia::gfx_device::create(requested_backend(argc, argv));
        alia::make_current(device);
        auto swapchain = device.create_swapchain({.target = win});
        alia::event_queue events;
        events.register_source(&win.get_event_source());

        alia::basic_effect triangle_effect;
        auto triangle_pipeline = alia::pipeline::create<alia::colored_vertex>(device, {.effect = &triangle_effect});
        const alia::colored_vertex triangle[] = {
            {{400.0f, 100.0f}, {1.0f, 0.15f, 0.15f}},
            {{100.0f, 500.0f}, {0.15f, 1.0f, 0.15f}},
            {{700.0f, 500.0f}, {0.15f, 0.15f, 1.0f}},
        };
        alia::texture checker(device, alia::load_image("./resources/test.png"));
        alia::painter painter(device);

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
            frame.clear(alia::light_blue);
            frame.set_pipeline(triangle_pipeline);
            triangle_effect.projection = alia::transform::ortho_ui(frame.target_size());
            frame.draw<alia::colored_vertex>(triangle);

            painter.begin(frame);
            painter.fill_rect(alia::rect_f::pos_size({50, 50}, {100, 100}), alia::color(1, 1, 0, 0.5f));
            painter.draw_textured_rect(alia::rect_f::pos_size({50, 250}, {256, 256}), checker);
            painter.draw_rect(alia::rect_f::pos_size({200, 50}, {100, 100}), alia::color(0, 1, 1, 1), 5.0f);
            painter.draw_line({50, 200}, {300, 250}, alia::color(1, 0, 1, 1), 3.0f);
            painter.end();
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "hello example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
