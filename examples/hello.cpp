#include "alia/os/window.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/transform.hpp"
#include "alia/gfx/primitives.hpp"
#include "alia/gfx/bitmap.hpp"
#include "alia/gfx/image_io.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/events/event_queue.hpp"

#include <chrono>
#include <print>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

std::string_view demo_font_path() {
#if defined(_WIN32)
    return "C:/Windows/Fonts/segoeui.ttf";
#elif defined(__APPLE__)
    return "/System/Library/Fonts/Supplemental/Arial.ttf";
#else
    return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
#endif
}

} // namespace

int main() {
    alia::window win(
        {800, 600},
        { 
            .title = "Hello ALIA — colored triangle", 
            .resizable = true 
        }
    );
    alia::gfx_device device = alia::gfx_device::create(alia::gfx_backend::d3d9);
    auto swapchain = device.create_swapchain({.target = win});

    alia::set_current_projection(alia::transform::ortho_ui(win.size()));

    alia::event_queue events;
    events.register_source(&win.get_event_source());

    alia::colored_vertex tri[3] = {
        {{ 400.0f, 100.0f}, {1.0f, 0.15f, 0.15f}},   // top  — red
        {{ 100.0f, 500.0f}, {0.15f, 1.0f, 0.15f}},   // BL   — green
        {{ 700.0f, 500.0f}, {0.15f, 0.15f, 1.0f}},   // BR   — blue
    };

    alia::bitmap  checker_bmp = alia::load_image("./resources/test.png");
    alia::texture checker_tex(device, checker_bmp);

    std::optional<alia::ttf_font> demo_font;
    std::optional<alia::text> demo_text;
    std::optional<alia::text> demo_numbers;
    std::optional<alia::text> fps_text;
    try {
        demo_font.emplace(alia::load_ttf_font(demo_font_path(), 32));
        demo_text.emplace(*demo_font);
        demo_text->set_text("The quick brown fox jumps over the lazy dog");
        demo_numbers.emplace(*demo_font);
        demo_numbers->set_text("1234567890!@#$%^&*()");
        fps_text.emplace(*demo_font);
        fps_text->set_text("FPS: --");
    } catch (const std::exception& e) {
        std::cerr << "text disabled: " << e.what() << '\n';
    }

    bool running = true;
    auto fps_window_start = std::chrono::steady_clock::now();
    int fps_frames = 0;

    while (running) {
        win.poll();
        ++fps_frames;

        while (!events.empty()) {
            auto ev = events.pop();
            if (auto* e = ev.get_if<alia::window_close_event>()) {
                running = false;
            } else if (auto* e = ev.get_if<alia::window_resize_event>()) {
                alia::on_resize(e->new_size);
                alia::set_current_projection(alia::transform::ortho_ui(e->new_size));
            } else if (auto* e = ev.get_if<alia::window_key_down_event>()) {
                if (e->key == alia::key::escape)
                    running = false;
            }
        }

        const auto fps_now = std::chrono::steady_clock::now();
        const float fps_elapsed = std::chrono::duration<float>(fps_now - fps_window_start).count();
        if (fps_elapsed >= 1.0f) {
            if (fps_text) {
                const int fps = static_cast<int>(static_cast<float>(fps_frames) / fps_elapsed + 0.5f);
                fps_text->set_text("FPS: " + std::to_string(fps));
            }
            fps_window_start = fps_now;
            fps_frames = 0;
        }

        alia::clear(alia::color::cornflower_blue);
        alia::draw_triangle(tri[0], tri[1], tri[2]);
        
        alia::draw_textured_rect(alia::rect_f::pos_size({50, 250}, {256, 256}), checker_tex);
        alia::fill_rect(alia::rect_f::pos_size({50, 50}, {100, 100}), alia::color(1, 1, 0, 0.5f));
        alia::draw_rect(alia::rect_f::pos_size({200, 50}, {100, 100}), alia::color(0, 1, 1, 1), 5.0f);
        alia::draw_line({50, 200}, {300, 250}, alia::color(1, 0, 1, 1), 3.0f);
        if (demo_text && demo_numbers) {
            demo_text->draw({310, 58});
            demo_numbers->draw({310, 98}, alia::color(0.05f, 0.08f, 0.12f, 1.0f));
        }
        if (fps_text)
            fps_text->draw({10, 10}, alia::color::white);

        alia::present();
    }

    return 0;
}
