/* Video decoding, frame events, synchronized audio, and texture presentation.
 * Not implementable yet: alia has no video or audio subsystem. The program
 * below is written against a hypothetical API; its shape is intentionally
 * kept here so that the proposed interface can be reviewed. The expected
 * video asset is ./resources/cssd.ogv. */
#if 0
#include "alia/audio/audio.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/os/window.hpp"
#include "alia/video/video.hpp"

#include <array>
#include <format>

std::array<alia::uv_vertex, 6> textured_quad(alia::rect_f dst) {
    return {{
        {{dst.left(), dst.top()}, {0, 0}}, {{dst.right(), dst.top()}, {1, 0}},
        {{dst.left(), dst.bottom()}, {0, 1}}, {{dst.right(), dst.top()}, {1, 0}},
        {{dst.right(), dst.bottom()}, {1, 1}}, {{dst.left(), dst.bottom()}, {0, 1}},
    }};
}

int main() {
    alia::window win({1024, 768}, {.title = "ALIA video example"});
    alia::gfx_device device = alia::gfx_device::create();
    alia::make_current(device);
    auto swapchain = device.create_swapchain({.target = win});
    alia::audio_device audio = alia::audio_device::create();

    // HYPOTHETICAL alia API: video emits frame-ready and finished events.
    alia::video vid = alia::load_video("./resources/cssd.ogv");
    vid.start(audio.default_mixer());

    alia::event_queue events;
    events.register_source(&win.get_event_source());
    events.register_source(&vid.get_event_source());

    alia::ttf_font font = alia::load_ttf_font("./resources/roboto.ttf", 18);
    alia::hardware_glyph_buffer glyphs(device, font);
    alia::basic_effect video_fx{.texture_op = alia::texture_operation::replace};
    alia::basic_effect text_fx{.texture_op = alia::texture_operation::alpha_mask};
    auto video_pipeline = alia::pipeline::create<alia::uv_vertex>(device, {.effect = &video_fx});
    auto text_pipeline = alia::pipeline::create<alia::full_vertex>(device, {.effect = &text_fx});

    bool running = true;
    bool redraw = true;
    while (running) {
        events.wait_for_event();
        while (!events.empty()) {
            auto ev = events.pop();
            if (ev.get_if<alia::window_close_event>() || ev.get_if<alia::video_finished_event>())
                running = false;
            else if (ev.get_if<alia::video_frame_event>())
                redraw = true;
            else if (auto *resize = ev.get_if<alia::window_resize_event>()) {
                swapchain.on_resize(resize->new_size);
                redraw = true;
            } else if (auto *key = ev.get_if<alia::window_key_down_event>();
                       key && key->key == alia::key::escape) {
                running = false;
            }
        }
        if (!running || !redraw)
            continue;
        redraw = false;

        auto frame = swapchain.begin_frame();
        frame.clear(alia::black);
        video_fx.projection = alia::transform::ortho_ui(frame.target_size());
        frame.set_pipeline(video_pipeline);
        frame.set_texture(0, vid.current_frame());
        const auto quad = textured_quad(alia::rect_f::pos_size(
            {0, 0}, alia::vec2f(vid.current_frame().size())));
        frame.draw<alia::uv_vertex>(quad);
        text_fx.projection = alia::transform::ortho_ui(frame.target_size());
        frame.set_pipeline(text_pipeline);
        alia::draw_text(
            frame,
            alia::vec2f(vid.current_frame().size()),
            glyphs,
            std::format("position: {:.2f} secs", vid.position()),
            alia::pure_green);
        frame.present();
    }
}
#endif

#include <iostream>

int main() {
    std::cout << "video example: requires alia video and audio subsystems, which do not exist yet. See the #if 0 block in examples/video.cpp\n";
    return 0;
}
