#include "alia/core/timing.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/prim_buffers.hpp"
#include "alia/gfx/shader.hpp"
#include "alia/os/window.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

struct cube_instance {
    alia::vec4f offset_scale;
    alia::color tint;

    static constexpr auto instance_elements() {
        return std::array<alia::instance_element, 2>{{
            {0, alia::vertex_storage::float_4, offsetof(cube_instance, offset_scale)},
            {1, alia::vertex_storage::float_4, offsetof(cube_instance, tint)},
        }};
    }
};

struct cube_mesh {
    std::array<alia::normal_vertex3d, 24> vertices;
    std::array<std::uint16_t, 36> indices;
};

cube_mesh make_cube() {
    // Four separate vertices per face keep the cube's normals flat.
    const std::array<alia::vec3f, 8> corners{{
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
        {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
        {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
        {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
    }};
    const std::array<std::array<int, 4>, 6> faces{{
        {{0, 1, 2, 3}}, {{5, 4, 7, 6}}, {{4, 0, 3, 7}},
        {{1, 5, 6, 2}}, {{3, 2, 6, 7}}, {{4, 5, 1, 0}},
    }};
    const std::array<alia::vec3f, 6> normals{{
        {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
    }};
    cube_mesh mesh{};
    for (int face = 0; face != 6; ++face) {
        for (int corner = 0; corner != 4; ++corner)
            mesh.vertices[face * 4 + corner] = {corners[faces[face][corner]], normals[face]};
        const auto base = static_cast<std::uint16_t>(face * 4);
        const std::array<std::uint16_t, 6> indices{{
            base, static_cast<std::uint16_t>(base + 1), static_cast<std::uint16_t>(base + 2),
            base, static_cast<std::uint16_t>(base + 2), static_cast<std::uint16_t>(base + 3),
        }};
        for (int i = 0; i != 6; ++i)
            mesh.indices[face * 6 + i] = indices[i];
    }
    return mesh;
}

alia::transform rotate_y(float angle) {
    auto result = alia::transform::identity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    result.m[0][0] = c; result.m[0][2] = -s;
    result.m[2][0] = s; result.m[2][2] = c;
    return result;
}

constexpr std::string_view d3d9_vertex_shader = R"(
struct vs_in {
    float3 position : POSITION0;
    float3 normal : NORMAL0;
    float4 offset_scale : TEXCOORD1;
    float4 tint : TEXCOORD2;
};
struct vs_out {
    float4 position : POSITION0;
    float3 normal : TEXCOORD0;
    float4 tint : COLOR0;
};
float4x4 u_view_projection : register(c0);
float4x4 u_rotation : register(c4);
vs_out main(vs_in input) {
    vs_out output;
    float3 position = mul(float4(input.position * input.offset_scale.w, 1.0), u_rotation).xyz;
    position += input.offset_scale.xyz;
    output.position = mul(float4(position, 1.0), u_view_projection);
    output.normal = mul(float4(input.normal, 0.0), u_rotation).xyz;
    output.tint = input.tint;
    return output;
}
)";

constexpr std::string_view d3d9_pixel_shader = R"(
struct ps_in {
    float3 normal : TEXCOORD0;
    float4 tint : COLOR0;
};
float4 main(ps_in input) : COLOR0 {
    float shade = 0.25 + 0.75 * saturate(dot(normalize(input.normal), normalize(float3(-0.35, 0.8, 0.55))));
    return float4(input.tint.rgb * shade, input.tint.a);
}
)";

constexpr std::string_view ogl_vertex_shader = R"(
#version 120
attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec4 a_instance0;
attribute vec4 a_instance1;
uniform mat4 u_view_projection;
uniform mat4 u_rotation;
varying vec3 v_normal;
varying vec4 v_tint;
void main() {
    vec3 position = (u_rotation * vec4(a_position * a_instance0.w, 1.0)).xyz + a_instance0.xyz;
    gl_Position = u_view_projection * vec4(position, 1.0);
    v_normal = (u_rotation * vec4(a_normal, 0.0)).xyz;
    v_tint = a_instance1;
}
)";

constexpr std::string_view ogl_pixel_shader = R"(
#version 120
varying vec3 v_normal;
varying vec4 v_tint;
void main() {
    float shade = 0.25 + 0.75 * max(dot(normalize(v_normal), normalize(vec3(-0.35, 0.8, 0.55))), 0.0);
    gl_FragColor = vec4(v_tint.rgb * shade, v_tint.a);
}
)";

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
        alia::window win({1000, 700}, {.title = "ALIA instancing: 256 cubes and strips", .resizable = true});
        auto device = alia::gfx_device::create(requested_backend(argc, argv));
        if (!device.caps().instancing)
            throw alia::unsupported_operation_exception("selected device does not support instancing");
        auto swapchain = device.create_swapchain({.target = win, .framebuffer = {.depth_bits = {24, alia::require}}});

        const auto cube = make_cube();
        alia::vertex_buffer<alia::normal_vertex3d> cube_vertices(device, std::span<const alia::normal_vertex3d>(cube.vertices));
        alia::index_buffer<std::uint16_t> cube_indices(device, std::span<const std::uint16_t>(cube.indices));
        const std::array<alia::normal_vertex3d, 4> strip{{
            {{-0.65f, 0.0f, -0.65f}, {0.0f, 1.0f, 0.0f}},
            {{0.65f, 0.0f, -0.65f}, {0.0f, 1.0f, 0.0f}},
            {{-0.65f, 0.0f, 0.65f}, {0.0f, 1.0f, 0.0f}},
            {{0.65f, 0.0f, 0.65f}, {0.0f, 1.0f, 0.0f}},
        }};
        // Same vertex layout, different VBO: both draws must fetch their own geometry.
        alia::vertex_buffer<alia::normal_vertex3d> strip_vertices(device, std::span<const alia::normal_vertex3d>(strip));

        constexpr int grid_size = 16;
        constexpr int cube_count = grid_size * grid_size;
        // Record zero is deliberately unused; both draws use nonzero first_instance.
        std::vector<cube_instance> data(1 + 2 * cube_count);
        for (int z = 0; z != grid_size; ++z) {
            for (int x = 0; x != grid_size; ++x) {
                const int i = z * grid_size + x;
                const float px = (x - (grid_size - 1) * 0.5f) * 1.6f;
                const float pz = (z - (grid_size - 1) * 0.5f) * 1.6f;
                const float u = static_cast<float>(x) / (grid_size - 1);
                const float v = static_cast<float>(z) / (grid_size - 1);
                data[1 + i] = {{px, 0.0f, pz, 1.0f}, {0.2f + 0.7f * u, 0.3f + 0.6f * v, 0.9f - 0.5f * u, 1.0f}};
                data[1 + cube_count + i] = {{px, -0.6f, pz, 1.0f}, {0.12f, 0.18f, 0.25f, 1.0f}};
            }
        }
        alia::instance_buffer<cube_instance> instances(device, std::span<const cube_instance>(data));
        alia::shader_program shader(device, "instancing", {
            {.backend = alia::gfx_backend::d3d9, .vertex = {d3d9_vertex_shader, {}, "main", "vs_3_0"}, .pixel = {d3d9_pixel_shader}},
            {.backend = alia::gfx_backend::opengl, .vertex = {ogl_vertex_shader}, .pixel = {ogl_pixel_shader}},
        });
        auto view_projection = shader.allocate_constant<alia::transform>("u_view_projection", alia::shader_type::vertex);
        auto rotation = shader.allocate_constant<alia::transform>("u_rotation", alia::shader_type::vertex);

        alia::event_queue events;
        events.register_source(&win.get_event_source());
        alia::frame_clock clock;
        bool running = true;
        while (running) {
            win.poll();
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>())
                    running = false;
                else if (const auto *resize = event.get_if<alia::window_resize_event>();
                         resize && resize->new_size.x > 0 && resize->new_size.y > 0)
                    swapchain.on_resize(resize->new_size);
                else if (const auto *key = event.get_if<alia::window_key_down_event>();
                         key && key->key == alia::key::escape)
                    running = false;
            }
            if (!running)
                break;

            const float t = static_cast<float>(clock.tick().elapsed);
            const auto size = swapchain.size();
            const auto view = alia::transform::look_at({24.0f, 25.0f, 30.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
            const auto projection = device.perspective_fov(45.0f, static_cast<float>(size.x) / size.y, 0.1f, 100.0f);
            view_projection.set_value(view * projection);

            auto frame = swapchain.begin_frame();
            frame.clear(alia::color(0.025f, 0.03f, 0.045f, 1.0f), 1.0f);
            frame.set_shader(shader);
            frame.set_blend(alia::no_blend);
            frame.set_depth(alia::depth_test_write);
            rotation.set_value(rotate_y(t * 0.7f));
            frame.draw_indexed_instanced(cube_vertices, cube_indices, instances, 0, -1, 0, 1, cube_count);
            rotation.set_value(alia::transform::identity());
            frame.draw_instanced(strip_vertices, instances, 0, -1, 1 + cube_count, cube_count, alia::primitive_topology::triangle_strip);
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "instancing example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
