#include "alia/core/timing.hpp"
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
            {1024, 768},
            {.title = "ALIA bouncing logo", .resizable = true});
        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        auto swapchain = device.create_swapchain({.target = win});

        const int mips = device.backend()->texture_generate_mipmaps.is_supported()
            ? alia::full_mip_chain
            : 1;
        alia::texture logo(
            device, alia::load_image("./resources/dvdlogo.png"), mips);
        if (mips != 1)
            logo.generate_mipmaps();
        logo.set_sampler(alia::linear_clamp);

        alia::basic_effect texture_fx{.texture_op = alia::texture_operation::modulate};
        auto texture_pipeline = alia::pipeline::create<alia::full_vertex>(
            device, {.effect = &texture_fx});

        alia::event_queue events;
        events.register_source(&win.get_event_source());

        alia::rect_f logo_rect =
            alia::rect_f::pos_size({40.0f, 70.0f}, {128.0f, 128.0f});
        alia::vec2f speed{256.0f, 256.0f};
        alia::frame_clock clock;
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

            const float dt = static_cast<float>(clock.tick().delta);
            logo_rect.translate_inplace(speed * dt);

            const alia::rect_f screen = alia::rect_f::pos_size(
                {0.0f, 0.0f}, alia::vec2f(swapchain.size()));
            const auto bounds = screen.test_rect(logo_rect);
            if (bounds.x_outside())
                speed.x *= -1.0f;
            if (bounds.y_outside())
                speed.y *= -1.0f;
            logo_rect.clamp_inside_inplace(screen);

            auto frame = swapchain.begin_frame();
            frame.clear(alia::color::from_rgba8(150, 180, 240));
            texture_fx.projection = device.ortho_ui(frame.target_size());
            frame.set_pipeline(texture_pipeline);
            alia::draw_texture({
                .target = frame,
                .texture = logo,
                .texture_slot = 0,
                .destination = logo_rect
            });
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "dvdlogo example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
