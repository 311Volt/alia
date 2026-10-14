#include "alia/audio/audio.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/os/window.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>
#include <thread>

int main() {
    try {
        auto audio = alia::audio_context::create();
        alia::window win({640, 240}, {.title = "ALIA tone: numpad +/- pitch, Escape quits"});
        auto graphics = alia::gfx_device::create();
        auto swapchain = graphics.create_swapchain({.target = win});
        alia::event_queue events;
        events.register_source(&win.get_event_source());
        std::atomic<float> pitch{440.0f};
        const auto endpoint = audio.default_output_device();
        if (!endpoint) throw std::runtime_error("no default audio output device");
        const int rate = endpoint->preferred.sample_rate;
        double phase = 0;
        alia::output_stream<alia::vec2f> tone(audio, {.device = endpoint, .sample_rate = rate},
            [&](std::span<alia::vec2f> frames) {
                const double step = 2 * std::numbers::pi * pitch.load(std::memory_order_relaxed) / rate;
                for (auto &frame : frames) {
                    const float point = static_cast<float>(std::sin(phase)) * 0.15f;
                    frame = {point, point};
                    phase += step;
                    if (phase >= 2 * std::numbers::pi) phase -= 2 * std::numbers::pi;
                }
            });
        tone.start();
        bool running = true;
        while (running) {
            win.poll();
            while (!events.empty()) {
                auto event = events.pop();
                if (event.get_if<alia::window_close_event>()) running = false;
                else if (auto *resize = event.get_if<alia::window_resize_event>()) swapchain.on_resize(resize->new_size);
                else if (auto *key = event.get_if<alia::window_key_down_event>()) {
                    if (key->key == alia::key::escape) running = false;
                    else if (key->key == alia::key::numpad_add) pitch.store((std::min)(8000.0f, pitch.load() * 1.0594631f));
                    else if (key->key == alia::key::numpad_subtract) pitch.store((std::max)(20.0f, pitch.load() / 1.0594631f));
                }
            }
            if (tone.state() == alia::audio_stream_state::failed) throw std::runtime_error("tone stream failed (device lost or callback error)");
            auto frame = swapchain.begin_frame();
            frame.clear(alia::color{pitch.load() / 10000.0f, 0.08f, 0.2f, 1.0f});
            frame.present();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        tone.stop();
    } catch (const std::exception &error) {
        std::cerr << "tone: " << error.what() << '\n';
        return 1;
    }
}
