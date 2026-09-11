#include "alia/core/get_time.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/cube_texture.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/primitive_renderer.hpp"
#include "alia/gfx/shader.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/os/window.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string_view>

namespace {

constexpr std::string_view d3d9_vertex_shader = R"(
struct vs_in {
    float3 position : POSITION0;
};

struct vs_out {
    float4 position : POSITION0;
    float3 direction : TEXCOORD0;
};

float4x4 u_view_proj : register(c0);

vs_out main(vs_in input) {
    vs_out output;
    float4 clip = mul(float4(input.position, 1.0), u_view_proj);
    output.position = clip.xyww;
    output.direction = input.position;
    return output;
}
)";

constexpr std::string_view d3d9_pixel_shader = R"(
struct ps_in {
    float3 direction : TEXCOORD0;
};

samplerCUBE u_sky : register(s0);

float4 main(ps_in input) : COLOR0 {
    return texCUBE(u_sky, normalize(input.direction));
}
)";

constexpr std::string_view ogl_vertex_shader = R"(
#version 120

attribute vec3 a_position;
uniform mat4 u_view_proj;
varying vec3 v_direction;

void main() {
    vec4 clip = u_view_proj * vec4(a_position, 1.0);
    gl_Position = clip.xyww;
    v_direction = a_position;
}
)";

constexpr std::string_view ogl_pixel_shader = R"(
#version 120

uniform samplerCube u_sky;
varying vec3 v_direction;

void main() {
    gl_FragColor = textureCube(u_sky, normalize(v_direction));
}
)";

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
    return length > 0.0f ? value / length : alia::vec3f{};
}

alia::transform look_at_rotation(alia::vec3f direction) {
    const alia::vec3f forward = normalized(direction);
    const alia::vec3f side = normalized(cross(forward, {0.0f, 1.0f, 0.0f}));
    const alia::vec3f up = cross(side, forward);
    alia::transform result = alia::transform::identity();
    result.m[0][0] = side.x;
    result.m[0][1] = up.x;
    result.m[0][2] = -forward.x;
    result.m[1][0] = side.y;
    result.m[1][1] = up.y;
    result.m[1][2] = -forward.y;
    result.m[2][0] = side.z;
    result.m[2][1] = up.z;
    result.m[2][2] = -forward.z;
    return result;
}

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

alia::bitmap make_face(alia::px_rgba8888 base) {
    constexpr int edge = 256;
    alia::bitmap result({edge, edge}, base);
    auto pixels = result.view_as<alia::px_rgba8888>();
    for (int y = 0; y < edge; ++y) {
        const float shade = 0.42f + 0.58f * static_cast<float>(y) / (edge - 1);
        for (int x = 0; x < edge; ++x) {
            const bool border = x < 4 || y < 4 || x >= edge - 4 || y >= edge - 4;
            const bool marker = x >= 38 && x < 82 && y >= 38 && y < 82;
            if (border || marker) {
                pixels[x, y] = {255, 255, 255, 255};
            } else {
                pixels[x, y] = {
                    static_cast<std::uint8_t>(base.r * shade),
                    static_cast<std::uint8_t>(base.g * shade),
                    static_cast<std::uint8_t>(base.b * shade),
                    255,
                };
            }
        }
    }
    return result;
}

std::array<alia::bitmap, alia::cube_face_count> make_sky_faces() {
    return {
        make_face({235, 55, 45, 255}),
        make_face({45, 215, 75, 255}),
        make_face({55, 95, 240, 255}),
        make_face({235, 205, 45, 255}),
        make_face({220, 55, 220, 255}),
        make_face({40, 210, 220, 255}),
    };
}

std::array<alia::bitmap, alia::cube_face_count> make_blank_faces() {
    constexpr alia::px_bgra8888 blank{8, 8, 8, 255};
    return {
        alia::bitmap({128, 128}, blank), alia::bitmap({128, 128}, blank),
        alia::bitmap({128, 128}, blank), alia::bitmap({128, 128}, blank),
        alia::bitmap({128, 128}, blank), alia::bitmap({128, 128}, blank),
    };
}

