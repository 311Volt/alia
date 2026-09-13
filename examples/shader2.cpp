#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/shader.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/io/mouse.hpp"
#include "alia/os/window.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

struct position_vertex {
    alia::vec2f position;

    static constexpr auto elements() {
        return std::array<alia::vertex_element, 1>{{
            {alia::vertex_attr::position, alia::vertex_storage::float_2, offsetof(position_vertex, position)},
        }};
    }
};

constexpr std::string_view d3d9_vertex_shader = R"(
struct vs_in {
    float2 position : POSITION0;
};

struct vs_out {
    float4 position : POSITION0;
    float2 pixel : TEXCOORD0;
};

float4x4 u_projection : register(c0);

vs_out main(vs_in input) {
    vs_out output;
    output.position = mul(float4(input.position, 0.0, 1.0), u_projection);
    output.pixel = input.position;
    return output;
}
)";

constexpr std::string_view d3d9_pixel_shader = R"(
struct ps_in {
    float2 pixel : TEXCOORD0;
};

sampler2D blue_noise : register(s0);
float2 mouse_pos : register(c0);
float2 dither_offset : register(c1);

float4 main(ps_in input) : COLOR0 {
    float dist = distance(input.pixel, mouse_pos);
    float noise = tex2D(blue_noise, frac(dither_offset + input.pixel / 64.0)).r;
    float glow = exp(-dist / 100.0) + noise / 128.0;
    return float4(0.0, glow, 0.0, 1.0);
}
)";

constexpr std::string_view ogl_vertex_shader = R"(
#version 120

attribute vec2 a_position;
uniform mat4 u_projection;
varying vec2 v_pixel;

void main() {
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    v_pixel = a_position;
}
)";

constexpr std::string_view ogl_pixel_shader = R"(
#version 120

uniform sampler2D blue_noise;
uniform vec2 mouse_pos;
uniform vec2 dither_offset;
varying vec2 v_pixel;

void main() {
    float dist = distance(v_pixel, mouse_pos);
    float noise = texture2D(
        blue_noise, fract(dither_offset + v_pixel / 64.0)).r;
    float glow = exp(-dist / 100.0) + noise / 128.0;
    gl_FragColor = vec4(0.0, glow, 0.0, 1.0);
}
)";

std::array<position_vertex, 6> fullscreen_quad(alia::vec2i size) {
    const float right = static_cast<float>(size.x);
    const float bottom = static_cast<float>(size.y);
    return {{
        {{0.0f, 0.0f}}, {{right, 0.0f}}, {{0.0f, bottom}},
        {{right, 0.0f}}, {{right, bottom}}, {{0.0f, bottom}},
    }};
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
        std::srand(1);
        alia::window win(
            {800, 600},
            {.title = "ALIA cursor glow shader", .resizable = true});
        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        auto swapchain = device.create_swapchain({.target = win});

        alia::texture noise(device, alia::load_image("./resources/bluenoise.png"));
        noise.set_sampler(alia::nearest_wrap);

        const std::array<alia::shader_source, 4> sources{{
            {
                .backend = alia::gfx_backend::d3d9,
                .type = alia::shader_type::vertex,
                .source = d3d9_vertex_shader,
                .debug_name = "cursor_glow_vs_hlsl",
            },
            {
                .backend = alia::gfx_backend::d3d9,
                .type = alia::shader_type::pixel,
                .source = d3d9_pixel_shader,
                .debug_name = "cursor_glow_ps_hlsl",
            },
            {
                .backend = alia::gfx_backend::opengl,
                .type = alia::shader_type::vertex,
                .source = ogl_vertex_shader,
                .debug_name = "cursor_glow_vs_glsl",
            },
            {
                .backend = alia::gfx_backend::opengl,
                .type = alia::shader_type::pixel,
                .source = ogl_pixel_shader,
                .debug_name = "cursor_glow_ps_glsl",
            },
        }};
        // Float constants are deliberate: D3D9 integer constants use i#
        // registers, which HLSL c# declarations do not read.
        const std::array<alia::shader_constant_binding, 3> constants{{
            {"u_projection", alia::shader_type::vertex, 0, 4},
            {"mouse_pos", alia::shader_type::pixel, 0, 1},
            {"dither_offset", alia::shader_type::pixel, 1, 1},
        }};
        const std::array<alia::shader_sampler_binding, 1> samplers{{
            {"blue_noise", alia::shader_type::pixel, 0},
        }};
        alia::shader_program shader(
            device,
            {
                .sources = sources,
                .constant_bindings = constants,
                .sampler_bindings = samplers,
            });
        auto projection_constant = shader.allocate_constant<alia::transform>(
            "u_projection", alia::shader_type::vertex);
        auto mouse_constant = shader.allocate_constant<alia::vec2f>(
            "mouse_pos", alia::shader_type::pixel);
        auto dither_constant = shader.allocate_constant<alia::vec2f>(
            "dither_offset", alia::shader_type::pixel);
        auto shader_pipeline = alia::pipeline::create<position_vertex>(
            device, {.effect = &shader});

        alia::event_queue events;
        events.register_source(&win.get_event_source());
        alia::vec2f mouse_position{400.0f, 300.0f};

        bool running = true;
        while (running) {
            win.poll();
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>())
                    running = false;
                else if (const auto *resize = event.get_if<alia::window_resize_event>())
                    swapchain.on_resize(resize->new_size);
                else if (const auto *key = event.get_if<alia::window_key_down_event>();
                         key && key->key == alia::key::escape)
                    running = false;
                else if (const auto *mouse = event.get_if<alia::mouse_axes_event>())
                    mouse_position = alia::vec2f(mouse->position);
            }

            auto frame = swapchain.begin_frame();
            frame.clear(alia::black);
            projection_constant.set_value(device.ortho_ui(frame.target_size()));
            mouse_constant.set_value(mouse_position);
            dither_constant.set_value({
                static_cast<float>(std::rand() % 64) / 64.0f,
                static_cast<float>(std::rand() % 64) / 64.0f,
            });
            frame.set_pipeline(shader_pipeline);
            frame.set_texture(0, noise, alia::nearest_wrap);
            const auto quad = fullscreen_quad(frame.target_size());
            frame.draw<position_vertex>(quad);
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "shader2 example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
