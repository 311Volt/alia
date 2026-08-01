#include "alia/os/window.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/prim_buffers.hpp"
#include "alia/gfx/shader.hpp"
#include "alia/gfx/transform.hpp"
#include "alia/events/event_queue.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr float pi = 3.14159265358979323846f;

struct mesh_data {
    std::vector<alia::normal_vertex3d> vertices;
    std::vector<uint32_t> indices;
};

mesh_data make_sphere_mesh(int stacks, int slices) {
    mesh_data mesh;
    mesh.vertices.reserve(static_cast<std::size_t>(stacks + 1) * static_cast<std::size_t>(slices + 1));
    mesh.indices.reserve(static_cast<std::size_t>(stacks) * static_cast<std::size_t>(slices) * 6);

    for (int stack = 0; stack <= stacks; ++stack) {
        const float v = static_cast<float>(stack) / static_cast<float>(stacks);
        const float phi = -0.5f * pi + v * pi;
        const float y = std::sin(phi);
        const float ring = std::cos(phi);

        for (int slice = 0; slice <= slices; ++slice) {
            const float u = static_cast<float>(slice) / static_cast<float>(slices);
            const float theta = u * 2.0f * pi;
            const alia::vec3f p{std::cos(theta) * ring, y, std::sin(theta) * ring};
            mesh.vertices.push_back({p, p.normalized()});
        }
    }

    const auto vertex_at = [slices](int stack, int slice) -> uint32_t {
        return static_cast<uint32_t>(stack * (slices + 1) + slice);
    };

    for (int stack = 0; stack < stacks; ++stack) {
        for (int slice = 0; slice < slices; ++slice) {
            const uint32_t a = vertex_at(stack, slice);
            const uint32_t b = vertex_at(stack + 1, slice);
            const uint32_t c = vertex_at(stack, slice + 1);
            const uint32_t d = vertex_at(stack + 1, slice + 1);

            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);

            mesh.indices.push_back(c);
            mesh.indices.push_back(b);
            mesh.indices.push_back(d);
        }
    }

    return mesh;
}

alia::transform rotate_x(float angle) {
    alia::transform r = alia::transform::identity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    r.m[1][1] = c;
    r.m[1][2] = s;
    r.m[2][1] = -s;
    r.m[2][2] = c;
    return r;
}

alia::transform rotate_y(float angle) {
    alia::transform r = alia::transform::identity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    r.m[0][0] = c;
    r.m[0][2] = -s;
    r.m[2][0] = s;
    r.m[2][2] = c;
    return r;
}

alia::transform scale3d(float s) {
    alia::transform r = alia::transform::identity();
    r.m[0][0] = s;
    r.m[1][1] = s;
    r.m[2][2] = s;
    return r;
}

alia::transform make_projection(alia::vec2i size) {
    const float aspect = size.y > 0 ? static_cast<float>(size.x) / static_cast<float>(size.y) : 1.0f;
    return alia::transform::ortho(-1.55f * aspect, 1.55f * aspect, -1.55f, 1.55f);
}

std::string make_title(bool buffered, int fps, std::size_t vertex_count, std::size_t index_count) {
    return "ALIA buffered mesh - " +
        std::string(buffered ? "buffered" : "immediate") +
        " | FPS: " + std::to_string(fps) +
        " | vertices: " + std::to_string(vertex_count) +
        " | indices: " + std::to_string(index_count);
}

constexpr std::string_view d3d9_vertex_shader = R"(
struct vs_in {
    float3 position : POSITION0;
    float3 normal : NORMAL0;
};

struct vs_out {
    float4 position : POSITION0;
    float3 normal : TEXCOORD0;
};

float4x4 u_projection : register(c0);
float4x4 u_transform : register(c4);

vs_out main(vs_in input) {
    vs_out output;
    float4 world_position = mul(float4(input.position, 1.0), u_transform);
    output.position = mul(world_position, u_projection);
    output.normal = normalize(mul(float4(input.normal, 0.0), u_transform).xyz);
    return output;
}
)";

constexpr std::string_view d3d9_pixel_shader = R"(
struct ps_in {
    float3 normal : TEXCOORD0;
};

float4 main(ps_in input) : COLOR0 {
    float3 n = normalize(input.normal);
    float3 light_dir = normalize(float3(-0.35, 0.75, -0.55));
    float shade = 0.18 + 0.82 * saturate(dot(n, light_dir));
    return float4(0.16 * shade, 0.58 * shade, 0.92 * shade, 1.0);
}
)";

constexpr std::string_view ogl_vertex_shader = R"(
#version 120

attribute vec3 a_position;
attribute vec3 a_normal;

uniform mat4 u_projection;
uniform mat4 u_transform;

varying vec3 v_normal;

void main() {
    vec4 world_position = u_transform * vec4(a_position, 1.0);
    gl_Position = u_projection * world_position;
    v_normal = normalize((u_transform * vec4(a_normal, 0.0)).xyz);
}
)";

constexpr std::string_view ogl_pixel_shader = R"(
#version 120

