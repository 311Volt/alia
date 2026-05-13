#include "alia/os/window.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/transform.hpp"
#include "alia/gfx/primitives.hpp"
#include "alia/gfx/bitmap.hpp"
#include "alia/gfx/image_io.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/events/event_queue.hpp"

#include <print>
#include <exception>
#include <iostream>
#include <optional>
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
    std::optional<alia::hardware_glyph_buffer> glyphs;
    try {
        demo_font.emplace(alia::load_ttf_font(demo_font_path(), 32));
        glyphs.emplace(*demo_font);
    } catch (const std::exception& e) {
        std::cerr << "text disabled: " << e.what() << '\n';
    }

    bool running = true;

    while (running) {
        win.poll();

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

        alia::clear(alia::color::cornflower_blue);
        alia::draw_triangle(tri[0], tri[1], tri[2]);
        
        alia::draw_textured_rect(alia::rect_f::pos_size({50, 250}, {256, 256}), checker_tex);
        alia::fill_rect(alia::rect_f::pos_size({50, 50}, {100, 100}), alia::color(1, 1, 0, 0.5f));
        alia::draw_rect(alia::rect_f::pos_size({200, 50}, {100, 100}), alia::color(0, 1, 1, 1), 5.0f);
        alia::draw_line({50, 200}, {300, 250}, alia::color(1, 0, 1, 1), 3.0f);
        if (demo_font && glyphs) {
            alia::draw_text(
                *demo_font,
                "The quick brown fox jumps over the lazy dog",
                alia::color::white,
                {310.0f, 58.0f},
                &*glyphs);
            alia::draw_text(
                *demo_font,
                "1234567890!@#$%^&*()",
                alia::color(0.05f, 0.08f, 0.12f, 1.0f),
                {310.0f, 98.0f},
                &*glyphs);
        }

        alia::present();
    }

    return 0;
}
