#include "alia/core/get_time.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/cube_texture.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/shader.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/os/window.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

struct ball_vertex {
    alia::vec3f position;
    alia::vec3f normal;
    alia::vec2f uv;

    static constexpr auto elements() {
        return std::array<alia::vertex_element, 3>{{
            {alia::vertex_attr::position, alia::vertex_storage::float_3,
             offsetof(ball_vertex, position)},
            {alia::vertex_attr::normal, alia::vertex_storage::float_3,
             offsetof(ball_vertex, normal)},
            {alia::vertex_attr::tex_coord, alia::vertex_storage::float_2,
             offsetof(ball_vertex, uv)},
        }};
    }
};

struct sphere_mesh {
    std::vector<ball_vertex> vertices;
    std::vector<std::uint32_t> indices;
};

constexpr std::string_view d3d9_sky_vertex_shader = R"(
struct vs_in { float3 position : POSITION0; };
struct vs_out {
    float4 position : POSITION0;
    float3 direction : TEXCOORD0;
};

float4x4 u_sky_view_proj : register(c0);

vs_out main(vs_in input) {
    vs_out output;
    float4 clip = mul(float4(input.position, 1.0), u_sky_view_proj);
    output.position = clip.xyww;
    output.direction = input.position;
    return output;
}
)";

constexpr std::string_view d3d9_sky_pixel_shader = R"(
struct ps_in { float3 direction : TEXCOORD0; };
samplerCUBE u_sky : register(s0);

float4 main(ps_in input) : COLOR0 {
    return texCUBE(u_sky, normalize(input.direction));
}
)";

constexpr std::string_view ogl_sky_vertex_shader = R"(
#version 120
attribute vec3 a_position;
uniform mat4 u_sky_view_proj;
varying vec3 v_direction;

void main() {
    vec4 clip = u_sky_view_proj * vec4(a_position, 1.0);
    gl_Position = clip.xyww;
    v_direction = a_position;
}
)";

constexpr std::string_view ogl_sky_pixel_shader = R"(
#version 120
uniform samplerCube u_sky;
varying vec3 v_direction;

void main() {
    gl_FragColor = textureCube(u_sky, normalize(v_direction));
}
)";

constexpr std::string_view d3d9_ball_vertex_shader = R"(
struct vs_in {
    float3 position : POSITION0;
    float3 normal : NORMAL0;
    float2 uv : TEXCOORD0;
};

struct vs_out {
    float4 position : POSITION0;
    float3 world_position : TEXCOORD0;
    float3 world_normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

float4x4 u_world : register(c0);
float4x4 u_view_proj : register(c4);

vs_out main(vs_in input) {
    vs_out output;
    float4 world_position = mul(float4(input.position, 1.0), u_world);
    output.position = mul(world_position, u_view_proj);
    output.world_position = world_position.xyz;
    output.world_normal = mul(float4(input.normal, 0.0), u_world).xyz;
    output.uv = input.uv;
    return output;
}
)";

constexpr std::string_view d3d9_ball_pixel_shader = R"(
struct ps_in {
    float3 world_position : TEXCOORD0;
    float3 world_normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

sampler2D u_normal_map : register(s0);
samplerCUBE u_environment : register(s1);
float3 u_camera_position : register(c0);
float3 u_light_direction : register(c1);
float u_roughness : register(c2);

float4 main(ps_in input) : COLOR0 {
    float3 geometric_normal = normalize(input.world_normal);
    float3 reference_axis = abs(geometric_normal.y) < 0.96
        ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(reference_axis, geometric_normal));
    float3 bitangent = cross(geometric_normal, tangent);
    float3 map_normal = tex2D(u_normal_map, input.uv * 3.0).xyz * 2.0 - 1.0;
    float3 normal = normalize(
        tangent * map_normal.x + bitangent * map_normal.y +
        geometric_normal * map_normal.z);

    float3 view_direction = normalize(u_camera_position - input.world_position);
    float3 reflection = reflect(-view_direction, normal);
    float3 blur_axis = abs(reflection.y) < 0.95
        ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 blur_tangent = normalize(cross(blur_axis, reflection));
    float3 blur_bitangent = cross(reflection, blur_tangent);
    float spread = 0.24 * u_roughness;
    float3 environment = texCUBE(u_environment, reflection).rgb * 0.40;
    environment += texCUBE(u_environment, normalize(reflection + blur_tangent * spread)).rgb * 0.15;
    environment += texCUBE(u_environment, normalize(reflection - blur_tangent * spread)).rgb * 0.15;
    environment += texCUBE(u_environment, normalize(reflection + blur_bitangent * spread)).rgb * 0.15;
    environment += texCUBE(u_environment, normalize(reflection - blur_bitangent * spread)).rgb * 0.15;

