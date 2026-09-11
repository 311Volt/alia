/* Type-safe stereo recording, streamed playback, DSP, and peak metering.
 * Not implementable yet: alia has no audio recording, streaming, or ring-buffer
 * subsystem. The program below is written against a hypothetical API; its
 * shape is intentionally kept here so that the proposed interface can be
 * reviewed. */
#if 0
#include "alia/audio/audio.hpp"
#include "alia/audio/ring_buffer.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/primitive_renderer.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/os/window.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <format>
#include <numbers>
#include <span>
#include <thread>
#include <vector>

float db(float value) {
    return 20.0f * std::log10((std::max)(value, 1.0e-9f));
}

struct audio_meter {
    std::atomic<float> left_peak = 0.0f;
    std::atomic<float> right_peak = 0.0f;

    void consume(std::span<const alia::vec2f> input) {
        float left = 0.0f;
        float right = 0.0f;
        for (alia::vec2f sample : input) {
            left = (std::max)(left, std::abs(sample.x));
            right = (std::max)(right, std::abs(sample.y));
        }
        left_peak = left;
        right_peak = right;
    }
};

struct ring_modulator {
    alia::ring_buffer<alia::vec2f> samples{32768};
    std::int64_t samples_processed = 0;
    std::size_t cooldown = 0;

    void consume(std::span<const alia::vec2f> input) {
        std::vector<alia::vec2f> output(input.size());
        for (std::size_t i = 0; i < input.size(); ++i) {
            const double seconds = double(samples_processed++) / 44100.0;
            const float carrier = float(std::sin(
                2.0 * std::numbers::pi * seconds * 440.0));
            output[i] = input[i] * carrier;
        }
        samples.push(output);
    }

    bool request(std::span<alia::vec2f> output) {
        if (samples.size() < cooldown)
            return false;
        cooldown = 0;
        if (samples.pop_into(output))
            return true;
        cooldown = 2 * output.size();
        return false;
    }
};

int main() {
    alia::window win({640, 480}, {.title = "ALIA microphone example"});
    alia::gfx_device device = alia::gfx_device::create();
    alia::make_current(device);
    auto swapchain = device.create_swapchain({.target = win});
    alia::audio_device audio = alia::audio_device::create();

    // HYPOTHETICAL alia API: recorder and stream exchange typed stereo chunks.
    alia::audio_recorder<float, alia::stereo> recorder(
        audio, {.sample_rate = 44100, .chunks = 12, .frames_per_chunk = 1024});
    alia::audio_stream<float, alia::stereo> stream(
        audio, {.sample_rate = 44100, .chunks = 3, .frames_per_chunk = 1024});
    audio.default_mixer().attach(stream);

    audio_meter meter;
    ring_modulator modulator;
    std::atomic<double> playback_gain = 1.0;

    std::jthread audio_thread([&](std::stop_token stop) {
        alia::event_queue audio_events;
        audio_events.register_source(&recorder.get_event_source());
        audio_events.register_source(&stream.get_event_source());
        recorder.start();
        stream.play();
        while (!stop.stop_requested()) {
            if (!audio_events.wait_for_event(0.5))
                continue;
            auto ev = audio_events.pop();
            if (auto *chunk = ev.get_if<alia::audio_recorder_chunk_event>()) {
                modulator.consume(chunk->samples);
                meter.consume(chunk->samples);
            } else if (auto *chunk = ev.get_if<alia::audio_stream_chunk_event>()) {
                if (modulator.request(chunk->samples)) {
                    const float gain = float(playback_gain.load());
                    for (auto &sample : chunk->samples)
                        sample *= gain;
                }
            }
        }
        stream.stop();
        recorder.stop();
    });

    alia::ttf_font font = alia::load_ttf_font("./resources/roboto.ttf", 16);
    alia::hardware_glyph_buffer glyphs(device, font);
    alia::basic_effect prim_fx;
    alia::basic_effect text_fx{.texture_op = alia::texture_operation::alpha_mask};
    auto prim_pipeline = alia::pipeline::create<alia::colored_vertex>(
        device, {.effect = &prim_fx});
    auto text_pipeline = alia::pipeline::create<alia::full_vertex>(
        device, {.effect = &text_fx});
    alia::immediate_primitive_renderer renderer;
    alia::event_queue events;
    events.register_source(&win.get_event_source());

    const alia::rect_f meter_left = alia::rect_f::pos_size({20, 100}, {400, 32});
    const alia::rect_f meter_right = meter_left.translated({0, 40});
    bool running = true;
    while (running) {
        win.poll();
        while (!events.empty()) {
            auto ev = events.pop();
            if (ev.get_if<alia::window_close_event>())
                running = false;
            else if (auto *resize = ev.get_if<alia::window_resize_event>())
                swapchain.on_resize(resize->new_size);
            else if (auto *key = ev.get_if<alia::window_key_char_event>()) {
                if (key->key == alia::key::numpad_add)
                    playback_gain = playback_gain.load() * 1.1;
                else if (key->key == alia::key::numpad_subtract)
                    playback_gain = playback_gain.load() / 1.1;
            }
        }

        const float left = meter.left_peak.load();
        const float right = meter.right_peak.load();
        auto frame = swapchain.begin_frame();
        frame.clear(alia::black);
        prim_fx.projection = alia::transform::ortho_ui(frame.target_size());
        frame.set_pipeline(prim_pipeline);
        renderer.fill_rect(frame, meter_left, alia::color::from_rgb_u32(0x141414));
        renderer.fill_rect(frame, meter_right, alia::color::from_rgb_u32(0x141414));
        renderer.fill_rect(frame, meter_left.scaled({left, 1}, meter_left.p1), alia::green);
        renderer.fill_rect(frame, meter_right.scaled({right, 1}, meter_right.p1), alia::green);

        text_fx.projection = alia::transform::ortho_ui(frame.target_size());
        frame.set_pipeline(text_pipeline);
        alia::draw_text(frame, {50, 50}, glyphs,
            std::format("peaks: L {:.1f} dB | R {:.1f} dB", db(left), db(right)));
        alia::draw_text(frame, {50, 75}, glyphs,
            std::format("playback gain: {:.1f} dB", db(float(playback_gain.load()))));
        frame.present();
    }
}
#endif

#include <iostream>

int main() {
    std::cout << "microphone example: requires an alia audio recording and streaming subsystem, which does not exist yet. See the #if 0 block in examples/microphone.cpp\n";
    return 0;
}
