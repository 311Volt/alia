#include "alia/core/timing.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/primitive_renderer.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/os/window.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <format>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string_view>

namespace {

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
    constexpr std::uint64_t benchmark_ticks = 700;
    alia::frame_statistics measured;
    try {
        alia::window win(
            {800, 600},
            {.title = "ALIA primitive benchmark", .resizable = true});
        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        auto swapchain = device.create_swapchain({.target = win});

        alia::primitive_renderer renderer;

        alia::ttf_font font = alia::load_ttf_font("./resources/roboto.ttf", 16);
        alia::hardware_glyph_buffer glyphs(device, font);

        alia::event_queue events;
        events.register_source(&win.get_event_source());
        std::random_device seed;
        std::mt19937_64 random(seed());
        alia::frame_clock clock;
        alia::fps_counter fps;
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

            const auto tick = clock.tick().tick;
            if (tick >= benchmark_ticks)
                running = false;

            auto frame = swapchain.begin_frame();
            frame.clear(alia::black);
            std::uniform_int_distribution<int> x_distribution(
                0, (std::max)(0, frame.target_size().x));
            std::uniform_int_distribution<int> y_distribution(
                0, (std::max)(0, frame.target_size().y));
            for (int i = 0; i < 100000; ++i) {
                const alia::vec2f a{
                    static_cast<float>(x_distribution(random)),
                    static_cast<float>(y_distribution(random)),
                };
                const alia::vec2f b{
                    static_cast<float>(x_distribution(random)),
                    static_cast<float>(y_distribution(random)),
                };
                renderer.draw_line(frame, a, b, alia::blue);
            }
            // One transient submission: roughly 400k vertices and 600k indices.
            renderer.flush(frame);

            alia::draw_text({
                .target = frame,
                .glyphs = glyphs,
                .text = std::format(
                    "tick={}, avg frametime: {:.9f} ms",
                    tick,
                    1000.0 * fps.total().mean_frame_time()),
                .texture_slot = 0,
                .position = {100.0f, 100.0f},
                .tint = alia::pure_yellow
            });
            frame.present();
            if (tick == 1)
                fps.reset(); // Exclude the first completed frame as warm-up.
            else
                fps.count_frame();
        }
        measured = fps.total();
    } catch (const std::exception &error) {
        std::cerr << "benchmark example failed: " << error.what() << '\n';
        return 1;
    }

    std::printf(
        "average frametime over %llu measured frames: %.9f ms\n",
        static_cast<unsigned long long>(measured.frames),
        1000.0 * measured.mean_frame_time());
    return 0;
}