varying vec3 v_normal;

void main() {
    vec3 n = normalize(v_normal);
    vec3 light_dir = normalize(vec3(-0.35, 0.75, -0.55));
    float shade = 0.18 + 0.82 * max(dot(n, light_dir), 0.0);
    gl_FragColor = vec4(0.16 * shade, 0.58 * shade, 0.92 * shade, 1.0);
}
)";

} // namespace

namespace {
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
        alia::window win(
            {1000, 700},
            {
                .title = "ALIA buffered mesh",
                .resizable = true,
            }
        );

        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        auto swapchain = device.create_swapchain({.target = win});

        mesh_data mesh = make_sphere_mesh(160, 320);
        std::span<const alia::normal_vertex3d> vertex_span(mesh.vertices.data(), mesh.vertices.size());
        std::span<const uint32_t> index_span(mesh.indices.data(), mesh.indices.size());

        alia::vertex_buffer<alia::normal_vertex3d> gpu_vertices(device, vertex_span, alia::buffer_usage::static_mesh);
        alia::index_buffer gpu_indices(device, index_span, alia::buffer_usage::static_mesh);

        const std::array<alia::shader_source, 4> sources{{
            {
                .backend = alia::gfx_backend::d3d9,
                .type = alia::shader_type::vertex,
                .source = d3d9_vertex_shader,
                .debug_name = "buffered_mesh_vs_hlsl",
            },
            {
                .backend = alia::gfx_backend::d3d9,
                .type = alia::shader_type::pixel,
                .source = d3d9_pixel_shader,
                .debug_name = "buffered_mesh_ps_hlsl",
            },
            {
                .backend = alia::gfx_backend::opengl,
                .type = alia::shader_type::vertex,
                .source = ogl_vertex_shader,
                .debug_name = "buffered_mesh_vs_glsl",
            },
            {
                .backend = alia::gfx_backend::opengl,
                .type = alia::shader_type::pixel,
                .source = ogl_pixel_shader,
                .debug_name = "buffered_mesh_ps_glsl",
            },
        }};

        const std::array<alia::shader_constant_binding, 2> constants{{
            {"u_projection", alia::shader_type::vertex, 0, 4},
            {"u_transform", alia::shader_type::vertex, 4, 4},
        }};

        alia::shader_program shader(
            device,
            {
                .sources = sources,
                .constant_bindings = constants,
            }
        );
        auto projection_constant =
            shader.allocate_constant<alia::transform>("u_projection", alia::shader_type::vertex);
        auto transform_constant =
            shader.allocate_constant<alia::transform>("u_transform", alia::shader_type::vertex);
        auto pipeline = alia::pipeline::create<alia::normal_vertex3d>(device, {
            .effect = &shader,
            .depth = {.test_enabled = true, .write_enabled = true},
        });

        alia::event_queue events;
        events.register_source(&win.get_event_source());

        bool running = true;
        bool use_buffered = true;
        int displayed_fps = 0;
        int fps_frames = 0;
        auto fps_window_start = std::chrono::steady_clock::now();
        const auto start = std::chrono::steady_clock::now();
        std::string title;
        auto update_title = [&]() {
            title = make_title(use_buffered, displayed_fps, mesh.vertices.size(), mesh.indices.size());
            win.set_title(title.c_str());
        };

        update_title();

        while (running) {
            win.poll();
            ++fps_frames;

            while (!events.empty()) {
                auto ev = events.pop();
                if (ev.get_if<alia::window_close_event>()) {
                    running = false;
                } else if (auto *e = ev.get_if<alia::window_resize_event>()) {
                    swapchain.on_resize(e->new_size);
                } else if (auto *e = ev.get_if<alia::window_key_down_event>()) {
                    if (e->key == alia::key::escape) {
                        running = false;
                    } else if (e->key == alia::key::space) {
                        use_buffered = !use_buffered;
                        update_title();
                    }
                }
            }

            const auto now = std::chrono::steady_clock::now();
            const float t = std::chrono::duration<float>(now - start).count();
            const float fps_elapsed = std::chrono::duration<float>(now - fps_window_start).count();
            if (fps_elapsed >= 1.0f) {
                displayed_fps = static_cast<int>(static_cast<float>(fps_frames) / fps_elapsed + 0.5f);
                fps_frames = 0;
                fps_window_start = now;
                update_title();
            }

            const alia::transform projection = make_projection(win.size());
            const alia::transform model =
                scale3d(0.95f) *
                rotate_x(-0.35f) *
                rotate_y(t * 0.75f);

            projection_constant.set_value(projection);
            transform_constant.set_value(model);

            auto frame = swapchain.begin_frame();
            frame.clear(alia::color(0.025f, 0.03f, 0.04f, 1.0f), 1.0f);
            frame.set_pipeline(pipeline);
            if (use_buffered)
                frame.draw_indexed(gpu_vertices, gpu_indices);
            else
                frame.draw_indexed(vertex_span, index_span);
            frame.present();
        }
    } catch (const std::exception &e) {
        std::cerr << "buffered mesh example failed: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
