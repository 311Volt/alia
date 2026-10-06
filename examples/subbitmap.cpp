#include "alia/core/get_time.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/draw_texture.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/os/window.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

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
        auto swapchain = device.create_swapchain({.target = win});

        alia::bitmap atlas_bitmap = alia::load_image("./resources/terrain.png");
        alia::texture atlas(device, atlas_bitmap);
        atlas.set_sampler(alia::nearest_clamp);
        alia::texture first_tile(
            device,
            atlas_bitmap.view().subview(
                alia::rect_i::pos_size({0, 0}, {16, 16})));
        first_tile.set_sampler(alia::nearest_clamp);

        alia::basic_effect texture_fx{.texture_op = alia::texture_operation::modulate};
        auto texture_pipeline = alia::pipeline::create<alia::full_vertex>(
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
            const alia::rect_f tile_crop = alia::rect_f::pos_size(
                {x * 16.0f, y * 16.0f}, {16.0f, 16.0f});

            auto frame = swapchain.begin_frame();
            frame.clear(alia::black);
            texture_fx.projection = device.ortho_ui(frame.target_size());
            frame.set_pipeline(texture_pipeline);
            alia::draw_texture({
                .target = frame,
                .texture = atlas,
                .texture_slot = 0,
                .source_crop_rect = tile_crop,
                .destination = alia::rect_f::pos_size({100.0f, 100.0f}, {64.0f, 64.0f})
            });

            alia::draw_texture({
                .target = frame,
                .texture = first_tile,
                .texture_slot = 0,
                .destination = alia::rect_f::pos_size({200.0f, 100.0f}, {64.0f, 64.0f})
            });
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "subbitmap example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
