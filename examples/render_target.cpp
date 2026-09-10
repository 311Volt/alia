#include "alia/os/window.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/primitive_renderer.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/events/event_queue.hpp"

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

std::array<alia::uv_vertex, 6> textured_quad(alia::rect_f rectangle) {
    return {{
        {{rectangle.left(), rectangle.top()}, {0.0f, 0.0f}},
        {{rectangle.right(), rectangle.top()}, {1.0f, 0.0f}},
        {{rectangle.left(), rectangle.bottom()}, {0.0f, 1.0f}},
        {{rectangle.right(), rectangle.top()}, {1.0f, 0.0f}},
        {{rectangle.right(), rectangle.bottom()}, {1.0f, 1.0f}},
        {{rectangle.left(), rectangle.bottom()}, {0.0f, 1.0f}},
    }};
}

alia::gfx_backend requested_backend(int argc, char **argv) {
    if (argc < 2) return alia::gfx_backend::auto_;
    const std::string_view value(argv[1]);
    if (value == "d3d9") return alia::gfx_backend::d3d9;
    if (value == "opengl") return alia::gfx_backend::opengl;
    throw std::invalid_argument("backend must be d3d9 or opengl");
}

} // namespace

int main(int argc, char **argv) {
    try {
        alia::window win({900, 540}, {.title = "ALIA render target example", .resizable = true});
        auto device = alia::gfx_device::create(requested_backend(argc, argv));
        alia::make_current(device);
        auto swapchain = device.create_swapchain({.target = win});
        constexpr alia::vec2i target_size{256, 256};
        const alia::vec2f target_size_f{256.0f, 256.0f};
        alia::texture offscreen(device, alia::pixel_format::bgra8888, target_size, 1, alia::texture_role::color, alia::texture_usage::render_target);
        alia::texture copied(device, alia::pixel_format::bgra8888, target_size);

        alia::basic_effect prim_fx;
        auto prim_pipeline = alia::pipeline::create<alia::colored_vertex>(device, {.effect = &prim_fx});
        alia::basic_effect tex_fx{.texture_op = alia::texture_operation::replace};
        auto tex_pipeline = alia::pipeline::create<alia::uv_vertex>(device, {.effect = &tex_fx});
        alia::primitive_renderer renderer;

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
            frame.set_target(offscreen);
            frame.clear(alia::color(0.04f, 0.06f, 0.08f, 1.0f));
            prim_fx.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(prim_pipeline);
            renderer.fill_rect(
                frame,
                alia::rect_f::pos_size({24.0f, 24.0f}, {96.0f, 96.0f}),
                alia::color(1.0f, 0.25f, 0.12f, 1.0f)
            );
            renderer.fill_rect(
                frame,
                alia::rect_f::pos_size({136.0f, 52.0f}, {76.0f, 152.0f}),
                alia::color(0.12f, 0.82f, 0.55f, 0.85f)
            );
            renderer.draw_line(frame, {32.0f, 224.0f}, {224.0f, 32.0f}, alia::white, 5.0f);
            renderer.flush(frame);
            frame.copy_to_texture(copied, alia::rect_i::pos_size({0, 0}, target_size));

            frame.set_target();
            frame.clear(alia::color(0.08f, 0.09f, 0.11f, 1.0f));
            tex_fx.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(tex_pipeline);
            frame.set_texture(0, offscreen);
            const auto offscreen_quad =
                textured_quad(alia::rect_f::pos_size({90.0f, 142.0f}, target_size_f));
            frame.draw<alia::uv_vertex>(offscreen_quad);
            frame.set_texture(0, copied);
            const auto copied_quad =
                textured_quad(alia::rect_f::pos_size({554.0f, 142.0f}, target_size_f));
            frame.draw<alia::uv_vertex>(copied_quad);

            prim_fx.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(prim_pipeline);
            renderer.draw_rect(
                frame,
                alia::rect_f::pos_size({90.0f, 142.0f}, target_size_f),
                alia::white,
                2.0f
            );
            renderer.draw_rect(
                frame,
                alia::rect_f::pos_size({554.0f, 142.0f}, target_size_f),
                alia::white,
                2.0f
            );
            renderer.flush(frame);
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "render target example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
