#include "alia/events/event_queue.hpp"
// NOTE (API feedback): event_source::emit template definitions require this
// separate implementation header when an application defines a custom source.
#include "alia/events/event_source_impl.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/text/font.hpp"
#include "alia/os/window.hpp"

#include <deque>
#include <exception>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct example_event {
    float a;
    float b;
    std::string msg;
};

struct simple_event {
    int a;
    int b;
};

class my_event_source : public alia::event_source {
public:
    float state = 0.5f;

    void custom_emit() {
        emit(example_event{state, 3.1f, "this is some text"});
        state += 0.5f;
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
            {640, 480},
            {.title = "ALIA user events", .resizable = true});
        alia::gfx_device device = alia::gfx_device::create(requested_backend(argc, argv));
        auto swapchain = device.create_swapchain({.target = win});

        alia::ttf_font font = alia::load_ttf_font("./resources/roboto.ttf", 16);
        alia::hardware_glyph_buffer glyphs(device, font);
        alia::basic_effect text_fx{.texture_op = alia::texture_operation::alpha_mask};
        auto text_pipeline = alia::pipeline::create<alia::full_vertex>(
            device, {.effect = &text_fx});

        alia::event_queue events;
        events.register_source(&win.get_event_source());
        my_event_source custom_source;
        events.register_source(&custom_source);
        std::deque<std::string> messages;

        bool running = true;
        while (running) {
            win.poll();
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>()) {
                    running = false;
                } else if (const auto *resize = event.get_if<alia::window_resize_event>()) {
                    swapchain.on_resize(resize->new_size);
                } else if (const auto *key = event.get_if<alia::window_key_down_event>();
                           key && key->key == alia::key::escape) {
                    running = false;
                } else if (const auto *key_char = event.get_if<alia::window_key_char_event>()) {
                    // NOTE (API feedback): custom handling is manual because alia
                    // has no event dispatcher or handler-registration layer.
                    if (key_char->key == alia::key::A)
                        custom_source.custom_emit();
                    else if (key_char->key == alia::key::Z)
                        custom_source.emit(simple_event{1, 2});
                } else if (const auto *value = event.get_if<example_event>()) {
                    messages.push_back(std::format(
                        "[{:.2f}] Received example event (id={}): {:.1f} {:.1f} \"{}\"",
                        event.meta.timestamp,
                        event.meta.event_type_id,
                        value->a,
                        value->b,
                        value->msg));
                    messages.push_back(std::format(
                        "Custom event source state: {:.1f}",
                        dynamic_cast<my_event_source &>(*event.meta.source).state));
                } else if (const auto *value = event.get_if<simple_event>()) {
                    messages.push_back(std::format(
                        "simple event received: {}, {}", value->a, value->b));
                }
            }

            while (messages.size() > 25)
                messages.pop_front();

            auto frame = swapchain.begin_frame();
            frame.clear(alia::color::from_rgba8(0, 0, 60));
            text_fx.projection = device.ortho_ui(frame.target_size());
            frame.set_pipeline(text_pipeline);

            constexpr std::string_view heading = "Press A or Z to emit events";
            const float heading_width = alia::measure_text(font, heading).x;
            alia::draw_text(
                frame,
                {(static_cast<float>(frame.target_size().x) - heading_width) * 0.5f, 10.0f},
                glyphs,
                heading);

            float y = 50.0f;
            const float line_step = font.metrics().line_height + 3.0f;
            for (const std::string &line : messages) {
                alia::draw_text(frame, {50.0f, y}, glyphs, line);
                y += line_step;
            }

            frame.present();
        }
    } catch (const std::exception &error) {
        std::cerr << "userevents example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
