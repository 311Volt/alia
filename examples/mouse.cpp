#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/bitmap.hpp"
#include "alia/gfx/bitmap/pixel_types.hpp"
#include "alia/io/keyboard.hpp"
#include "alia/io/mouse.hpp"
#include "alia/os/platform.hpp"
#include "alia/os/window.hpp"

#include <array>
#include <chrono>
#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <thread>

namespace {

const char *mode_name(alia::mouse_mode mode) {
    switch (mode) {
    case alia::mouse_mode::confined: return "confined";
    case alia::mouse_mode::relative: return "relative";
    default: return "normal";
    }
}

alia::bitmap make_cursor_bitmap() {
    alia::bitmap image({32, 32}, alia::px_rgba8888{0, 0, 0, 0});
    auto pixels = image.view_as<alia::px_rgba8888>();
    for (int i = 0; i < 32; ++i) {
        pixels[i, 15] = {255, 220, 40, 255};
        pixels[15, i] = {255, 220, 40, 255};
    }
    for (int y = 12; y <= 18; ++y)
        for (int x = 12; x <= 18; ++x)
            pixels[x, y] = {255, 80, 40, 255};
    return image;
}

} // namespace

int main() {
    try {
        alia::window window({760, 260}, {.title = "ALIA mouse example"});
        auto cursor_bitmap = make_cursor_bitmap();
        alia::mouse_cursor custom_cursor(cursor_bitmap.view(), {15, 15});

        alia::event_queue events;
        events.register_source(&window.get_event_source());
        events.register_source(&alia::get_platform_event_source());

        constexpr std::array cursors{
            alia::system_mouse_cursor::arrow,
            alia::system_mouse_cursor::busy,
            alia::system_mouse_cursor::question,
            alia::system_mouse_cursor::edit,
            alia::system_mouse_cursor::move,
            alia::system_mouse_cursor::resize_north,
            alia::system_mouse_cursor::resize_south,
            alia::system_mouse_cursor::resize_northeast,
            alia::system_mouse_cursor::resize_northwest,
            alia::system_mouse_cursor::resize_east,
            alia::system_mouse_cursor::resize_west,
            alia::system_mouse_cursor::resize_southeast,
            alia::system_mouse_cursor::resize_southwest,
            alia::system_mouse_cursor::progress,
            alia::system_mouse_cursor::precision,
            alia::system_mouse_cursor::link,
            alia::system_mouse_cursor::alternate_selection,
            alia::system_mouse_cursor::unavailable,
        };
        std::size_t cursor_index = 0;
        bool cursor_visible = true;
        bool running = true;

        std::cout
            << "1 normal, 2 confined, 3 relative, C next system cursor, "
               "V custom cursor, H visibility, Z/W wheel reset, Esc quit\n";
        while (running) {
            window.poll();
            while (!events.empty()) {
                const auto event = events.pop();
                if (event.get_if<alia::window_close_event>()) {
                    running = false;
                } else if (const auto *key =
                               event.get_if<alia::window_key_down_event>()) {
                    switch (key->key) {
                    case alia::key::escape: running = false; break;
                    case alia::key::num1:
                        window.set_mouse_mode(alia::mouse_mode::normal);
                        break;
                    case alia::key::num2:
                        window.set_mouse_mode(alia::mouse_mode::confined);
                        break;
                    case alia::key::num3:
                        window.set_mouse_mode(alia::mouse_mode::relative);
                        break;
                    case alia::key::C:
                        cursor_index = (cursor_index + 1) % cursors.size();
                        window.set_cursor(cursors[cursor_index]);
                        break;
                    case alia::key::V:
                        window.set_cursor(custom_cursor);
                        break;
                    case alia::key::H:
                        cursor_visible = !cursor_visible;
                        window.show_cursor(cursor_visible);
                        break;
                    case alia::key::Z: alia::set_mouse_z(0); break;
                    case alia::key::W: alia::set_mouse_w(0); break;
                    default: break;
                    }
                } else if (const auto *relative =
                               event.get_if<alia::mouse_relative_event>()) {
                    std::cout << "relative " << relative->delta.x << ", "
                              << relative->delta.y << '\n';
                } else if (const auto *axis =
                               event.get_if<alia::mouse_axis_set_event>()) {
                    std::cout << "axis " << axis->axis << " set to "
                              << axis->value << " (delta " << axis->delta
                              << ")\n";
                }
            }

            const alia::mouse_state state = alia::get_mouse_state();
            const std::string title = std::format(
                "pos {},{}  z {}  w {}  buttons 0x{:x}  pressure {:.1f}  "
                "requested {}  active {}",
                state.position().x, state.position().y, state.z(), state.w(),
                state.button_mask(), state.pressure(),
                mode_name(window.requested_mouse_mode()),
                mode_name(window.active_mouse_mode()));
            window.set_title(title.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    } catch (const std::exception &error) {
        std::cerr << "mouse example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
