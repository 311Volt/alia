#include "alia/core/get_time.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/io/keyboard.hpp"
#include "alia/os/window.hpp"

#include <array>
#include <cmath>
#include <exception>
#include <format>
#include <iostream>
#include <numbers>
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
            {.title = "ALIA 3D camera", .resizable = true});
        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        auto swapchain = device.create_swapchain({.target = win});

        const std::array triangle{
            alia::uv_vertex3d{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            alia::uv_vertex3d{{0.0f, 1.5f, 0.0f}, {0.0f, 1.0f}},
            alia::uv_vertex3d{{1.5f, 0.0f, 1.5f}, {1.0f, 0.0f}},
        };
        alia::texture background(device, alia::load_image("./resources/bg.jpg"));
        alia::basic_effect scene_fx{.texture_op = alia::texture_operation::replace};
        auto scene_pipeline = alia::pipeline::create<alia::uv_vertex3d>(
            device, {.effect = &scene_fx});

        alia::ttf_font font = alia::load_ttf_font("./resources/roboto.ttf", 16);
        alia::hardware_glyph_buffer glyphs(device, font);
        alia::basic_effect text_fx{.texture_op = alia::texture_operation::alpha_mask};
        auto text_pipeline = alia::pipeline::create<alia::full_vertex>(
            device, {.effect = &text_fx});

        alia::event_queue events;
        events.register_source(&win.get_event_source());
        float rotation_degrees = 0.0f;
        alia::vec3f position{0.0f, 0.0f, -5.0f};
        alia::vec3f forward{0.0f, 0.0f, 1.0f};
        double last_time = alia::get_time();

        bool running = true;
        while (running) {
            win.poll();
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>()) {
                    running = false;
                } else if (const auto *resize = event.get_if<alia::window_resize_event>()) {
                    swapchain.on_resize(resize->new_size);
                } else if (const auto *key = event.get_if<alia::window_key_down_event>()) {
                    if (key->key == alia::key::escape)
                        running = false;
                }
            }

            const double now = alia::get_time();
            const float dt = static_cast<float>(now - last_time);
            last_time = now;
            const alia::keyboard_state keyboard = alia::get_keyboard_state();
            if (keyboard[alia::key::left])
                rotation_degrees += dt * 100.0f;
            if (keyboard[alia::key::right])
                rotation_degrees -= dt * 100.0f;
            if (keyboard[alia::key::up])
                position += forward * (dt * 5.0f);
            if (keyboard[alia::key::down])
                position -= forward * (dt * 5.0f);

            const float rotation_radians =
                rotation_degrees * std::numbers::pi_v<float> / 180.0f;
            forward = {std::sin(rotation_radians), 0.0f, std::cos(rotation_radians)};

            auto frame = swapchain.begin_frame();
            frame.clear(alia::color::from_rgba8(100, 190, 240));
            const float aspect = frame.target_size().y > 0
                ? static_cast<float>(frame.target_size().x) / frame.target_size().y
                : 1.0f;
            scene_fx.projection =
                device.perspective_fov(78.0f, aspect, 0.01f, 100.0f);
            scene_fx.world = alia::transform::look_at(
                position, position + forward, {0.0f, 1.0f, 0.0f});
            frame.set_pipeline(scene_pipeline);
            frame.set_texture(0, background, alia::linear_clamp);
            frame.draw<alia::uv_vertex3d>(triangle);

            // Each pipeline owns its transform state; no global reset is needed.
            text_fx.projection = device.ortho_ui(frame.target_size());
            frame.set_pipeline(text_pipeline);
            alia::draw_text(
                frame,
                {10.0f, 10.0f},
                glyphs,
                std::format(
                    "player pos = ({:.2f}, {:.2f}, {:.2f}), facing {:.2f} deg",
                    position.x,
                    position.y,
                    position.z,
                    rotation_degrees));
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "3dcam example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
