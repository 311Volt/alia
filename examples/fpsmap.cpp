#include "alia/core/get_time.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/image_io.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/prim_buffers.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/gfx/texture.hpp"
#include "alia/io/keyboard.hpp"
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
#include <string>
#include <string_view>
#include <vector>

namespace {

float dot(alia::vec2f a, alia::vec2f b) {
    return a.x * b.x + a.y * b.y;
}

alia::vec3f normalized(alia::vec3f value) {
    const float length = std::sqrt(value.dot(value));
    return length == 0.0f ? alia::vec3f{} : value / length;
}

class perlin_noise_generator {
public:
    explicit perlin_noise_generator(std::uint32_t seed = 0x12345678u)
        : seed_(seed) {}

    float operator()(alia::vec2f position, int octaves = 1) const {
        octaves = (std::max)(1, octaves);
        float sum = 0.0f;
        float maximum = 0.0f;
        for (int i = 0; i < octaves; ++i) {
            const float scale = static_cast<float>(1u << i);
            const float coefficient = 1.0f / scale;
            maximum += coefficient;
            sum += base_octave(position * scale) * coefficient;
        }
        return sum / maximum;
    }

private:
    static float interpolate(float a, float b, float weight) {
        weight = std::clamp(weight, 0.0f, 1.0f);
        return (b - a) * (3.0f - weight * 2.0f) * weight * weight + a;
    }

    alia::vec2f random_gradient(alia::vec2i position) const {
        std::uint32_t value =
            static_cast<std::uint32_t>(position.x) * 89733u +
            static_cast<std::uint32_t>(position.y) * 2879327u +
            0xB16B00B5u;
        value = (value << 13) | (value >> 19);
        value ^= seed_;
        value *= 0xD398A93Fu;
        return {
            static_cast<float>(std::cos(static_cast<double>(value))),
            static_cast<float>(std::sin(static_cast<double>(value))),
        };
    }

    float base_octave(alia::vec2f position) const {
        const alia::vec2f floor_position{
            std::floor(position.x), std::floor(position.y)};
        const alia::vec2f fraction = position - floor_position;
        float values[4]{};
        for (int i = 0; i < 4; ++i) {
            const alia::vec2f lattice = floor_position + alia::vec2f{
                (i & 1) != 0 ? 1.0f : 0.0f,
                (i & 2) != 0 ? 1.0f : 0.0f,
            };
            values[i] = dot(
                random_gradient(alia::vec2i(lattice)), position - lattice);
        }
        const float x0 = interpolate(values[0], values[1], fraction.x);
        const float x1 = interpolate(values[2], values[3], fraction.x);
        return std::clamp(interpolate(x0, x1, fraction.y), -1.0f, 1.0f) * 0.5f + 0.5f;
    }

    std::uint32_t seed_;
};

struct textured_vertex3d {
    alia::vec3f position;
    alia::color col;
    alia::vec2f uv;

