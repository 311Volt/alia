#include "alia/core/timing.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/bitmap/pixel_types.hpp"
#include "alia/gfx/draw_texture.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/primitive_renderer.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/io/mouse.hpp"
#include "alia/os/window.hpp"

#include <cmath>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
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
            {.title = "ALIA kitchen sink example", .resizable = true});
        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        auto swapchain = device.create_swapchain({.target = win});

        alia::texture background(device, alia::load_image("./resources/bg.jpg"));
        background.set_sampler(alia::linear_clamp);
        alia::ttf_font font = alia::load_ttf_font("./resources/roboto.ttf", 24);
        alia::hardware_glyph_buffer glyphs(device, font);

        alia::immediate_primitive_renderer renderer;

        alia::event_queue events;
        events.register_source(&win.get_event_source());

        alia::vec2f text_position{320.0f, 240.0f};
        std::string text = "Click to move this text; use the arrow keys";
        alia::frame_clock clock;
        bool running = true;
        while (running) {
            win.poll();
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>()) {
                    running = false;
                } else if (const auto *resize = event.get_if<alia::window_resize_event>()) {
                    swapchain.on_resize(resize->new_size);
                } else if (const auto *mouse = event.get_if<alia::mouse_button_down_event>();
                           mouse && mouse->button == alia::mouse_button::left) {
                    text_position = mouse->position.as<float>();
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

            const auto tick = clock.tick().tick;
            const float max_width =
                10.0f + (0.5f + 0.5f * std::sin(static_cast<float>(alia::get_time()))) * 300.0f;
            const std::string full_text = std::format("{}. tick={}", text, tick);
            const std::string visible_text =
                full_text.substr(0, font.cutoff_point(full_text, max_width));

            const int x = static_cast<int>(tick % static_cast<std::uint64_t>(background.width() - 10));
            const int y = static_cast<int>(tick % static_cast<std::uint64_t>(background.height() - 10));
            const alia::rect_i write_rect = alia::rect_i::pos_size({x, y}, {2, 2});
            if (auto locked = background.lock_any(write_rect))
                locked.visit_rows<alia::px_rgba8888>([](std::span<alia::px_rgba8888> row, int) {
                    for (auto &px : row)
                        px.g = 255;
                });

            auto frame = swapchain.begin_frame();
            frame.clear(alia::black);

            alia::draw_texture({
                .target = frame,
                .texture = background,
                .texture_slot = 0,
                .destination = alia::rect_f::pos_size(
                    {0.0f, 0.0f}, frame.target_size().as<float>())
            });

            {
                auto translated = frame.push_world(alia::transform::translate(text_position));
                renderer.draw_line(
                    frame, {0.0f, 0.0f}, {max_width, 0.0f}, alia::red, 4.0f);
                alia::draw_text({
                    .target = frame,
                    .glyphs = glyphs,
                    .text = visible_text,
                    .texture_slot = 0,
                    .position = {0.0f, 0.0f},
                    .tint = alia::white
                });
            }

            const alia::rect_i r1 = alia::rect_i::pos_size({95, 160}, {70, 70});
            const alia::rect_i r2 = alia::rect_i::pos_size({70, 30}, {80, 80});
            renderer.draw_rect(frame, r1.as<float>(), alia::blue);
            renderer.draw_rect(frame, r2.as<float>(), alia::blue);
            renderer.draw_rect(frame, r1.union_with(r2).as<float>(), alia::magenta);
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
