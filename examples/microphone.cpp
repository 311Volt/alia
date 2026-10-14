#include "alia/audio/audio.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/primitive_renderer.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/os/window.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <format>
#include <iostream>
#include <mutex>
#include <thread>

int main() {
    try {
        auto audio = alia::audio_context::create();
        alia::window win({640, 240}, {.title = "ALIA microphone: numpad +/- gain, Escape quits (use headphones)"});
        auto graphics = alia::gfx_device::create();
        auto swapchain = graphics.create_swapchain({.target = win});
        alia::event_queue events;
        events.register_source(&win.get_event_source());
        auto font = alia::load_ttf_font("./resources/roboto.ttf", 16);
        alia::hardware_glyph_buffer glyphs(graphics, font);
        alia::immediate_primitive_renderer renderer;
        alia::ring_buffer<alia::vec2f> frames{32768};
        std::mutex buffer_mutex;
        std::atomic<float> left_peak{0}, right_peak{0}, gain{0.5f};
        // Best-effort monitoring: neither callback waits for the other thread.
        // A busy buffer drops capture frames or leaves output silent.
        alia::output_stream<alia::vec2f> monitor(audio, {}, [&](std::span<alia::vec2f> output) {
            std::size_t count = 0;
            {
                std::unique_lock lock(buffer_mutex, std::try_to_lock);
                if (lock.owns_lock()) count = frames.pop(output);
            }
            const float level = gain.load(std::memory_order_relaxed);
            for (auto &frame : output.first(count)) frame *= level;
        });
        alia::input_stream<alia::vec2f> microphone(audio, {.sample_rate = monitor.sample_rate()},
            [&](std::span<const alia::vec2f> input) {
                float left = 0, right = 0;
                for (auto frame : input) {
                    left = (std::max)(left, std::abs(frame.x));
                    right = (std::max)(right, std::abs(frame.y));
                }
                left_peak.store(left, std::memory_order_relaxed);
                right_peak.store(right, std::memory_order_relaxed);
                std::unique_lock lock(buffer_mutex, std::try_to_lock);
                if (lock.owns_lock()) frames.push(input);
            });
        microphone.start();
        monitor.start();
        bool running = true;
        while (running) {
            win.poll();
            while (!events.empty()) {
                auto event = events.pop();
                if (event.get_if<alia::window_close_event>()) running = false;
                else if (auto *resize = event.get_if<alia::window_resize_event>()) swapchain.on_resize(resize->new_size);
                else if (auto *key = event.get_if<alia::window_key_down_event>()) {
                    if (key->key == alia::key::escape) running = false;
                    else if (key->key == alia::key::numpad_add) gain.store((std::min)(4.0f, gain.load() * 1.1f));
                    else if (key->key == alia::key::numpad_subtract) gain.store(gain.load() / 1.1f);
                }
            }
            if (microphone.state() == alia::audio_stream_state::failed || monitor.state() == alia::audio_stream_state::failed)
                throw std::runtime_error("microphone/monitor stream failed (device lost or callback error)");
            const float left = left_peak.load(std::memory_order_relaxed), right = right_peak.load(std::memory_order_relaxed);
            auto frame = swapchain.begin_frame();
            frame.clear(alia::black);
            frame.set_projection(graphics.ortho_ui(win.size()));
            const auto meter = alia::rect_f::pos_size({20, 100}, {600, 32});
            const auto other = meter.translated({0, 40});
            renderer.fill_rect(frame, meter, alia::color::from_rgb_u32(0x141414));
            renderer.fill_rect(frame, other, alia::color::from_rgb_u32(0x141414));
            renderer.fill_rect(frame, meter.scaled({(std::min)(left, 1.0f), 1}, meter.p1), alia::green);
            renderer.fill_rect(frame, other.scaled({(std::min)(right, 1.0f), 1}, other.p1), alia::green);
            alia::draw_text({.target = frame, .glyphs = glyphs,
                .text = std::format("L {:.1f} dB | R {:.1f} dB | monitor gain {:.2f}",
                    20 * std::log10((std::max)(left, 1e-9f)), 20 * std::log10((std::max)(right, 1e-9f)), gain.load()),
                .texture_slot = 0, .position = {20, 40}});
            frame.present();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        microphone.stop();
        monitor.stop();
    } catch (const std::exception &error) {
        std::cerr << "microphone: " << error.what() << '\n';
        return 1;
    }
}