void stamp_positive_z(alia::cube_texture &texture) {
    const alia::rect_i stamp = alia::rect_i::pos_size({96, 96}, {64, 64});
    if (texture.format() == alia::pixel_format::rgba8888) {
        if (auto region = texture.lock<alia::px_rgba8888>(
                alia::cube_face::positive_z, stamp)) {
            auto &pixels = *region;
            for (int y = 0; y < pixels.height(); ++y)
                for (int x = 0; x < pixels.width(); ++x)
                    pixels[x, y] = {255, 245, 40, 255};
        }
    } else if (texture.format() == alia::pixel_format::bgra8888) {
        if (auto region = texture.lock<alia::px_bgra8888>(
                alia::cube_face::positive_z, stamp)) {
            auto &pixels = *region;
            for (int y = 0; y < pixels.height(); ++y)
                for (int x = 0; x < pixels.width(); ++x)
                    pixels[x, y] = {40, 245, 255, 255};
        }
    }
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
        alia::window window(
            {960, 600}, {.title = "ALIA cube texture skybox", .resizable = true});
        auto device = alia::gfx_device::create(requested_backend(argc, argv));
        alia::make_current(device);
        auto swapchain = device.create_swapchain({.target = window});

        if (!device.caps().cube_textures)
            throw std::runtime_error("selected device does not support cube textures");
        if (!device.caps().render_to_texture)
            throw std::runtime_error("selected device does not support render-to-texture");

        auto face_bitmaps = make_sky_faces();
        alia::cube_texture sky(
            device,
            std::span<const alia::bitmap, alia::cube_face_count>(face_bitmaps));
        auto blank_faces = make_blank_faces();
        alia::cube_texture rendered(
            device,
            std::span<const alia::bitmap, alia::cube_face_count>(blank_faces), 1,
            alia::texture_usage::render_target);
        alia::cube_texture copied(
            device,
            std::span<const alia::bitmap, alia::cube_face_count>(blank_faces));

        const std::array<alia::shader_source, 4> sources{{
            {alia::gfx_backend::d3d9, alia::shader_type::vertex,
             d3d9_vertex_shader, "main", {}, "skybox_vs_hlsl"},
            {alia::gfx_backend::d3d9, alia::shader_type::pixel,
             d3d9_pixel_shader, "main", {}, "skybox_ps_hlsl"},
            {alia::gfx_backend::opengl, alia::shader_type::vertex,
             ogl_vertex_shader, "main", {}, "skybox_vs_glsl"},
            {alia::gfx_backend::opengl, alia::shader_type::pixel,
             ogl_pixel_shader, "main", {}, "skybox_ps_glsl"},
        }};
        const std::array<alia::shader_constant_binding, 1> constants{{
            {"u_view_proj", alia::shader_type::vertex, 0, 4},
        }};
        const std::array<alia::shader_sampler_binding, 1> samplers{{
            {"u_sky", alia::shader_type::pixel, 0},
        }};
        alia::shader_program sky_shader(
            device,
            {.sources = sources,
             .constant_bindings = constants,
             .sampler_bindings = samplers});
        auto view_projection = sky_shader.allocate_constant<alia::transform>(
            "u_view_proj", alia::shader_type::vertex);
        auto sky_sampler = sky_shader.allocate_sampler("u_sky");
        auto sky_pipeline = alia::pipeline::create<alia::vertex3d>(
            device, {.effect = &sky_shader, .blend = {.enabled = false}});

        const std::array cube_vertices{
            alia::vertex3d{{-1.0f, -1.0f, -1.0f}},
            alia::vertex3d{{ 1.0f, -1.0f, -1.0f}},
            alia::vertex3d{{ 1.0f,  1.0f, -1.0f}},
            alia::vertex3d{{-1.0f,  1.0f, -1.0f}},
            alia::vertex3d{{-1.0f, -1.0f,  1.0f}},
            alia::vertex3d{{ 1.0f, -1.0f,  1.0f}},
            alia::vertex3d{{ 1.0f,  1.0f,  1.0f}},
            alia::vertex3d{{-1.0f,  1.0f,  1.0f}},
        };
        const std::array<std::uint32_t, 36> cube_indices{{
            0, 1, 2, 0, 2, 3, 5, 4, 7, 5, 7, 6,
            4, 0, 3, 4, 3, 7, 1, 5, 6, 1, 6, 2,
            3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
        }};

        alia::basic_effect primitive_effect;
        auto primitive_pipeline = alia::pipeline::create<alia::colored_vertex>(
            device, {.effect = &primitive_effect, .blend = {.enabled = false}});
        alia::primitive_renderer primitives;

        alia::ttf_font font = alia::load_ttf_font("./resources/roboto.ttf", 17);
        alia::hardware_glyph_buffer glyphs(device, font);
        alia::basic_effect text_effect{
            .texture_op = alia::texture_operation::alpha_mask};
        auto text_pipeline = alia::pipeline::create<alia::full_vertex>(
            device, {.effect = &text_effect});

        alia::event_queue events;
        events.register_source(&window.get_event_source());
        std::array<bool, static_cast<std::size_t>(alia::key::key_count)> keys{};
        alia::vec2i last_mouse{};
        bool have_mouse = false;
        float yaw = 0.0f;
        float pitch = 0.0f;
        int active_texture = 0;
        double last_time = alia::get_time();

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
                    keys[key_index(key->key)] = true;
                    if (key->key == alia::key::escape)
                        running = false;
                    else if (key->key == alia::key::space)
                        active_texture = (active_texture + 1) % 3;
                    else if (key->key == alia::key::L)
                        stamp_positive_z(sky);
                } else if (const auto *key = event.get_if<alia::window_key_up_event>()) {
                    keys[key_index(key->key)] = false;
                } else if (const auto *mouse = event.get_if<alia::window_mouse_move_event>()) {
                    if (have_mouse) {
                        yaw += static_cast<float>(mouse->position.x - last_mouse.x) * 0.004f;
                        pitch -= static_cast<float>(mouse->position.y - last_mouse.y) * 0.004f;
                    }
                    last_mouse = mouse->position;
                    have_mouse = true;
                }
            }

            const double now = alia::get_time();
            const float dt = static_cast<float>(now - last_time);
            last_time = now;
            if (keys[key_index(alia::key::left)])
                yaw -= 1.25f * dt;
            if (keys[key_index(alia::key::right)])
                yaw += 1.25f * dt;
            if (keys[key_index(alia::key::up)])
                pitch += 1.25f * dt;
            if (keys[key_index(alia::key::down)])
                pitch -= 1.25f * dt;
            pitch = std::clamp(pitch, -1.45f, 1.45f);

            auto frame = swapchain.begin_frame();

            // Replace a shader-owned binding left by the previous frame before
            // selecting a cube face as a render target.
            frame.set_texture(0, sky);
            const int animated_face = static_cast<int>(now) % alia::cube_face_count;
            const auto face = static_cast<alia::cube_face>(animated_face);
            frame.set_target(rendered, face);
            const float phase = static_cast<float>(std::fmod(now, 1.0));
            frame.clear(alia::color(0.025f, 0.035f, 0.06f, 1.0f));
            primitive_effect.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(primitive_pipeline);
            primitives.fill_rect(
                frame,
                alia::rect_f::pos_size({12.0f + phase * 68.0f, 20.0f}, {38.0f, 88.0f}),
                alia::color(0.95f, 0.26f, 0.12f, 1.0f));
            primitives.draw_line(
                frame, {8.0f, 112.0f}, {120.0f, 16.0f},
                alia::color(0.2f, 0.85f, 1.0f, 1.0f), 6.0f);
            primitives.flush(frame);
            frame.copy_to_texture(
                copied, face, alia::rect_i::pos_size({}, {128, 128}));

            frame.set_target();
            frame.clear(alia::color(0.015f, 0.02f, 0.035f, 1.0f));
            const auto target_size = frame.target_size();
            const float aspect = target_size.y > 0
                ? static_cast<float>(target_size.x) / target_size.y : 1.0f;
            const alia::vec3f direction{
                std::cos(pitch) * std::sin(yaw),
                std::sin(pitch),
                std::cos(pitch) * std::cos(yaw),
            };
            const auto projection = perspective_fov(
                78.0f, aspect, 0.05f, 10.0f,
                device.backend()->id == alia::gfx_backend::d3d9);
            view_projection.set_value(look_at_rotation(direction) * projection);
            if (active_texture == 0)
                sky_sampler.set_texture(sky);
            else if (active_texture == 1)
                sky_sampler.set_texture(rendered);
            else
                sky_sampler.set_texture(copied);
            frame.set_pipeline(sky_pipeline);
            frame.draw_indexed<alia::vertex3d>(cube_vertices, cube_indices);

            static constexpr std::array<std::string_view, 3> labels{
                "uploaded sky", "rendered faces", "copied faces"};
            text_effect.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(text_pipeline);
            alia::draw_text(
                frame, {14.0f, 14.0f}, glyphs,
                std::format(
                    "{} | animated face {} | Space: source  L: stamp +Z  arrows/mouse: look",
                    labels[static_cast<std::size_t>(active_texture)], animated_face),
                alia::white);
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "skybox example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
