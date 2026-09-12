#include "alia/os/window.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/primitive_renderer.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/events/event_queue.hpp"

#include <array>
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
        alia::window win({800, 600}, {.title = "Hello ALIA — pipelines", .resizable = true});
        auto device = alia::gfx_device::create(requested_backend(argc, argv));
        auto swapchain = device.create_swapchain({
            .target = win,
            .vsync = alia::vsync_mode::disable,
        });
        alia::event_queue events;
        events.register_source(&win.get_event_source());

        alia::basic_effect triangle_effect;
        auto triangle_pipeline = alia::pipeline::create<alia::colored_vertex>(device, {.effect = &triangle_effect});
        const alia::colored_vertex triangle[] = {
            {{400.0f, 100.0f}, {1.0f, 0.15f, 0.15f}},
            {{100.0f, 500.0f}, {0.15f, 1.0f, 0.15f}},
            {{700.0f, 500.0f}, {0.15f, 0.15f, 1.0f}},
        };

        alia::basic_effect prim_fx;
        auto prim_pipeline = alia::pipeline::create<alia::colored_vertex>(device, {.effect = &prim_fx});
        alia::basic_effect tex_fx{.texture_op = alia::texture_operation::replace};
        auto tex_pipeline = alia::pipeline::create<alia::uv_vertex>(device, {.effect = &tex_fx});
        alia::basic_effect text_fx{.texture_op = alia::texture_operation::alpha_mask};
        auto text_pipeline = alia::pipeline::create<alia::full_vertex>(device, {.effect = &text_fx});
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
        const auto animation_start = std::chrono::steady_clock::now();
        auto fps_window_start = animation_start;
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

            const auto now = std::chrono::steady_clock::now();
            const float elapsed = std::chrono::duration<float>(now - animation_start).count();
            const float fps_elapsed = std::chrono::duration<float>(now - fps_window_start).count();
            if (fps_elapsed >= 1.0f) {
                const int fps = static_cast<int>(static_cast<float>(fps_frames) / fps_elapsed + 0.5f);
                if (fps_text)
                    fps_text = alia::create_text_texture(
                        device,
                        *demo_font,
                        "FPS: " + std::to_string(fps)
                    );
                const std::string title = "Hello ALIA — pipelines | FPS: " + std::to_string(fps);
                win.set_title(title.c_str());
                fps_window_start = now;
                fps_frames = 0;
            }

            auto frame = swapchain.begin_frame();
            frame.clear(alia::light_blue);

            triangle_effect.projection = device.ortho_ui(frame.target_size());
            frame.set_pipeline(triangle_pipeline);
            frame.draw<alia::colored_vertex>(triangle);

            prim_fx.world = alia::transform::identity();
            prim_fx.projection = device.ortho_ui(frame.target_size());
            frame.set_pipeline(prim_pipeline);
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

            tex_fx.projection = device.ortho_ui(frame.target_size());
            frame.set_pipeline(tex_pipeline);
            frame.set_texture(0, checker);
            const auto checker_quad =
                textured_quad(alia::rect_f::pos_size({50.0f, 290.0f}, {256.0f, 256.0f}));
            frame.draw<alia::uv_vertex>(checker_quad);

            prim_fx.world =
                alia::transform::translate(-1.0f * transformed_center) *
                alia::transform::rotate(elapsed) *
                alia::transform::translate(transformed_center);
            frame.set_pipeline(prim_pipeline);
            renderer.draw_rect(frame, transformed_rect, alia::white, 5.0f);
            prim_fx.world = alia::transform::identity();

            text_fx.projection = device.ortho_ui(frame.target_size());
            frame.set_pipeline(text_pipeline);
            if (demo_text)
                alia::draw_text(frame, {310.0f, 58.0f}, *demo_text);
            if (demo_numbers) {
                alia::draw_text(
                    frame,
                    {310.0f, 98.0f},
                    *demo_numbers,
                    alia::color(0.05f, 0.08f, 0.12f, 1.0f)
                );
            }
            if (fps_text)
                alia::draw_text(frame, {10.0f, 10.0f}, *fps_text);
            if (glyph_cache) {
                alia::draw_text(
                    frame,
                    {310.0f, 138.0f},
                    *glyph_cache,
                    "immediate atlas path (hardware_glyph_buffer)"
                );
            }

            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "hello example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