    static constexpr auto elements() {
        return std::array<alia::vertex_element, 3>{{
            {alia::vertex_attr::position, alia::vertex_storage::float_3, offsetof(textured_vertex3d, position)},
            {alia::vertex_attr::color_attr, alia::vertex_storage::float_4, offsetof(textured_vertex3d, col)},
            {alia::vertex_attr::tex_coord, alia::vertex_storage::float_2, offsetof(textured_vertex3d, uv)},
        }};
    }
};

struct mesh {
    std::vector<textured_vertex3d> vertices;
    std::vector<std::uint32_t> indices;
};

mesh create_terrain(int size_x, int size_y, float height, alia::vec2i texture_size) {
    mesh result;
    result.vertices.reserve(static_cast<std::size_t>(size_x * size_y));
    result.indices.reserve(static_cast<std::size_t>((size_x - 1) * (size_y - 1) * 6));
    const perlin_noise_generator perlin;
    for (int y = 0; y < size_y; ++y) {
        for (int x = 0; x < size_x; ++x) {
            const alia::vec2f position{static_cast<float>(x), static_cast<float>(y)};
            const float vertex_height = height * perlin(position * 0.01f, 8);
            const alia::vec2f uv{
                position.x * 15.0f / static_cast<float>(texture_size.x),
                position.y * 15.0f / static_cast<float>(texture_size.y),
            };
            result.vertices.push_back({
                {position.x, position.y, vertex_height},
                alia::color::gray(vertex_height / height),
                uv,
            });
            if (x < size_x - 1 && y < size_y - 1) {
                const std::uint32_t base = static_cast<std::uint32_t>(y * size_x + x);
                const std::uint32_t row = static_cast<std::uint32_t>(size_x);
                for (std::uint32_t offset : {0u, 1u, row, 1u, row + 1u, row})
                    result.indices.push_back(base + offset);
            }
        }
    }
    return result;
}

mesh create_skybox_mesh() {
    constexpr std::array positions{
        alia::vec3f{-1, -1, -1}, alia::vec3f{-1, 1, -1},
        alia::vec3f{1, 1, -1}, alia::vec3f{1, -1, -1},
        alia::vec3f{-1, -1, 1}, alia::vec3f{-1, 1, 1},
        alia::vec3f{1, 1, 1}, alia::vec3f{1, -1, 1},
    };
    constexpr std::array wall_uv{
        alia::vec2f{0, 2}, alia::vec2f{1, 2}, alia::vec2f{2, 2},
        alia::vec2f{3, 2}, alia::vec2f{4, 2}, alia::vec2f{0, 1},
        alia::vec2f{1, 1}, alia::vec2f{2, 1}, alia::vec2f{3, 1},
        alia::vec2f{4, 1},
    };
    constexpr std::array floor_ceiling_uv{
        alia::vec2f{1, 3}, alia::vec2f{1, 2}, alia::vec2f{2, 2}, alia::vec2f{2, 3},
        alia::vec2f{1, 0}, alia::vec2f{1, 1}, alia::vec2f{2, 1}, alia::vec2f{2, 0},
    };
    constexpr std::array<std::uint32_t, 24> wall_indices{
        0, 1, 5, 1, 5, 6,
        1, 2, 6, 2, 6, 7,
        2, 3, 7, 3, 7, 8,
        3, 4, 8, 4, 8, 9,
    };
    constexpr std::array<std::uint32_t, 12> floor_ceiling_indices{
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
    };

    mesh result;
    result.vertices.reserve(18);
    result.indices.reserve(36);
    for (std::size_t i = 0; i < wall_uv.size(); ++i) {
        const alia::vec2f uv{
            wall_uv[i].x / 4.0f,
            1.0f - wall_uv[i].y / 3.0f,
        };
        result.vertices.push_back({
            positions[(i % 5) % 4 + 4 * (i / 5)], alia::white, uv});
    }
    for (std::size_t i = 0; i < floor_ceiling_uv.size(); ++i) {
        const alia::vec2f uv{
            floor_ceiling_uv[i].x / 4.0f,
            1.0f - floor_ceiling_uv[i].y / 3.0f,
        };
        result.vertices.push_back({positions[i], alia::white, uv});
    }
    result.indices.insert(
        result.indices.end(), wall_indices.begin(), wall_indices.end());
    for (std::uint32_t index : floor_ceiling_indices)
        result.indices.push_back(index + 10u);
    return result;
}

class skybox {
public:
    explicit skybox(alia::gfx_device &device)
        : texture_(device, alia::load_image("./resources/nightsky.jpg")),
          mesh_(create_skybox_mesh()) {
        texture_.set_sampler(alia::linear_clamp);
    }

    void render(alia::frame &frame) {
        frame.set_texture(0, texture_, alia::linear_clamp);
        frame.draw_indexed<textured_vertex3d>(
            std::span<const textured_vertex3d>(mesh_.vertices.data(), mesh_.vertices.size()),
            std::span<const std::uint32_t>(mesh_.indices.data(), mesh_.indices.size()));
    }

private:
    alia::texture texture_;
    mesh mesh_;
};

struct camera {
    alia::vec3f pos{30.0f, 30.0f, 100.0f};
    alia::vec2d rot{};
    inline static constexpr alia::vec3f up{0.0f, 0.0f, -1.0f};

    [[nodiscard]] alia::vec3f forward() const {
        return normalized({
            static_cast<float>(std::cos(rot.x) * std::cos(rot.y)),
            static_cast<float>(std::cos(rot.x) * std::sin(rot.y)),
            static_cast<float>(std::sin(-rot.x)),
        });
    }

    void rotate(alia::vec2f delta) {
        rot += alia::vec2d(delta);
        rot.x = std::clamp(rot.x, -1.5706, 1.5706);
        rot.y = std::fmod(rot.y, 2.0 * std::numbers::pi);
    }

    void rotate_degrees(alia::vec2f delta) {
        rotate(delta * (std::numbers::pi_v<float> / 180.0f));
    }

    [[nodiscard]] alia::vec3f right() const {
        return normalized(forward().cross(up));
    }

