#include "alia/core/get_time.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/os/window.hpp"

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

// NOTE (API feedback): alia currently has no sprite or texture-blit helper.
std::array<alia::uv_vertex, 6> textured_quad(
    alia::rect_f dst,
    alia::rect_f uv = {{0.0f, 0.0f}, {1.0f, 1.0f}}
) {
    return {{
        {{dst.left(), dst.top()}, {uv.left(), uv.top()}},
        {{dst.right(), dst.top()}, {uv.right(), uv.top()}},
        {{dst.left(), dst.bottom()}, {uv.left(), uv.bottom()}},
        {{dst.right(), dst.top()}, {uv.right(), uv.top()}},
        {{dst.right(), dst.bottom()}, {uv.right(), uv.bottom()}},
        {{dst.left(), dst.bottom()}, {uv.left(), uv.bottom()}},
    }};
}

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
            {.title = "ALIA bitmap subviews", .resizable = true});
        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        alia::make_current(device);
        auto swapchain = device.create_swapchain({.target = win});

        alia::bitmap atlas_bitmap = alia::load_image("./resources/terrain.png");
        alia::texture atlas(device, atlas_bitmap);
        atlas.set_sampler(alia::nearest_clamp);
        alia::texture first_tile(
            device,
            atlas_bitmap.view().subview(
                alia::rect_i::pos_size({0, 0}, {16, 16})));
        first_tile.set_sampler(alia::nearest_clamp);

        alia::basic_effect texture_fx{.texture_op = alia::texture_operation::replace};
        auto texture_pipeline = alia::pipeline::create<alia::uv_vertex>(
            device, {.effect = &texture_fx});
        alia::event_queue events;
        events.register_source(&win.get_event_source());

        bool running = true;
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

            const int index = static_cast<int>(alia::get_time() * 5.0) % 256;
            const int x = index % 16;
            const int y = index / 16;
            const alia::rect_f uv = alia::rect_f::pos_size(
                {x * 16.0f / 256.0f, y * 16.0f / 256.0f},
                {16.0f / 256.0f, 16.0f / 256.0f});

            auto frame = swapchain.begin_frame();
            frame.clear(alia::black);
            texture_fx.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(texture_pipeline);
            frame.set_texture(0, atlas, alia::nearest_clamp);
            const auto animated_quad = textured_quad(
                alia::rect_f::pos_size({100.0f, 100.0f}, {64.0f, 64.0f}), uv);
            frame.draw<alia::uv_vertex>(animated_quad);

            // HYPOTHETICAL alia API: texture_view and
            // draw_texture(texture, source_rect, destination_rect).
            frame.set_texture(0, first_tile, alia::nearest_clamp);
            const auto copied_quad = textured_quad(
                alia::rect_f::pos_size({200.0f, 100.0f}, {64.0f, 64.0f}));
            frame.draw<alia::uv_vertex>(copied_quad);
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "subbitmap example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
