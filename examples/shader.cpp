#include "alia/os/window.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/shader.hpp"
#include "alia/gfx/transform.hpp"
#include "alia/gfx/primitives.hpp"
#include "alia/gfx/bitmap/bitmap.hpp"
#include "alia/gfx/bitmap/pixel_types.hpp"
#include "alia/events/event_queue.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <span>

namespace {

alia::bitmap make_checker_bitmap() {
    constexpr int size = 64;
    std::array<alia::px_rgba8888, size * size> pixels = {};
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool high = ((x / 8) + (y / 8)) % 2 == 0;
            pixels[y * size + x] = high
                ? alia::px_rgba8888{255, 245, 180, 255}
                : alia::px_rgba8888{35, 120, 210, 255};
        }
    }
    return alia::bitmap({size, size}, std::span<const alia::px_rgba8888>(pixels));
}

constexpr std::string_view d3d9_vertex_shader = R"(
struct vs_in {
    float2 position : POSITION0;
    float2 uv : TEXCOORD0;
};

struct vs_out {
    float4 position : POSITION0;
    float2 uv : TEXCOORD0;
};

float4x4 u_projection : register(c0);
float4x4 u_transform : register(c4);

vs_out main(vs_in input) {
    vs_out output;
    float4 position = float4(input.position, 0.0, 1.0);
    output.position = mul(mul(position, u_transform), u_projection);
    output.uv = input.uv;
    return output;
}
)";

constexpr std::string_view d3d9_pixel_shader = R"(
struct ps_in {
    float2 uv : TEXCOORD0;
};

sampler2D u_texture : register(s0);
float4 u_tint : register(c0);

float4 main(ps_in input) : COLOR0 {
    return tex2D(u_texture, input.uv) * u_tint;
}
)";

constexpr std::string_view ogl_vertex_shader = R"(
#version 120

attribute vec2 a_position;
attribute vec2 a_tex_coord;

uniform mat4 u_projection;
uniform mat4 u_transform;

varying vec2 v_uv;

void main() {
    gl_Position = u_projection * u_transform * vec4(a_position, 0.0, 1.0);
    v_uv = a_tex_coord;
}
)";

constexpr std::string_view ogl_pixel_shader = R"(
#version 120

uniform sampler2D u_texture;
uniform vec4 u_tint;

varying vec2 v_uv;

void main() {
    gl_FragColor = texture2D(u_texture, v_uv) * u_tint;
}
)";

} // namespace

int main() {
    try {
        alia::window win(
            {800, 600},
            {
                .title = "ALIA shader example",
                .resizable = true,
            }
        );

        alia::gfx_device device = alia::gfx_device::create();
        auto swapchain = device.create_swapchain({.target = win});
        alia::set_current_projection(alia::transform::ortho_ui(win.size()));

        alia::bitmap checker_bmp = make_checker_bitmap();
        alia::texture checker_tex(device, checker_bmp);

        const std::array<alia::shader_source, 4> sources{{
            {
                .backend = alia::gfx_backend::d3d9,
                .type = alia::shader_type::vertex,
                .source = d3d9_vertex_shader,
                .debug_name = "shader_example_vs_hlsl",
            },
            {
                .backend = alia::gfx_backend::d3d9,
                .type = alia::shader_type::pixel,
                .source = d3d9_pixel_shader,
                .debug_name = "shader_example_ps_hlsl",
            },
            {
                .backend = alia::gfx_backend::opengl,
                .type = alia::shader_type::vertex,
                .source = ogl_vertex_shader,
                .debug_name = "shader_example_vs_glsl",
            },
            {
                .backend = alia::gfx_backend::opengl,
                .type = alia::shader_type::pixel,
                .source = ogl_pixel_shader,
                .debug_name = "shader_example_ps_glsl",
            },
        }};

        const std::array<alia::shader_constant_binding, 3> constants{{
            {"u_projection", alia::shader_type::vertex, 0, 4},
            {"u_transform", alia::shader_type::vertex, 4, 4},
            {"u_tint", alia::shader_type::pixel, 0, 1},
        }};
        const std::array<alia::shader_sampler_binding, 1> samplers{{
            {"u_texture", alia::shader_type::pixel, 0},
        }};

        alia::shader_program shader(
            device,
            {
                .sources = sources,
                .constant_bindings = constants,
                .sampler_bindings = samplers,
            }
        );
        auto projection_constant =
            shader.allocate_constant<alia::transform>("u_projection", alia::shader_type::vertex);

        auto transform_constant =
            shader.allocate_constant<alia::transform>("u_transform", alia::shader_type::vertex);

        auto tint_constant =
            shader.allocate_constant<alia::color>("u_tint", alia::shader_type::pixel);

        auto texture_sampler =
            shader.allocate_sampler("u_texture", alia::shader_type::pixel);
            
        texture_sampler.set_texture(checker_tex);

        alia::event_queue events;
        events.register_source(&win.get_event_source());

        const auto start = std::chrono::steady_clock::now();
        bool running = true;
        while (running) {
            win.poll();
            while (!events.empty()) {
                auto ev = events.pop();
                if (ev.get_if<alia::window_close_event>()) {
                    running = false;
                } else if (auto *e = ev.get_if<alia::window_resize_event>()) {
                    alia::on_resize(e->new_size);
                    alia::set_current_projection(alia::transform::ortho_ui(e->new_size));
                } else if (auto *e = ev.get_if<alia::window_key_down_event>()) {
                    if (e->key == alia::key::escape)
                        running = false;
                }
            }

            const auto now = std::chrono::steady_clock::now();
            const float t = std::chrono::duration<float>(now - start).count();
            const float pulse = 0.5f + 0.5f * std::sin(t * 2.0f);

            const alia::transform model =
                alia::transform::scale({220.0f, 220.0f}) *
                alia::transform::rotate(t) *
                alia::transform::translate({400.0f, 300.0f});

            projection_constant.set_value(alia::get_current_projection());
            transform_constant.set_value(model);
            tint_constant.set_value(alia::color(1.0f, 0.65f + 0.35f * pulse, 0.8f, 1.0f));

            alia::clear(alia::color(0.06f, 0.07f, 0.09f, 1.0f));

            alia::use(shader);
            alia::draw_textured_rect(alia::rect_f::pos_size({-0.5f, -0.5f}, {1.0f, 1.0f}), checker_tex);
            alia::reset_shader();

            alia::draw_rect(alia::rect_f::pos_size({260.0f, 160.0f}, {280.0f, 280.0f}), alia::white, 2.0f);
            alia::fill_rect(alia::rect_f::pos_size({30.0f, 30.0f}, {80.0f, 80.0f}), alia::color(0.1f, 0.8f, 0.45f, 0.75f));

            alia::present();
        }
    } catch (const std::exception &e) {
        std::cerr << "shader example failed: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
