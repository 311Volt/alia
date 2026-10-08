/* Streamed music, a triggered sound effect, and a simple text prompt.
 * Not implementable yet: alia has no audio subsystem. The program below is
 * written against a hypothetical API; its shape is intentionally kept here
 * so that the proposed interface can be reviewed. The expected assets are
 * ./resources/audio/spring_in_my_step.ogg and ./resources/audio/uuhhh.ogg. */
#if 0
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

        // HYPOTHETICAL alia API: decoded streams and samples share a mixer.
        alia::audio_device audio = alia::audio_device::create();
        alia::audio_stream music = alia::load_audio_stream(
            "./resources/audio/spring_in_my_step.ogg");
        audio.default_mixer().attach(music);
        music.set_gain(0.6f);
        music.set_play_mode(alia::play_mode::loop);
        music.play();
        alia::sample sfx = alia::load_sample("./resources/audio/uuhhh.ogg");

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
                        audio.play(sfx);
                }
            }

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
#endif

#include <iostream>

int main() {
    std::cout << "audio example: requires an alia audio subsystem, which does not exist yet. See the #if 0 block in examples/audio.cpp\n";
    return 0;
}
