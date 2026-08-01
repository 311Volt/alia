#include "alia/os/window.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/painter.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/events/event_queue.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
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

alia::gfx_backend requested_backend(int argc, char **argv) {
    if (argc < 2) return alia::gfx_backend::auto_;
    const std::string_view value(argv[1]);
    if (value == "d3d9") return alia::gfx_backend::d3d9;
    if (value == "opengl") return alia::gfx_backend::opengl;
    throw std::invalid_argument("backend must be d3d9 or opengl");
}
}

int main(int argc, char **argv) {
    try {
        alia::window win({800, 600}, {.title = "Hello ALIA — pipelines", .resizable = true});
        auto device = alia::gfx_device::create(requested_backend(argc, argv));
        alia::make_current(device);
        auto swapchain = device.create_swapchain({.target = win});
        alia::event_queue events;
        events.register_source(&win.get_event_source());

        alia::basic_effect triangle_effect;
        auto triangle_pipeline = alia::pipeline::create<alia::colored_vertex>(device, {.effect = &triangle_effect});
        const alia::colored_vertex triangle[] = {
            {{400.0f, 100.0f}, {1.0f, 0.15f, 0.15f}},
            {{100.0f, 500.0f}, {0.15f, 1.0f, 0.15f}},
            {{700.0f, 500.0f}, {0.15f, 0.15f, 1.0f}},
        };
        alia::texture checker(device, alia::load_image("./resources/test.png"));
        alia::painter painter(device);
        std::optional<alia::ttf_font> demo_font;
        std::optional<alia::text> demo_text;
        std::optional<alia::text> demo_numbers;
        std::optional<alia::text> fps_text;
        std::optional<alia::hardware_glyph_buffer> glyph_cache;
        try {
            demo_font.emplace(alia::load_ttf_font(demo_font_path(), 32));
            demo_text.emplace(*demo_font);
            demo_text->set_text("The quick brown fox jumps over the lazy dog");
            demo_numbers.emplace(*demo_font);
            demo_numbers->set_text("1234567890!@#$%^&*()");
            fps_text.emplace(*demo_font);
            fps_text->set_text("FPS: --");
            glyph_cache.emplace(*demo_font);
        } catch (const std::exception &error) {
            std::cerr << "text disabled: " << error.what() << '\n';
            glyph_cache.reset();
            fps_text.reset();
            demo_numbers.reset();
            demo_text.reset();
            demo_font.reset();
        }

        bool running = true;
        auto fps_window_start = std::chrono::steady_clock::now();
        int fps_frames = 0;
        while (running) {
            win.poll();
            ++fps_frames;
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>()) running = false;
                else if (const auto *resize = event.get_if<alia::window_resize_event>()) swapchain.on_resize(resize->new_size);
                else if (const auto *key = event.get_if<alia::window_key_down_event>(); key && key->key == alia::key::escape) running = false;
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

            auto frame = swapchain.begin_frame();
            frame.clear(alia::light_blue);
            frame.set_pipeline(triangle_pipeline);
            triangle_effect.projection = alia::transform::ortho_ui(frame.target_size());
            frame.draw<alia::colored_vertex>(triangle);

            painter.begin(frame);
            painter.fill_rect(alia::rect_f::pos_size({50, 50}, {100, 100}), alia::color(1, 1, 0, 0.5f));
            painter.draw_textured_rect(alia::rect_f::pos_size({50, 250}, {256, 256}), checker);
            painter.draw_rect(alia::rect_f::pos_size({200, 50}, {100, 100}), alia::color(0, 1, 1, 1), 5.0f);
            painter.draw_line({50, 200}, {300, 250}, alia::color(1, 0, 1, 1), 3.0f);
            if (demo_text)
                painter.draw_text({310, 58}, *demo_text);
            if (demo_numbers)
                painter.draw_text({310, 98}, *demo_numbers, alia::color(0.05f, 0.08f, 0.12f, 1.0f));
            if (fps_text)
                painter.draw_text({10, 10}, *fps_text);
            if (demo_font && glyph_cache) {
                painter.draw_text(
                    {310.0f, 138.0f},
                    *demo_font,
                    "immediate atlas path (hardware_glyph_buffer)",
                    alia::white,
                    &*glyph_cache
                );
            }
            painter.end();
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "hello example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
