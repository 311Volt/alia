#include "alia/core/timing.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/lighting.hpp"
#include "alia/gfx/primitive_renderer.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/os/window.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
constexpr float pi = std::numbers::pi_v<float>;

alia::gfx_backend requested_backend(int argc, char **argv) {
    if (argc < 2) return alia::gfx_backend::auto_;
    const std::string_view value(argv[1]);
    if (value == "d3d9") return alia::gfx_backend::d3d9;
    if (value == "opengl") return alia::gfx_backend::opengl;
    throw std::invalid_argument("backend must be d3d9 or opengl");
}

struct mesh_data {
    std::vector<alia::full_vertex3d> vertices;
    std::vector<std::uint32_t> indices;
};

mesh_data make_sphere_mesh(int stacks, int slices) {
    mesh_data mesh;
    mesh.vertices.reserve(static_cast<std::size_t>(stacks + 1) * (slices + 1));
    mesh.indices.reserve(static_cast<std::size_t>(stacks) * slices * 6);
    for (int stack = 0; stack <= stacks; ++stack) {
        const float v = static_cast<float>(stack) / stacks;
        const float phi = -0.5f * pi + v * pi;
        const float y = std::sin(phi);
        const float ring = std::cos(phi);
        for (int slice = 0; slice <= slices; ++slice) {
            const float u = static_cast<float>(slice) / slices;
            const float theta = u * 2.0f * pi;
            const alia::vec3f position{std::cos(theta) * ring, y, std::sin(theta) * ring};
            const alia::color vertex_color{
                0.3f + 0.7f * (0.5f + 0.5f * position.x),
                0.3f + 0.7f * (0.5f + 0.5f * position.y),
                0.3f + 0.7f * (0.5f + 0.5f * position.z), 1.0f};
            mesh.vertices.push_back({position, position.normalized(), vertex_color, {u, v}});
        }
    }
    const auto vertex_at = [slices](int stack, int slice) {
        return static_cast<std::uint32_t>(stack * (slices + 1) + slice);
    };
    for (int stack = 0; stack < stacks; ++stack) {
        for (int slice = 0; slice < slices; ++slice) {
            const auto a = vertex_at(stack, slice);
            const auto b = vertex_at(stack + 1, slice);
            const auto c = vertex_at(stack, slice + 1);
            const auto d = vertex_at(stack + 1, slice + 1);
            mesh.indices.insert(mesh.indices.end(), {a, b, c, c, b, d});
        }
    }
    return mesh;
}

alia::transform rotation_y(float angle) {
    auto result = alia::transform::identity();
    const float c = std::cos(angle), s = std::sin(angle);
    result.m[0][0] = c;
    result.m[0][2] = -s;
    result.m[2][0] = s;
    result.m[2][2] = c;
    return result;
}

alia::transform translation(alia::vec3f position) {
    auto result = alia::transform::identity();
    result.m[3][0] = position.x;
    result.m[3][1] = position.y;
    result.m[3][2] = position.z;
    return result;
}

const char *on_off(bool value) { return value ? "on" : "off"; }

const char *fog_name(alia::fog_mode mode) {
    switch (mode) {
    case alia::fog_mode::none: return "none";
    case alia::fog_mode::linear: return "linear";
    case alia::fog_mode::exp: return "exp";
    case alia::fog_mode::exp2: return "exp2";
    }
    return "unknown";
}
} // namespace

