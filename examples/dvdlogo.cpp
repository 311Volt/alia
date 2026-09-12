#include "alia/core/get_time.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/os/window.hpp"

#include <algorithm>
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

// HYPOTHETICAL alia API: rect::test(rect) could expose these results directly.
bool outside_x(alia::rect_f inner, alia::rect_f outer) {
    return inner.left() < outer.left() || inner.right() > outer.right();
}

bool outside_y(alia::rect_f inner, alia::rect_f outer) {
    return inner.top() < outer.top() || inner.bottom() > outer.bottom();
}

// HYPOTHETICAL alia API: rect::clamp(rect).
alia::rect_f clamp_inside(alia::rect_f inner, alia::rect_f outer) {
    const float max_x = (std::max)(outer.left(), outer.right() - inner.width());
    const float max_y = (std::max)(outer.top(), outer.bottom() - inner.height());
    const alia::vec2f position{
        std::clamp(inner.left(), outer.left(), max_x),
        std::clamp(inner.top(), outer.top(), max_y),
    };
    return alia::rect_f::pos_size(position, inner.size());
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

        alia::basic_effect texture_fx{.texture_op = alia::texture_operation::replace};
        auto texture_pipeline = alia::pipeline::create<alia::uv_vertex>(
            device, {.effect = &texture_fx});

        alia::event_queue events;
        events.register_source(&win.get_event_source());

        alia::rect_f logo_rect =
            alia::rect_f::pos_size({40.0f, 70.0f}, {128.0f, 128.0f});
        alia::vec2f speed{256.0f, 256.0f};
        // NOTE (API feedback): alia has no event-loop delta-time helper.
        double last_time = alia::get_time();
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

            const double now = alia::get_time();
            const float dt = static_cast<float>(now - last_time);
            last_time = now;
            logo_rect.translate_inplace(speed * dt);

            const alia::rect_f screen = alia::rect_f::pos_size(
                {0.0f, 0.0f}, alia::vec2f(swapchain.size()));
            if (outside_x(logo_rect, screen))
                speed.x *= -1.0f;
            if (outside_y(logo_rect, screen))
                speed.y *= -1.0f;
            logo_rect = clamp_inside(logo_rect, screen);

            auto frame = swapchain.begin_frame();
            frame.clear(alia::color::from_rgba8(150, 180, 240));
            texture_fx.projection = device.ortho_ui(frame.target_size());
            frame.set_pipeline(texture_pipeline);
            frame.set_texture(0, logo, alia::linear_clamp);
            const auto quad = textured_quad(logo_rect);
            frame.draw<alia::uv_vertex>(quad);
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "dvdlogo example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