    float diffuse = saturate(dot(normal, -normalize(u_light_direction)));
    float3 half_vector = normalize(view_direction - normalize(u_light_direction));
    float specular = pow(saturate(dot(normal, half_vector)), lerp(70.0, 9.0, u_roughness));
    float fresnel = pow(1.0 - saturate(dot(normal, view_direction)), 5.0);
    float reflection_amount = 0.07 + 0.18 * fresnel;
    float3 base = float3(0.34, 0.24, 0.17);
    float3 lit = base * (0.22 + 0.78 * diffuse) + specular * 0.22;
    return float4(lerp(lit, environment, reflection_amount), 1.0);
}
)";

constexpr std::string_view ogl_ball_vertex_shader = R"(
#version 120
attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_tex_coord;
uniform mat4 u_world;
uniform mat4 u_view_proj;
varying vec3 v_world_position;
varying vec3 v_world_normal;
varying vec2 v_uv;

void main() {
    vec4 world_position = u_world * vec4(a_position, 1.0);
    gl_Position = u_view_proj * world_position;
    v_world_position = world_position.xyz;
    v_world_normal = (u_world * vec4(a_normal, 0.0)).xyz;
    v_uv = a_tex_coord;
}
)";

constexpr std::string_view ogl_ball_pixel_shader = R"(
#version 120
uniform sampler2D u_normal_map;
uniform samplerCube u_environment;
uniform vec3 u_camera_position;
uniform vec3 u_light_direction;
uniform float u_roughness;
varying vec3 v_world_position;
varying vec3 v_world_normal;
varying vec2 v_uv;

void main() {
    vec3 geometric_normal = normalize(v_world_normal);
    vec3 reference_axis = abs(geometric_normal.y) < 0.96
        ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(reference_axis, geometric_normal));
    vec3 bitangent = cross(geometric_normal, tangent);
    vec3 map_normal = texture2D(u_normal_map, v_uv * 3.0).xyz * 2.0 - 1.0;
    vec3 normal = normalize(
        tangent * map_normal.x + bitangent * map_normal.y +
        geometric_normal * map_normal.z);

    vec3 view_direction = normalize(u_camera_position - v_world_position);
    vec3 reflection = reflect(-view_direction, normal);
    vec3 blur_axis = abs(reflection.y) < 0.95
        ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 blur_tangent = normalize(cross(blur_axis, reflection));
    vec3 blur_bitangent = cross(reflection, blur_tangent);
    float spread = 0.24 * u_roughness;
    vec3 environment = textureCube(u_environment, reflection).rgb * 0.40;
    environment += textureCube(u_environment, normalize(reflection + blur_tangent * spread)).rgb * 0.15;
    environment += textureCube(u_environment, normalize(reflection - blur_tangent * spread)).rgb * 0.15;
    environment += textureCube(u_environment, normalize(reflection + blur_bitangent * spread)).rgb * 0.15;
    environment += textureCube(u_environment, normalize(reflection - blur_bitangent * spread)).rgb * 0.15;

    float diffuse = max(dot(normal, -normalize(u_light_direction)), 0.0);
    vec3 half_vector = normalize(view_direction - normalize(u_light_direction));
    float specular = pow(max(dot(normal, half_vector), 0.0), mix(70.0, 9.0, u_roughness));
    float fresnel = pow(1.0 - max(dot(normal, view_direction), 0.0), 5.0);
    float reflection_amount = 0.07 + 0.18 * fresnel;
    vec3 base = vec3(0.34, 0.24, 0.17);
    vec3 lit = base * (0.22 + 0.78 * diffuse) + specular * 0.22;
    gl_FragColor = vec4(mix(lit, environment, reflection_amount), 1.0);
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