int main(int argc, char **argv) {
    try {
        alia::window window({1000, 720}, {.title = "ALIA fixed-function lighting and fog", .resizable = true});
        auto device = alia::gfx_device::create(requested_backend(argc, argv));
        if (device.caps().max_lights < 3)
            throw std::runtime_error("lighting example requires at least three fixed-function lights");
        alia::swapchain_config config{.target = window};
        config.framebuffer.depth_bits = {24, alia::require};
        auto swapchain = device.create_swapchain(config);

        const auto sphere = make_sphere_mesh(48, 80);
        const auto vertices = std::span<const alia::full_vertex3d>(sphere.vertices);
        const auto indices = std::span<const std::uint32_t>(sphere.indices);
        auto font = alia::load_ttf_font("./resources/roboto.ttf", 18);
        alia::hardware_glyph_buffer glyphs(device, font);
        alia::immediate_primitive_renderer overlay;
        alia::event_queue events;
        events.register_source(&window.get_event_source());

        bool sun_enabled = true, point_enabled = true, spot_enabled = true;
        bool vertex_colors = true, orbit_camera = false;
        constexpr std::array shininess_values{0.0f, 8.0f, 32.0f, 96.0f, 128.0f};
        constexpr std::array fog_modes{
            alia::fog_mode::none, alia::fog_mode::linear, alia::fog_mode::exp, alia::fog_mode::exp2};
        constexpr alia::color clear_color{0.025f, 0.03f, 0.045f, 1.0f};
        std::size_t shininess_index = 2;
        std::size_t fog_index = 0;
        float camera_angle = 0.0f;
        alia::frame_clock clock;
        bool running = true;
        while (running) {
            window.poll();
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>()) {
                    running = false;
                } else if (const auto *resize = event.get_if<alia::window_resize_event>()) {
                    swapchain.on_resize(resize->new_size);
                } else if (const auto *key = event.get_if<alia::window_key_down_event>()) {
                    switch (key->key) {
                    case alia::key::escape: running = false; break;
                    case alia::key::num1: sun_enabled = !sun_enabled; break;
                    case alia::key::num2: point_enabled = !point_enabled; break;
                    case alia::key::num3: spot_enabled = !spot_enabled; break;
                    case alia::key::C: vertex_colors = !vertex_colors; break;
                    case alia::key::S: shininess_index = (shininess_index + 1) % shininess_values.size(); break;
                    case alia::key::V: orbit_camera = !orbit_camera; break;
                    case alia::key::F:
                        if (device.caps().fog)
                            fog_index = (fog_index + 1) % fog_modes.size();
                        break;
                    default: break;
                    }
                }
            }
            const auto tick = clock.tick();
            const float time = static_cast<float>(tick.elapsed);
            if (orbit_camera)
                camera_angle += static_cast<float>(tick.delta) * 0.3f;

            const alia::vec3f point_position{3.0f * std::cos(time * 0.7f), 1.2f, 3.0f * std::sin(time * 0.7f)};
            const alia::vec3f spot_position{0.0f, 3.2f, -3.5f};
            std::vector<alia::light> lights;
            if (sun_enabled)
                lights.emplace_back(alia::directional_light{
                    .direction = {0.35f, -0.8f, 0.65f},
                    .diffuse = {0.65f, 0.6f, 0.5f, 1.0f}});
            if (point_enabled)
                lights.emplace_back(alia::point_light{
                    .position = point_position,
                    .attenuation = {1.0f, 0.08f, 0.025f},
                    .diffuse = {0.18f, 0.45f, 1.0f, 1.0f}});
            if (spot_enabled)
                lights.emplace_back(alia::spot_light{
                    .position = spot_position,
                    .direction = -1.0f * spot_position,
                    .inner_outer = {25.0f * pi / 180.0f, 60.0f * pi / 180.0f},
                    .cutoff_exponent = {30.0f * pi / 180.0f, 12.0f},
                    .attenuation = {1.0f, 0.04f, 0.015f},
                    .diffuse = {1.0f, 0.25f, 0.12f, 1.0f}});

            auto frame = swapchain.begin_frame();
            frame.clear(clear_color, 1.0f);
            // Keep lights selected for the overlay too: layouts without normals stay unlit.
            frame.set_lights(std::span<const alia::light>(lights));
            frame.set_ambient({0.06f, 0.06f, 0.08f, 1.0f});
            {
                auto scene = frame.save_state();
                const auto size = frame.target_size();
                const float aspect = size.y > 0 ? static_cast<float>(size.x) / size.y : 1.0f;
                frame.set_projection(device.perspective_fov(45.0f, aspect, 0.1f, 40.0f));
                frame.set_view(alia::transform::look_at(
                    {7.2f * std::sin(camera_angle), 1.0f, -7.2f * std::cos(camera_angle)},
                    {}, {0.0f, 1.0f, 0.0f}));
                frame.set_blend(alia::no_blend);
                frame.set_depth(alia::depth_test_write);
                frame.set_fog({
                    .mode = fog_modes[fog_index], .col = clear_color,
                    .start = 4.0f, .end = 11.0f, .density = 0.12f});

                alia::material surface{
                    .diffuse = {0.65f, 0.7f, 0.82f, 1.0f},
                    .ambient = {0.65f, 0.7f, 0.82f, 1.0f},
                    .specular = {0.7f, 0.7f, 0.7f, 1.0f},
                    .shininess = shininess_values[shininess_index],
                    .use_vertex_color = vertex_colors};
                frame.set_material(surface);
                {
                    auto object = frame.push_world(rotation_y(time * 0.22f) * translation({1.25f, 0.0f, 0.0f}));
                    frame.draw_indexed<alia::full_vertex3d>(vertices, indices);
                }
                // Uniform reference also exercises restoration after GL color material.
                surface.use_vertex_color = false;
                frame.set_material(surface);
                {
                    auto object = frame.push_world(rotation_y(time * 0.22f) * translation({-1.25f, 0.0f, 0.0f}));
                    frame.draw_indexed<alia::full_vertex3d>(vertices, indices);
                }
            }

            overlay.fill_rect(frame, alia::rect_f::pos_size({10.0f, 10.0f}, {860.0f, 142.0f}),
                alia::color{0.015f, 0.02f, 0.03f, 0.88f});
            const char *backend_name = device.backend()->id == alia::gfx_backend::d3d9 ? "D3D9" : "OpenGL";
            const char *spot_model = device.caps().spot_model == alia::spot_light_model::inner_outer
                ? "inner_outer" : "cutoff_exponent";
            const char *fog_distance = !device.caps().fog ? "unsupported"
                : device.caps().fog_distance == alia::fog_distance_model::radial ? "radial" : "view_depth";
            alia::draw_text({
                .target = frame,
                .glyphs = glyphs,
                .text = std::format(
                    "{} | spot model: {} | max lights: {}\n"
                    "1: sun {}   2: orbiting point {}   3: spot {}\n"
                    "C: vertex colors {}   S: shininess {:.0f}   V: orbit camera {}\n"
                    "F: fog {} | distance: {}\n"
                    "Left: selected material   Right: uniform reference | Overlay stays unlit and unfogged",
                    backend_name, spot_model, device.caps().max_lights,
                    on_off(sun_enabled), on_off(point_enabled), on_off(spot_enabled),
                    on_off(vertex_colors), shininess_values[shininess_index], on_off(orbit_camera),
                    device.caps().fog ? fog_name(fog_modes[fog_index]) : "unsupported", fog_distance),
                .texture_slot = 0,
                .position = {20.0f, 18.0f}});
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "lighting example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
