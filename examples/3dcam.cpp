#include "alia/core/get_time.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/os/window.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <format>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string_view>

namespace {

// NOTE (API feedback): vec3 currently has no dot or cross operations.
float dot(alia::vec3f a, alia::vec3f b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

alia::vec3f cross(alia::vec3f a, alia::vec3f b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

alia::vec3f normalized(alia::vec3f value) {
    const float length = std::sqrt(dot(value, value));
    return length == 0.0f ? alia::vec3f{} : value / length;
}

// HYPOTHETICAL alia API: transform::look_at for row-vector transforms.
alia::transform look_at(alia::vec3f eye, alia::vec3f target, alia::vec3f up) {
    const alia::vec3f f = normalized(target - eye);
    const alia::vec3f s = normalized(cross(f, up));
    const alia::vec3f u = cross(s, f);
    alia::transform result = alia::transform::identity();
    result.m[0][0] = s.x;
    result.m[0][1] = u.x;
    result.m[0][2] = -f.x;
    result.m[1][0] = s.y;
    result.m[1][1] = u.y;
    result.m[1][2] = -f.y;
    result.m[2][0] = s.z;
    result.m[2][1] = u.z;
    result.m[2][2] = -f.z;
    result.m[3][0] = -dot(s, eye);
    result.m[3][1] = -dot(u, eye);
    result.m[3][2] = dot(f, eye);
    return result;
}

// NOTE (API feedback): transform::perspective should choose the backend's
// clip-space depth convention just as transform::ortho_ui handles pixel centers.
alia::transform perspective_fov(
    float fov_degrees,
    float aspect,
    float near_plane,
    float far_plane,
    bool zero_to_one
) {
    const float radians = fov_degrees * std::numbers::pi_v<float> / 180.0f;
    const float y_scale = 1.0f / std::tan(radians * 0.5f);
    alia::transform result{};
    result.m[0][0] = y_scale / aspect;
    result.m[1][1] = y_scale;
    result.m[2][3] = -1.0f;
    if (zero_to_one) {
        result.m[2][2] = far_plane / (near_plane - far_plane);
        result.m[3][2] = near_plane * far_plane / (near_plane - far_plane);
    } else {
        result.m[2][2] = (far_plane + near_plane) / (near_plane - far_plane);
        result.m[3][2] = 2.0f * near_plane * far_plane / (near_plane - far_plane);
    }
    return result;
}

std::size_t key_index(alia::key value) {
    return static_cast<std::size_t>(value);
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
            {800, 600},
            {.title = "ALIA 3D camera", .resizable = true});
        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        alia::make_current(device);
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
        // NOTE (API feedback): alia has no is_key_down polling API.
        std::array<bool, static_cast<std::size_t>(alia::key::key_count)> keys{};
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
                    keys[key_index(key->key)] = true;
                    if (key->key == alia::key::escape)
                        running = false;
                } else if (const auto *key = event.get_if<alia::window_key_up_event>()) {
                    keys[key_index(key->key)] = false;
                }
            }

            const double now = alia::get_time();
            const float dt = static_cast<float>(now - last_time);
            last_time = now;
            if (keys[key_index(alia::key::left)])
                rotation_degrees += dt * 100.0f;
            if (keys[key_index(alia::key::right)])
                rotation_degrees -= dt * 100.0f;
            if (keys[key_index(alia::key::up)])
                position += forward * (dt * 5.0f);
            if (keys[key_index(alia::key::down)])
                position -= forward * (dt * 5.0f);

            const float rotation_radians =
                rotation_degrees * std::numbers::pi_v<float> / 180.0f;
            forward = {std::sin(rotation_radians), 0.0f, std::cos(rotation_radians)};

            auto frame = swapchain.begin_frame();
            frame.clear(alia::color::from_rgba8(100, 190, 240));
            const float aspect = frame.target_size().y > 0
                ? static_cast<float>(frame.target_size().x) / frame.target_size().y
                : 1.0f;
            const bool zero_to_one = device.backend()->id == alia::gfx_backend::d3d9;
            scene_fx.projection = perspective_fov(
                78.0f, aspect, 0.01f, 100.0f, zero_to_one);
            scene_fx.world = look_at(position, position + forward, {0.0f, 1.0f, 0.0f});
            frame.set_pipeline(scene_pipeline);
            frame.set_texture(0, background, alia::linear_clamp);
            frame.draw<alia::uv_vertex3d>(triangle);

            // Each pipeline owns its transform state; no global reset is needed.
            text_fx.projection = alia::transform::ortho_ui(frame.target_size());
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