alia::transform look_at(alia::vec3f eye, alia::vec3f target, alia::vec3f up) {
    const alia::vec3f forward = normalized(target - eye);
    const alia::vec3f side = normalized(cross(forward, up));
    const alia::vec3f camera_up = cross(side, forward);
    alia::transform result = alia::transform::identity();
    result.m[0][0] = side.x;
    result.m[0][1] = camera_up.x;
    result.m[0][2] = -forward.x;
    result.m[1][0] = side.y;
    result.m[1][1] = camera_up.y;
    result.m[1][2] = -forward.y;
    result.m[2][0] = side.z;
    result.m[2][1] = camera_up.z;
    result.m[2][2] = -forward.z;
    result.m[3][0] = -dot(side, eye);
    result.m[3][1] = -dot(camera_up, eye);
    result.m[3][2] = dot(forward, eye);
    return result;
}

alia::transform perspective_fov(float aspect, bool zero_to_one) {
    constexpr float near_plane = 0.05f;
    constexpr float far_plane = 30.0f;
    const float radians = 55.0f * std::numbers::pi_v<float> / 180.0f;
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

alia::transform rotation_x(float angle) {
    alia::transform result = alia::transform::identity();
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    result.m[1][1] = cosine;
    result.m[1][2] = sine;
    result.m[2][1] = -sine;
    result.m[2][2] = cosine;
    return result;
}

alia::transform rotation_y(float angle) {
    alia::transform result = alia::transform::identity();
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    result.m[0][0] = cosine;
    result.m[0][2] = -sine;
    result.m[2][0] = sine;
    result.m[2][2] = cosine;
    return result;
}

sphere_mesh make_sphere(int rings, int slices) {
    sphere_mesh result;
    result.vertices.reserve(static_cast<std::size_t>((rings + 1) * (slices + 1)));
    result.indices.reserve(static_cast<std::size_t>(rings * slices * 6));

    for (int ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / rings;
        const float latitude = v * std::numbers::pi_v<float>;
        const float radius = std::sin(latitude);
        const float y = std::cos(latitude);
        for (int slice = 0; slice <= slices; ++slice) {
            const float u = static_cast<float>(slice) / slices;
            const float longitude = u * 2.0f * std::numbers::pi_v<float>;
            const alia::vec3f normal{
                radius * std::sin(longitude),
                y,
                radius * std::cos(longitude),
            };
            result.vertices.push_back({normal, normal, {u, v}});
        }
    }

    const int row = slices + 1;
    for (int ring = 0; ring < rings; ++ring) {
        for (int slice = 0; slice < slices; ++slice) {
            const std::uint32_t a = static_cast<std::uint32_t>(ring * row + slice);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(row);
            const std::uint32_t d = c + 1;
            result.indices.insert(result.indices.end(), {a, c, b, b, c, d});
        }
    }
    return result;
}

alia::bitmap make_normal_map() {
    constexpr int edge = 256;
    alia::bitmap result({edge, edge}, alia::px_rgba8888{128, 128, 255, 255});
    auto pixels = result.view_as<alia::px_rgba8888>();
    for (int y = 0; y < edge; ++y) {
        for (int x = 0; x < edge; ++x) {
            const float u = static_cast<float>(x) / edge;
            const float v = static_cast<float>(y) / edge;
            const float nx = 0.30f * std::sin(u * 24.0f * std::numbers::pi_v<float>) *
                std::cos(v * 10.0f * std::numbers::pi_v<float>);
            const float ny = 0.24f * std::cos(u * 14.0f * std::numbers::pi_v<float>) *
                std::sin(v * 22.0f * std::numbers::pi_v<float>);
            const float nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
            auto pack = [](float value) {
                return static_cast<std::uint8_t>((value * 0.5f + 0.5f) * 255.0f + 0.5f);
            };
            pixels[x, y] = {pack(nx), pack(ny), pack(nz), 255};
        }
    }
    return result;
}

std::array<alia::bitmap, alia::cube_face_count> load_environment_faces(
    std::string_view path) {
    auto atlas = alia::load_image(path);
    if (atlas.width() % 4 != 0 || atlas.height() % 3 != 0 ||
        atlas.width() / 4 != atlas.height() / 3)
        throw std::runtime_error(
            "cube reflection environment must use a 4x3 cross layout");

    const int edge = atlas.width() / 4;
    const auto face = [&](int column, int row) {
        return alia::bitmap(atlas.view().subview(
            alia::rect_i::pos_size({column * edge, row * edge}, {edge, edge})));
    };

    // The cross is laid out as +Y above, -Y below, and
    // -X, +Z, +X, -Z across the middle row.
    return {
        face(2, 1), face(0, 1), face(1, 0),
        face(1, 2), face(1, 1), face(3, 1),
    };
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
            {960, 640},
            {.title = "ALIA rough ball cube reflections", .resizable = true});
        auto device = alia::gfx_device::create(requested_backend(argc, argv));
        alia::make_current(device);
        alia::swapchain_config swapchain_config{.target = window};
        swapchain_config.framebuffer.depth_bits = {24, alia::require};
        auto swapchain = device.create_swapchain(swapchain_config);

        if (!device.caps().cube_textures)
            throw std::runtime_error("selected device does not support cube textures");

        auto environment_faces =
            load_environment_faces("./resources/nightsky.jpg");
        alia::cube_texture environment(
            device,
            std::span<const alia::bitmap, alia::cube_face_count>(environment_faces));
        environment.set_sampler(alia::linear_clamp);
        alia::texture normal_map(device, make_normal_map());
        normal_map.set_sampler(alia::linear_wrap);
        const sphere_mesh ball = make_sphere(48, 72);

        const std::array<alia::shader_source, 4> sky_sources{{
            {alia::gfx_backend::d3d9, alia::shader_type::vertex,
             d3d9_sky_vertex_shader, "main", {}, "rough_ball_sky_vs_hlsl"},
            {alia::gfx_backend::d3d9, alia::shader_type::pixel,
             d3d9_sky_pixel_shader, "main", {}, "rough_ball_sky_ps_hlsl"},
            {alia::gfx_backend::opengl, alia::shader_type::vertex,
             ogl_sky_vertex_shader, "main", {}, "rough_ball_sky_vs_glsl"},
            {alia::gfx_backend::opengl, alia::shader_type::pixel,
             ogl_sky_pixel_shader, "main", {}, "rough_ball_sky_ps_glsl"},
        }};
        const std::array<alia::shader_constant_binding, 1> sky_constants{{
            {"u_sky_view_proj", alia::shader_type::vertex, 0, 4},
        }};
        const std::array<alia::shader_sampler_binding, 1> sky_samplers{{
            {"u_sky", alia::shader_type::pixel, 0},
        }};
        alia::shader_program sky_shader(
            device,
            {.sources = sky_sources,
             .constant_bindings = sky_constants,
             .sampler_bindings = sky_samplers});
        auto sky_view_projection = sky_shader.allocate_constant<alia::transform>(
            "u_sky_view_proj", alia::shader_type::vertex);
        auto sky_sampler = sky_shader.allocate_sampler("u_sky");
        sky_sampler.set_texture(environment);
        auto sky_pipeline = alia::pipeline::create<alia::vertex3d>(
            device,
            {.effect = &sky_shader,
             .blend = {.enabled = false},
             .depth = {true, false, alia::compare_func::less_equal}});

        const std::array<alia::shader_source, 4> ball_sources{{
            {alia::gfx_backend::d3d9, alia::shader_type::vertex,
             d3d9_ball_vertex_shader, "main", {}, "rough_ball_vs_hlsl"},
            {alia::gfx_backend::d3d9, alia::shader_type::pixel,
             d3d9_ball_pixel_shader, "main", {}, "rough_ball_ps_hlsl"},
            {alia::gfx_backend::opengl, alia::shader_type::vertex,
             ogl_ball_vertex_shader, "main", {}, "rough_ball_vs_glsl"},
            {alia::gfx_backend::opengl, alia::shader_type::pixel,
             ogl_ball_pixel_shader, "main", {}, "rough_ball_ps_glsl"},
        }};
        const std::array<alia::shader_constant_binding, 5> ball_constants{{
            {"u_world", alia::shader_type::vertex, 0, 4},
            {"u_view_proj", alia::shader_type::vertex, 4, 4},
            {"u_camera_position", alia::shader_type::pixel, 0, 1},
            {"u_light_direction", alia::shader_type::pixel, 1, 1},
            {"u_roughness", alia::shader_type::pixel, 2, 1},
        }};
        const std::array<alia::shader_sampler_binding, 2> ball_samplers{{
            {"u_normal_map", alia::shader_type::pixel, 0},
            {"u_environment", alia::shader_type::pixel, 1},
        }};
        alia::shader_program ball_shader(
            device,
            {.sources = ball_sources,
             .constant_bindings = ball_constants,
             .sampler_bindings = ball_samplers});
        auto world_constant = ball_shader.allocate_constant<alia::transform>(
            "u_world", alia::shader_type::vertex);
        auto view_projection_constant = ball_shader.allocate_constant<alia::transform>(
            "u_view_proj", alia::shader_type::vertex);
        auto camera_constant = ball_shader.allocate_constant<alia::vec3f>(
            "u_camera_position", alia::shader_type::pixel);
        auto light_constant = ball_shader.allocate_constant<alia::vec3f>(
            "u_light_direction", alia::shader_type::pixel);
        auto roughness_constant = ball_shader.allocate_constant<float>(
            "u_roughness", alia::shader_type::pixel);
        auto normal_sampler = ball_shader.allocate_sampler("u_normal_map");
        auto environment_sampler = ball_shader.allocate_sampler("u_environment");
        normal_sampler.set_texture(normal_map);
        environment_sampler.set_texture(environment);
        auto ball_pipeline = alia::pipeline::create<ball_vertex>(
            device,
            {.effect = &ball_shader,
             .blend = {.enabled = false},
             .depth = {true, true, alia::compare_func::less_equal}});

        const std::array sky_vertices{
            alia::vertex3d{{-1.0f, -1.0f, -1.0f}},
            alia::vertex3d{{ 1.0f, -1.0f, -1.0f}},
            alia::vertex3d{{ 1.0f,  1.0f, -1.0f}},
            alia::vertex3d{{-1.0f,  1.0f, -1.0f}},
            alia::vertex3d{{-1.0f, -1.0f,  1.0f}},
            alia::vertex3d{{ 1.0f, -1.0f,  1.0f}},
            alia::vertex3d{{ 1.0f,  1.0f,  1.0f}},
            alia::vertex3d{{-1.0f,  1.0f,  1.0f}},
        };
        const std::array<std::uint32_t, 36> sky_indices{{
            0, 1, 2, 0, 2, 3, 5, 4, 7, 5, 7, 6,
            4, 0, 3, 4, 3, 7, 1, 5, 6, 1, 6, 2,
            3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
        }};

        alia::ttf_font font = alia::load_ttf_font("./resources/roboto.ttf", 17);
        alia::hardware_glyph_buffer glyphs(device, font);
        alia::basic_effect text_effect{
            .texture_op = alia::texture_operation::alpha_mask};
        auto text_pipeline = alia::pipeline::create<alia::full_vertex>(
            device, {.effect = &text_effect});

        alia::event_queue events;
        events.register_source(&window.get_event_source());
        bool running = true;
        while (running) {
            window.poll();
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>())
                    running = false;
                else if (const auto *resize = event.get_if<alia::window_resize_event>())
                    swapchain.on_resize(resize->new_size);
                else if (const auto *key = event.get_if<alia::window_key_down_event>();
                         key && key->key == alia::key::escape)
                    running = false;
            }

            const float time = static_cast<float>(alia::get_time());
            const alia::vec3f camera_position{0.0f, 0.1f, -3.35f};
            const auto view = look_at(
                camera_position, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});

            auto frame = swapchain.begin_frame();
            frame.clear(alia::color(0.015f, 0.02f, 0.035f, 1.0f), 1.0f);
            const float aspect = frame.target_size().y > 0
                ? static_cast<float>(frame.target_size().x) / frame.target_size().y
                : 1.0f;
            const auto projection = perspective_fov(
                aspect, device.backend()->id == alia::gfx_backend::d3d9);

            auto sky_view = view;
            sky_view.m[3][0] = 0.0f;
            sky_view.m[3][1] = 0.0f;
            sky_view.m[3][2] = 0.0f;
            sky_view_projection.set_value(sky_view * projection);
            frame.set_pipeline(sky_pipeline);
            frame.draw_indexed<alia::vertex3d>(sky_vertices, sky_indices);

            world_constant.set_value(
                rotation_y(time * 0.32f) * rotation_x(-0.18f));
            view_projection_constant.set_value(view * projection);
            camera_constant.set_value(camera_position);
            light_constant.set_value({-0.45f, -0.8f, -0.35f});
            roughness_constant.set_value(0.68f);
            frame.set_pipeline(ball_pipeline);
            frame.draw_indexed<ball_vertex>(
                std::span<const ball_vertex>(ball.vertices),
                std::span<const std::uint32_t>(ball.indices));

            text_effect.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(text_pipeline);
            alia::draw_text(
                frame, {16.0f, 16.0f}, glyphs,
                "normal-mapped rough ball | five-tap cubemap reflection",
                alia::white);
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "cube_reflection example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
