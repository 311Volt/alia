/* Custom position/UV/color vertices are supported below. User attributes and
 * normalized integer vertex storage are not implementable yet; the excluded
 * declaration shows the additional API surface this example needs. */
#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/os/window.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {

struct my_vertex {
    alia::vec2f pos;
    alia::vec2f uv;
    alia::color col = alia::white;

    static constexpr auto elements() {
        return std::array<alia::vertex_element, 3>{{
            {alia::vertex_attr::position, alia::vertex_storage::float_2, offsetof(my_vertex, pos)},
            {alia::vertex_attr::tex_coord, alia::vertex_storage::float_2, offsetof(my_vertex, uv)},
            {alia::vertex_attr::color_attr, alia::vertex_storage::float_4, offsetof(my_vertex, col)},
        }};
    }
};

#if 0
// HYPOTHETICAL alia API: user attributes and normalized 16-bit storage.
struct advanced_vertex {
    alia::vec3f pos;
    alia::vec4f customdata;
    std::array<std::int16_t, 4> normal;

    static constexpr auto elements() {
        return std::array<alia::vertex_element, 3>{{
            {alia::vertex_attr::position, alia::vertex_storage::float_3, offsetof(advanced_vertex, pos)},
            {alia::vertex_attr::user0, alia::vertex_storage::float_4, offsetof(advanced_vertex, customdata)},
            {alia::vertex_attr::normal, alia::vertex_storage::norm_i16_4, offsetof(advanced_vertex, normal)},
        }};
    }
};
#endif

alia::gfx_backend requested_backend(int argc, char **argv) {
    if (argc < 2)
        return alia::gfx_backend::auto_;
    const std::string_view value(argv[1]);
    if (value == "d3d9")
        return alia::gfx_backend::d3d9;
    if (value == "opengl")
        return alia::gfx_backend::opengl;
    throw std::invalid_argument("backend must be d3d9 or opengl");
}

} // namespace


int main(int argc, char **argv) {
    try {
        alia::window win(
            {800, 600},
            {.title = "ALIA custom vertices", .resizable = true});
        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        auto swapchain = device.create_swapchain({.target = win});

        alia::texture background(device, alia::load_image("./resources/bg.jpg"));
        alia::basic_effect fx{
            .texture_op = alia::texture_operation::modulate,
            .world = alia::transform::scale({100.0f, 100.0f}),
        };
        auto draw_pipeline = alia::pipeline::create<my_vertex>(
            device, {.effect = &fx});

        const std::array vertices{
            my_vertex{.pos = {1.0f, 1.0f}, .uv = {0.0f, 0.0f}},
            my_vertex{.pos = {2.0f, 2.0f}, .uv = {0.0f, 1.0f}, .col = alia::blue},
            my_vertex{.pos = {3.0f, 1.0f}, .uv = {1.0f, 0.0f}},
        };

        alia::event_queue events;
        events.register_source(&win.get_event_source());
        bool running = true;
        auto next_frame = std::chrono::steady_clock::now();
        while (running) {
            win.poll();
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>())
                    running = false;
                else if (const auto *resize = event.get_if<alia::window_resize_event>())
                    swapchain.on_resize(resize->new_size);
                else if (const auto *key = event.get_if<alia::window_key_down_event>();
                         key && key->key == alia::key::escape)
                    running = false;
            }

            auto frame = swapchain.begin_frame();
            frame.clear(alia::black);
            fx.projection = device.ortho_ui(frame.target_size());
            frame.set_pipeline(draw_pipeline);
            frame.set_texture(0, background, alia::linear_clamp);
            frame.draw<my_vertex>(vertices);
            frame.present();

            // HYPOTHETICAL alia API: framerate_limiter{30_Hz}.wait().
            next_frame += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(1.0 / 30.0));
            std::this_thread::sleep_until(next_frame);
        }
    } catch (const std::exception &error) {
        std::cerr << "vertices example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