    [[nodiscard]] alia::transform view() const {
        return alia::transform::look_at(pos, pos + forward(), up);
    }
};

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
            {1024, 768},
            {.title = "ALIA terrain camera", .resizable = true});
        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        // NOTE (API feedback): swapchain depth format and low-level presentation
        // flags are not configurable; the available vsync policy is disabled here.
        auto swapchain = device.create_swapchain({
            .target = win,
            .vsync = alia::vsync_mode::disable,
        });

        alia::texture rock(device, alia::load_image("./resources/rock.jpg"));
        rock.set_sampler(alia::linear_wrap);
        const mesh terrain = create_terrain(128, 128, 96.0f, rock.size());
        alia::vertex_buffer<textured_vertex3d> terrain_vertices(
            device,
            std::span<const textured_vertex3d>(
                terrain.vertices.data(), terrain.vertices.size()),
            alia::buffer_usage::static_mesh);
        alia::index_buffer terrain_indices(
            device,
            std::span<const std::uint32_t>(
                terrain.indices.data(), terrain.indices.size()),
            alia::buffer_usage::static_mesh);
        skybox sky(device);

        alia::basic_effect scene_fx{.texture_op = alia::texture_operation::modulate};
        auto sky_pipeline = alia::pipeline::create<textured_vertex3d>(
            device, {.effect = &scene_fx});
        auto terrain_pipeline = alia::pipeline::create<textured_vertex3d>(
            device,
            {
                .effect = &scene_fx,
                .depth = {.test_enabled = true, .write_enabled = true},
            });

        alia::ttf_font font = alia::load_ttf_font("./resources/roboto.ttf", 16);
        alia::hardware_glyph_buffer glyphs(device, font);
        alia::basic_effect text_fx{.texture_op = alia::texture_operation::alpha_mask};
        auto text_pipeline = alia::pipeline::create<alia::full_vertex>(
            device, {.effect = &text_fx});

        alia::event_queue events;
        events.register_source(&win.get_event_source());
        camera player_camera;
        alia::vec2i last_mouse = win.size() / 2;
        win.hide_cursor();

        double last_time = alia::get_time();
        // NOTE (API feedback): frame timing and FPS measurement are local
        // because alia has no event-loop timing helpers.
        double fps_window_start = last_time;
        int fps_frames = 0;
        int displayed_fps = 0;
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
                } else if (const auto *mouse = event.get_if<alia::window_mouse_move_event>()) {
                    // NOTE (API feedback): there is no relative mouse mode or
                    // mouse-delta event, so absolute positions must be differenced.
                    const alia::vec2i delta = mouse->position - last_mouse;
                    last_mouse = mouse->position;
                    player_camera.rotate_degrees({
                        -static_cast<float>(delta.y) * 0.022f * 4.0f,
                        static_cast<float>(delta.x) * 0.022f * 4.0f,
                    });
                }
            }

            const alia::vec2i center = win.size() / 2;
            if ((last_mouse - center).length() > 15.0) {
                win.set_cursor_position(center);
                last_mouse = center;
            }

            const double now = alia::get_time();
            const float dt = static_cast<float>(now - last_time);
            last_time = now;
            const float movement = dt * 30.0f;
            const alia::keyboard_state keyboard = alia::get_keyboard_state();
            if (keyboard[alia::key::W])
                player_camera.pos += player_camera.forward() * movement;
            if (keyboard[alia::key::S])
                player_camera.pos -= player_camera.forward() * movement;
            if (keyboard[alia::key::A])
                player_camera.pos -= player_camera.right() * movement;
            if (keyboard[alia::key::D])
                player_camera.pos += player_camera.right() * movement;

            ++fps_frames;
            const double fps_elapsed = now - fps_window_start;
            if (fps_elapsed >= 0.25) {
                displayed_fps = static_cast<int>(fps_frames / fps_elapsed + 0.5);
                fps_frames = 0;
                fps_window_start = now;
            }

            auto frame = swapchain.begin_frame();
            frame.clear(alia::black, 1.0f);
            const float aspect = frame.target_size().y > 0
                ? static_cast<float>(frame.target_size().x) / frame.target_size().y
                : 1.0f;
            scene_fx.projection =
                device.perspective_fov(78.0f, aspect, 0.01f, 10000.0f);

            scene_fx.world = alia::transform::look_at(
                {0.0f, 0.0f, 0.0f},
                player_camera.forward(),
                camera::up);
            frame.set_pipeline(sky_pipeline);
            sky.render(frame);

            scene_fx.world = player_camera.view();
            frame.set_pipeline(terrain_pipeline);
            frame.set_texture(0, rock, alia::linear_wrap);
            frame.draw_indexed(terrain_vertices, terrain_indices);

            text_fx.projection = device.ortho_ui(frame.target_size());
            frame.set_pipeline(text_pipeline);
            alia::draw_text(
                frame,
                {15.0f, 15.0f},
                glyphs,
                std::format("{} fps", displayed_fps));
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "fpsmap example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
