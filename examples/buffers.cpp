#include "alia/core/get_time.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/prim_buffers.hpp"
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
#include <string>
#include <string_view>
#include <vector>

namespace {

template <class T>
struct arr2d {
    explicit arr2d(alia::vec2i size)
        : size(size), data(static_cast<std::size_t>(size.x * size.y)) {}

    [[nodiscard]] std::size_t index_at(int x, int y) const {
        return static_cast<std::size_t>(y * size.x + x);
    }

    T &operator()(int x, int y) {
        return data[index_at(x, y)];
    }

    const T &operator()(int x, int y) const {
        return data[index_at(x, y)];
    }

    alia::vec2i size;
    std::vector<T> data;
};

double sin2(double value) {
    return 0.5 + 0.5 * std::sin(2.0 * value);
}

arr2d<alia::colored_vertex> generate_grid(int width, int height) {
    arr2d<alia::colored_vertex> result({width, height});
    const float x_denominator = static_cast<float>((std::max)(1, width - 1));
    const float y_denominator = static_cast<float>((std::max)(1, height - 1));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            result(x, y) = {
                {static_cast<float>(x) / x_denominator,
                 static_cast<float>(y) / y_denominator},
                alia::white,
            };
        }
    }
    return result;
}

std::vector<std::uint32_t> generate_indices(
    const arr2d<alia::colored_vertex> &vertices
) {
    std::vector<std::uint32_t> result;
    result.reserve(static_cast<std::size_t>(
        (vertices.size.x - 1) * (vertices.size.y - 1) * 6));
    for (int y = 0; y < vertices.size.y - 1; ++y) {
        for (int x = 0; x < vertices.size.x - 1; ++x) {
            const std::uint32_t base =
                static_cast<std::uint32_t>(vertices.index_at(x, y));
            const std::uint32_t width = static_cast<std::uint32_t>(vertices.size.x);
            for (std::uint32_t offset : {0u, 1u, width, 1u, width + 1u, width})
                result.push_back(base + offset);
        }
    }
    return result;
}

void update_colors(
    arr2d<alia::colored_vertex> &vertices,
    double time,
    double extent
) {
    const int rows = std::clamp(
        static_cast<int>(std::ceil(vertices.size.y * extent)),
        1,
        vertices.size.y);
    constexpr std::array phases{
        0.0,
        std::numbers::pi / 6.0,
        2.0 * std::numbers::pi / 6.0,
    };
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < vertices.size.x; ++x) {
            const double base = x / 20.0 + y / 20.0 + time;
            vertices(x, y).col = {
                static_cast<float>(sin2(base + phases[0])),
                static_cast<float>(sin2(base + phases[1])),
                static_cast<float>(sin2(base + phases[2])),
                1.0f,
            };
        }
    }
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
            {.title = "ALIA dynamic buffers", .resizable = true});
        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        alia::make_current(device);
        auto swapchain = device.create_swapchain({.target = win});

        auto grid = generate_grid(64, 64);
        const auto indices = generate_indices(grid);
        alia::vertex_buffer<alia::colored_vertex> vertex_buffer(
            device,
            std::span<const alia::colored_vertex>(grid.data.data(), grid.data.size()),
            alia::buffer_usage::dynamic_mesh);
        alia::index_buffer index_buffer(
            device,
            std::span<const std::uint32_t>(indices.data(), indices.size()),
            alia::buffer_usage::static_mesh);

        alia::basic_effect prim_fx;
        auto prim_pipeline = alia::pipeline::create<alia::colored_vertex>(
            device, {.effect = &prim_fx});
        alia::ttf_font font = alia::load_ttf_font("./resources/roboto.ttf", 16);
        alia::hardware_glyph_buffer glyphs(device, font);
        alia::basic_effect text_fx{.texture_op = alia::texture_operation::alpha_mask};
        auto text_pipeline = alia::pipeline::create<alia::full_vertex>(
            device, {.effect = &text_fx});

        alia::event_queue events;
        events.register_source(&win.get_event_source());
        // NOTE (API feedback): the rolling FPS counter is local because alia
        // has no event-loop FPS helper.
        double fps_window_start = alia::get_time();
        int fps_frames = 0;
        int displayed_fps = 0;

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
            }

            const double now = alia::get_time();
            const double extent = sin2(now);
            update_colors(grid, now, extent);
            const int lock_size = (std::max)(
                1, static_cast<int>(vertex_buffer.count() * extent));
            // NOTE (API feedback): write_only replaces the requested range,
            // so preserving the old buffer contents is unnecessary.
            if (auto locked = vertex_buffer.lock_write_only(0, lock_size))
                std::copy_n(grid.data.begin(), lock_size, locked.view().begin());

            ++fps_frames;
            const double fps_elapsed = now - fps_window_start;
            if (fps_elapsed >= 0.25) {
                displayed_fps = static_cast<int>(fps_frames / fps_elapsed + 0.5);
                fps_frames = 0;
                fps_window_start = now;
            }

            auto frame = swapchain.begin_frame();
            frame.clear(alia::black);
            prim_fx.world = alia::transform::scale(alia::vec2f(frame.target_size()));
            prim_fx.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(prim_pipeline);
            frame.draw_indexed(vertex_buffer, index_buffer);
            prim_fx.world = alia::transform::identity();

            text_fx.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(text_pipeline);
            const std::string fps_text = std::format("{} fps", displayed_fps);
            alia::draw_text(frame, {16.0f, 16.0f}, glyphs, fps_text, alia::black);
            alia::draw_text(frame, {15.0f, 15.0f}, glyphs, fps_text, alia::white);
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "buffers example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
