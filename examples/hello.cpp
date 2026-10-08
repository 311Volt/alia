#include "alia/os/window.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/primitive_renderer.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/core/timing.hpp"

#include <array>
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
        alia::window win({800, 600}, {.title = "Hello ALIA", .resizable = true});
        auto device = alia::gfx_device::create(requested_backend(argc, argv));
        auto swapchain = device.create_swapchain({
            .target = win,
            .vsync = alia::vsync_mode::disable,
        });
        alia::event_queue events;
        events.register_source(&win.get_event_source());

        const alia::colored_vertex triangle[] = {
            {{400.0f, 100.0f}, {1.0f, 0.15f, 0.15f}},
            {{100.0f, 500.0f}, {0.15f, 1.0f, 0.15f}},
            {{700.0f, 500.0f}, {0.15f, 0.15f, 1.0f}},
        };

        alia::immediate_primitive_renderer renderer;
        alia::texture checker(device, alia::load_image("./resources/test.png"));

        std::optional<alia::ttf_font> demo_font;
        std::optional<alia::text_texture> demo_text;
        std::optional<alia::text_texture> demo_numbers;
        std::optional<alia::text_texture> fps_text;
        std::optional<alia::hardware_glyph_buffer> glyph_cache;
        try {
            demo_font.emplace(alia::load_ttf_font(demo_font_path(), 32));
            demo_text.emplace(alia::create_text_texture(
                device,
                *demo_font,
                "The quick brown fox jumps over the lazy dog"
            ));
            demo_numbers.emplace(alia::create_text_texture(
                device,
                *demo_font,
                "1234567890!@#$%^&*()"
            ));
            fps_text.emplace(alia::create_text_texture(device, *demo_font, "FPS: --"));
            glyph_cache.emplace(device, *demo_font);
        } catch (const std::exception &error) {
            std::cerr << "text disabled: " << error.what() << '\n';
            glyph_cache.reset();
            fps_text.reset();
            demo_numbers.reset();
            demo_text.reset();
            demo_font.reset();
        }

        constexpr std::array zigzag{
            alia::vec2f{350.0f, 205.0f},
            alia::vec2f{410.0f, 245.0f},
            alia::vec2f{470.0f, 190.0f},
            alia::vec2f{535.0f, 250.0f},
        };
        const alia::rect_f transformed_rect =
            alia::rect_f::pos_size({440.0f, 350.0f}, {150.0f, 90.0f});
        const alia::vec2f transformed_center = transformed_rect.center();

        bool running = true;
        alia::frame_clock clock;
        alia::fps_counter counter;
        while (running) {
            win.poll();
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>()) running = false;
                else if (const auto *resize = event.get_if<alia::window_resize_event>()) swapchain.on_resize(resize->new_size);
                else if (const auto *key = event.get_if<alia::window_key_down_event>(); key && key->key == alia::key::escape) running = false;
            }

            const float elapsed = static_cast<float>(clock.tick().elapsed);

            auto frame = swapchain.begin_frame();
            frame.clear(alia::light_blue);

            frame.draw<alia::colored_vertex>(triangle);

            renderer.fill_rect(
                frame,
                alia::rect_f::pos_size({50.0f, 50.0f}, {100.0f, 100.0f}),
                alia::color(1.0f, 1.0f, 0.0f, 0.5f)
            );
            renderer.draw_rect(
                frame,
                alia::rect_f::pos_size({200.0f, 50.0f}, {100.0f, 100.0f}),
                alia::color(0.0f, 1.0f, 1.0f, 0.65f),
                5.0f
            );
            renderer.draw_rect(
                frame,
                alia::rect_f::pos_size({350.0f, 50.0f}, {100.0f, 100.0f}),
                alia::color(1.0f, 0.55f, 0.1f, 0.85f),
                8.0f,
                alia::line_join::bevel
            );
            renderer.draw_line(
                frame,
                {50.0f, 200.0f},
                {300.0f, 250.0f},
                alia::color(1.0f, 0.0f, 1.0f, 1.0f),
                3.0f
            );
            renderer.draw_polyline(
                frame,
                zigzag,
                alia::color(0.05f, 0.15f, 0.45f, 1.0f),
                10.0f,
                alia::line_join::bevel
            );

            frame.set_texture(0, checker);
            const auto checker_quad =
                textured_quad(alia::rect_f::pos_size({50.0f, 290.0f}, {256.0f, 256.0f}));
            frame.draw<alia::uv_vertex>(checker_quad);

            {
                auto rotated = frame.push_world(
                    alia::transform::translate(-1.0f * transformed_center) *
                    alia::transform::rotate(elapsed) *
                    alia::transform::translate(transformed_center));
                renderer.draw_rect(frame, transformed_rect, alia::white, 5.0f);
            }

            if (demo_text) {
                alia::draw_text_texture({
                    .target = frame,
                    .texture = *demo_text,
                    .texture_slot = 0,
                    .position = {310.0f, 58.0f}
                });
            }
            if (demo_numbers) {
                alia::draw_text_texture({
                    .target = frame,
                    .texture = *demo_numbers,
                    .texture_slot = 0,
                    .position = {310.0f, 98.0f},
                    .tint = alia::color(0.05f, 0.08f, 0.12f, 1.0f)
                });
            }
            if (fps_text) {
                alia::draw_text_texture({
                    .target = frame,
                    .texture = *fps_text,
                    .texture_slot = 0,
                    .position = {10.0f, 10.0f}
                });
            }
            if (glyph_cache) {
                alia::draw_text({
                    .target = frame,
                    .glyphs = *glyph_cache,
                    .text = "immediate atlas path (hardware_glyph_buffer)",
                    .texture_slot = 0,
                    .position = {310.0f, 138.0f}
                });
            }

            frame.present();
            if (counter.count_frame()) {
                const int fps = static_cast<int>(counter.interval().fps() + 0.5);
                if (fps_text)
                    fps_text = alia::create_text_texture(
                        device,
                        *demo_font,
                        "FPS: " + std::to_string(fps)
                    );
                const std::string title = "Hello ALIA | FPS: " + std::to_string(fps);
                win.set_title(title.c_str());
            }
        }
    } catch (const std::exception &error) {
        std::cerr << "hello example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
