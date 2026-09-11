#include "alia/core/get_time.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/bitmap/pixel_types.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/primitive_renderer.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/os/window.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
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

// Avoid rect::f32()/cast(), which currently depends on a missing vec2::cast().
alia::rect_f to_rect_f(alia::rect_i value) {
    return {alia::vec2f(value.p1), alia::vec2f(value.p2)};
}

// HYPOTHETICAL alia API: font::cutoff_point(text, max_width).
std::size_t cutoff_index(alia::font &font, std::string_view text, float max_width) {
    float width = 0.0f;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto codepoint = static_cast<unsigned char>(text[i]);
        const float next_width = width + font.get_glyph_metrics(codepoint).advance;
        if (next_width > max_width)
            return i;
        width = next_width;
    }
    return text.size();
}

template <class LockedRegion>
void paint_green(LockedRegion &locked) {
    auto &view = locked.view();
    for (int y = 0; y < view.height(); ++y)
        for (int x = 0; x < view.width(); ++x)
            view[x, y].g = 255;
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
            {.title = "ALIA kitchen sink example", .resizable = true});
        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        alia::make_current(device);
        auto swapchain = device.create_swapchain({.target = win});

        alia::texture background(device, alia::load_image("./resources/bg.jpg"));
        background.set_sampler(alia::linear_clamp);
        alia::ttf_font font = alia::load_ttf_font("./resources/roboto.ttf", 24);
        alia::hardware_glyph_buffer glyphs(device, font);

        alia::basic_effect prim_fx;
        alia::basic_effect texture_fx{.texture_op = alia::texture_operation::replace};
        alia::basic_effect text_fx{.texture_op = alia::texture_operation::alpha_mask};
        auto prim_pipeline = alia::pipeline::create<alia::colored_vertex>(
            device, {.effect = &prim_fx});
        auto texture_pipeline = alia::pipeline::create<alia::uv_vertex>(
            device, {.effect = &texture_fx});
        auto text_pipeline = alia::pipeline::create<alia::full_vertex>(
            device, {.effect = &text_fx});
        alia::immediate_primitive_renderer renderer;

        alia::event_queue events;
        events.register_source(&win.get_event_source());

        alia::vec2f text_position{320.0f, 240.0f};
        std::string text = "Click to move this text; use the arrow keys";
        // NOTE (API feedback): tick and delta-time bookkeeping are application
        // code because alia has no event-loop timing helpers.
        std::uint64_t tick = 0;
        bool running = true;
        while (running) {
            win.poll();
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>()) {
                    running = false;
                } else if (const auto *resize = event.get_if<alia::window_resize_event>()) {
                    swapchain.on_resize(resize->new_size);
                } else if (const auto *mouse = event.get_if<alia::window_mouse_button_down_event>();
                           mouse && mouse->button == alia::mouse_button::left) {
                    text_position = alia::vec2f(mouse->position);
                } else if (const auto *key = event.get_if<alia::window_key_down_event>()) {
                    if (key->key == alia::key::escape)
                        running = false;
                    else if (key->key == alia::key::up)
                        text = "UP was pressed";
                    else if (key->key == alia::key::down)
                        text = "DOWN was pressed";
                    else if (key->key == alia::key::left)
                        text = "LEFT was pressed";
                    else if (key->key == alia::key::right)
                        text = "RIGHT was pressed";
                }
            }

            ++tick;
            const float max_width =
                10.0f + (0.5f + 0.5f * std::sin(static_cast<float>(alia::get_time()))) * 300.0f;
            const std::string full_text = std::format("{}. tick={}", text, tick);
            const std::string visible_text =
                full_text.substr(0, cutoff_index(font, full_text, max_width));

            const int x = static_cast<int>(tick % static_cast<std::uint64_t>(background.width() - 10));
            const int y = static_cast<int>(tick % static_cast<std::uint64_t>(background.height() - 10));
            const alia::rect_i write_rect = alia::rect_i::pos_size({x, y}, {2, 2});
            // NOTE (API feedback): texture writes have no format-agnostic path.
            if (auto locked = background.lock<alia::px_bgra8888>(write_rect))
                paint_green(locked);
            else if (auto locked = background.lock<alia::px_rgb888>(write_rect))
                paint_green(locked);
            else if (auto locked = background.lock<alia::px_rgba8888>(write_rect))
                paint_green(locked);

            auto frame = swapchain.begin_frame();
            frame.clear(alia::black);

            texture_fx.world = alia::transform::identity();
            texture_fx.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(texture_pipeline);
            frame.set_texture(0, background, alia::linear_clamp);
            const auto background_quad = textured_quad(alia::rect_f::pos_size(
                {0.0f, 0.0f}, alia::vec2f(frame.target_size())));
            frame.draw<alia::uv_vertex>(background_quad);

            // NOTE (API feedback): alia has no scoped world-transform helper.
            prim_fx.world = alia::transform::translate(text_position);
            prim_fx.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(prim_pipeline);
            renderer.draw_line(
                frame, {0.0f, 0.0f}, {max_width, 0.0f}, alia::red, 4.0f);

            text_fx.world = alia::transform::translate(text_position);
            text_fx.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(text_pipeline);
            alia::draw_text(frame, {0.0f, 0.0f}, glyphs, visible_text, alia::white);
            prim_fx.world = alia::transform::identity();
            text_fx.world = alia::transform::identity();

            const alia::rect_i r1 = alia::rect_i::pos_size({95, 160}, {70, 70});
            const alia::rect_i r2 = alia::rect_i::pos_size({70, 30}, {80, 80});
            prim_fx.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(prim_pipeline);
            renderer.draw_rect(frame, to_rect_f(r1), alia::blue);
            renderer.draw_rect(frame, to_rect_f(r2), alia::blue);
            renderer.draw_rect(frame, to_rect_f(r1.union_with(r2)), alia::magenta);
            for (int i = 0; i < 16; ++i) {
                renderer.fill_rect(
                    frame,
                    alia::rect_f::pos_size({16.0f * i, 0.0f}, {16.0f, 16.0f}),
                    alia::color::cga(static_cast<std::uint8_t>(i)));
            }

            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "example1 failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
