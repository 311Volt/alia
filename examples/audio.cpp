/* Streamed music and an in-memory sound effect, each with its own output stream.
 * Supply ./resources/audio/spring_in_my_step.ogg and ./resources/audio/uuhhh.ogg
 * as described in resources/audio/README.md. No mixer or resampling is used. */
#include "alia/audio/audio.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/os/window.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        alia::window win({640, 480}, {.title = "ALIA audio example"});
        alia::gfx_device device = alia::gfx_device::create();
        auto swapchain = device.create_swapchain({.target = win});

        alia::audio_context audio = alia::audio_context::create();
        alia::audio_player music(audio, alia::open_audio_reader("./resources/audio/spring_in_my_step.ogg"),
                                 {.gain = 0.6f, .mode = alia::play_mode::loop});
        music.play();
        alia::audio_clip sfx_clip = alia::load_audio_clip("./resources/audio/uuhhh.ogg");
        alia::audio_player sfx(audio, sfx_clip.view());

        alia::ttf_font font = alia::load_ttf_font("./resources/roboto.ttf", 36);
        alia::hardware_glyph_buffer glyphs(device, font);

        alia::event_queue events;
        events.register_source(&win.get_event_source());
        bool running = true;
        while (running) {
            win.poll();
            while (!events.empty()) {
                auto ev = events.pop();
                if (ev.get_if<alia::window_close_event>())
                    running = false;
                else if (auto *resize = ev.get_if<alia::window_resize_event>())
                    swapchain.on_resize(resize->new_size);
                else if (auto *key = ev.get_if<alia::window_key_down_event>()) {
                    if (key->key == alia::key::escape)
                        running = false;
                    else if (key->key == alia::key::F)
                        sfx.restart();
                }
            }

            if (music.state() == alia::audio_player_state::failed)
                music.play(); // Rethrow the stored decoder/device error.
            if (sfx.state() == alia::audio_player_state::failed)
                sfx.play();

            auto frame = swapchain.begin_frame();
            frame.clear(alia::blue);
            alia::draw_text({
                .target = frame,
                .glyphs = glyphs,
                .text = "press F to play the sound",
                .texture_slot = 0,
                .position = {100.0f, 100.0f}
            });
            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "audio example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}

