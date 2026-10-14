#include <alia/os/os_context.hpp>
#include <alia/os/clipboard.hpp>
#include <alia/events/event_queue.hpp>
#include <chrono>
#include <clocale>
#include <iostream>
#include <string_view>
#include <thread>

int main(int argc, char **argv) try {
    std::setlocale(LC_CTYPE, "");
    auto preferred = alia::os_backend::auto_;
    if (argc > 1) {
        const std::string_view backend(argv[1]);
        if (backend == "x11") preferred = alia::os_backend::x11;
        else if (backend == "win32") preferred = alia::os_backend::win32;
        else throw std::invalid_argument("expected x11 or win32");
    }
    auto context = alia::os_context::create(preferred);
    for (const auto &monitor : context.monitors())
        std::cout << monitor.index << ": " << monitor.name << ' ' << monitor.size.x << 'x' << monitor.size.y << '\n';
    alia::window win(context, {640, 400}, {.title = "OS events: Esc closes, F11 fullscreen, C copies, V pastes", .resizable = true, .generate_expose_events = true});
    alia::event_queue events; events.register_source(&win.get_event_source());
    std::cout << "Type to print Unicode codepoints; resize and move the window.\n";
    bool running = true;
    while (running) {
        context.poll();
        while (!events.empty()) {
            auto event = events.pop();
            if (event.get_if<alia::window_close_event>()) running = false;
            if (auto *resize = event.get_if<alia::window_resize_event>()) std::cout << "size " << resize->new_size.x << 'x' << resize->new_size.y << '\n';
            if (auto *text = event.get_if<alia::window_key_char_event>()) std::cout << "U+" << std::hex << static_cast<unsigned>(text->codepoint) << std::dec << (text->is_repeat ? " repeat\n" : "\n");
            if (auto *key = event.get_if<alia::window_key_down_event>()) {
                try {
                    if (key->key == alia::key::escape) running = false;
                    if (key->key == alia::key::f11) win.set_fullscreen_window(!win.is_fullscreen_window());
                    if (key->key == alia::key::C) (void)alia::clipboard::set_text(context, "ALIA clipboard: UTF-8 \xc5\x82\xc3\xb3\xc4\x85");
                    if (key->key == alia::key::V) std::cout << alia::clipboard::get_text(context) << '\n';
                } catch (const std::exception &error) { std::cerr << error.what() << '\n'; }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n'; return 1;
}
