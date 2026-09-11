#include "alia/events/event_queue.hpp"
#include "alia/gfx/bitmap/bitmap.hpp"
#include "alia/gfx/frame.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/pipeline.hpp"
#include "alia/gfx/transform.hpp"
#include "alia/os/display.hpp"
#include "alia/os/monitor.hpp"
#include "alia/os/window.hpp"

#include <array>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
    struct options {
        alia::gfx_backend backend = alia::gfx_backend::auto_;
        int monitor = -1;
        alia::window_fullscreen_mode mode = alia::window_fullscreen_mode::windowed;
        std::optional<alia::display_mode> fullscreen_mode;
        bool borderless = false;
        bool resizable = false;
        bool expose = false;
        alia::vsync_mode vsync = alia::vsync_mode::disable;
        alia::window_size_constraints constraints;
        alia::framebuffer_config framebuffer;
    };

    alia::display_mode parse_mode(std::string_view text) {
        const auto x = text.find_first_of("xX");
        if (x == std::string_view::npos)
            throw std::invalid_argument("display size must be WxH or WxH@Hz");
        const auto at = text.find('@', x + 1);
        alia::display_mode result;
        result.size.x = std::stoi(std::string(text.substr(0, x)));
        result.size.y = std::stoi(std::string(text.substr(
            x + 1, at == std::string_view::npos ? at : at - x - 1)));
        result.refresh_rate = at == std::string_view::npos
            ? 0 : std::stoi(std::string(text.substr(at + 1)));
        result.color_depth = 0;
        if (result.size.x <= 0 || result.size.y <= 0)
            throw std::invalid_argument("display dimensions must be positive");
        return result;
    }

    options parse_arguments(int argc, char **argv) {
        options result;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            auto value = [&](const char *name) -> std::string_view {
                if (++i >= argc)
                    throw std::invalid_argument(std::string(name) + " needs a value");
                return argv[i];
            };
            if (arg == "d3d9") result.backend = alia::gfx_backend::d3d9;
            else if (arg == "opengl") result.backend = alia::gfx_backend::opengl;
            else if (arg == "--monitor") result.monitor = std::stoi(std::string(value("--monitor")));
            else if (arg == "--fullscreen") {
                result.mode = alia::window_fullscreen_mode::fullscreen;
                if (i + 1 < argc && std::string_view(argv[i + 1]).starts_with("--") == false)
                    result.fullscreen_mode = parse_mode(argv[++i]);
            } else if (arg == "--fullscreen-window") result.mode = alia::window_fullscreen_mode::fullscreen_window;
            else if (arg == "--borderless") result.borderless = true;
            else if (arg == "--resizable") result.resizable = true;
            else if (arg == "--expose") result.expose = true;
            else if (arg == "--samples") result.framebuffer.samples = {std::stoi(std::string(value("--samples")))};
            else if (arg == "--depth") result.framebuffer.depth_bits = {std::stoi(std::string(value("--depth")))};
            else if (arg == "--stencil") result.framebuffer.stencil_bits = {std::stoi(std::string(value("--stencil")))};
            else if (arg == "--color") result.framebuffer.color_bits = {std::stoi(std::string(value("--color")))};
            else if (arg == "--copy-swap") result.framebuffer.swap = {alia::swap_method::copy};
            else if (arg == "--update-region") result.framebuffer.update_display_region = {true};
            else if (arg == "--vsync") result.vsync = alia::vsync_mode::require;
            else if (arg == "--min") result.constraints.min = parse_mode(value("--min")).size;
            else if (arg == "--max") result.constraints.max = parse_mode(value("--max")).size;
            else throw std::invalid_argument("unknown argument: " + std::string(arg));
        }
        return result;
    }

    const char *yes_no(bool value) { return value ? "yes" : "no"; }

    void print_monitors() {
        for (const auto &monitor : alia::get_monitors()) {
            std::cout << "monitor " << monitor.index << ": " << monitor.name
                      << " pos=" << monitor.position.x << ',' << monitor.position.y
                      << " size=" << monitor.size.x << 'x' << monitor.size.y
                      << " dpi=" << monitor.dpi
                      << " primary=" << yes_no(monitor.is_primary)
                      << " current=" << monitor.current_mode.size.x << 'x'
                      << monitor.current_mode.size.y << '@'
                      << monitor.current_mode.refresh_rate << "Hz/"
                      << monitor.current_mode.color_depth << "bpp\n";
            for (const auto &mode : monitor.modes)
                std::cout << "  " << mode.size.x << 'x' << mode.size.y << '@'
                          << mode.refresh_rate << "Hz/" << mode.color_depth << "bpp\n";
        }
    }

    void print_caps(const alia::gfx_device_caps &caps) {
        std::cout << "device caps\n"
                  << "  renderer             " << caps.renderer_name << '\n'
                  << "  render method        " << (caps.render == alia::render_method::hardware ? "hardware" : "software") << '\n'
                  << "  max texture size     " << caps.max_texture_size << '\n'
                  << "  NPOT textures        " << yes_no(caps.npot_textures) << '\n'
                  << "  render to texture    " << yes_no(caps.render_to_texture) << '\n'
                  << "  separate alpha blend " << yes_no(caps.separate_alpha_blend) << '\n';
    }

    void print_properties(const alia::framebuffer_properties &p) {
        std::cout << "swapchain properties\n"
                  << "  color RGBA bits      " << p.color_bits << " ("
                  << p.red_bits << ',' << p.green_bits << ',' << p.blue_bits << ',' << p.alpha_bits << ")\n"
                  << "  RGBA shifts          " << p.red_shift << ',' << p.green_shift << ',' << p.blue_shift << ',' << p.alpha_shift << '\n'
                  << "  depth/stencil        " << p.depth_bits << '/' << p.stencil_bits << '\n'
                  << "  sample buffers/count " << p.sample_buffers << '/' << p.samples << '\n'
                  << "  aux buffers          " << p.aux_buffers << '\n'
                  << "  accum RGBA bits      " << p.accum_red_bits << ',' << p.accum_green_bits << ',' << p.accum_blue_bits << ',' << p.accum_alpha_bits << '\n'
                  << "  float color/depth    " << yes_no(p.float_color) << '/' << yes_no(p.float_depth) << '\n'
                  << "  single buffer        " << yes_no(p.single_buffer) << '\n'
                  << "  update region        " << yes_no(p.update_display_region) << '\n'
                  << "  vsync                " << yes_no(p.vsync) << '\n'
                  << "  swap method          "
                  << (p.swap == alia::swap_method::copy ? "copy" : p.swap == alia::swap_method::flip ? "flip" : "undefined") << '\n';
    }

    alia::bitmap checker_icon(int side) {
        alia::bitmap result({side, side}, alia::px_bgra8888{});
        auto pixels = result.view_as<alia::px_bgra8888>();
        for (int y = 0; y < side; ++y) {
            for (int x = 0; x < side; ++x) {
                const bool light = ((x / (side / 4)) + (y / (side / 4))) % 2 == 0;
                pixels[x, y] = light
                    ? alia::px_bgra8888{0x50, 0xc0, 0xff, 0xff}
                    : alia::px_bgra8888{0x30, 0x30, 0x50, 0xff};
            }
        }
        return result;
    }

    void update_title(alia::window &window) {
        const char *mode = window.fullscreen_mode() == alia::window_fullscreen_mode::fullscreen
            ? "exclusive" : window.fullscreen_mode() == alia::window_fullscreen_mode::fullscreen_window
                ? "fullscreen-window" : "windowed";
        const std::string title = std::string("ALIA display config | ") + mode +
            " | monitor " + std::to_string(window.monitor()) +
            " | " + std::to_string(window.refresh_rate()) + " Hz" +
            " | resizable=" + yes_no(window.is_resizable()) +
            " borderless=" + yes_no(window.is_borderless());
        window.set_title(title.c_str());
    }

    struct screensaver_guard {
        screensaver_guard() {
#if defined(_WIN32)
            alia::inhibit_screensaver(true);
#endif
        }
        ~screensaver_guard() {
#if defined(_WIN32)
            alia::inhibit_screensaver(false);
#endif
        }
    };
}

int main(int argc, char **argv) {
    try {
        const options opts = parse_arguments(argc, argv);
        print_monitors();

        alia::vec2i initial_size{800, 600};
        if (opts.fullscreen_mode)
            initial_size = opts.fullscreen_mode->size;
        alia::window_options window_options;
        window_options.title = "ALIA display config";
        window_options.mode = opts.mode;
        window_options.resizable = opts.resizable;
        window_options.borderless = opts.borderless;
        window_options.monitor = opts.monitor;
        window_options.refresh_rate = opts.fullscreen_mode
            ? opts.fullscreen_mode->refresh_rate : 0;
        window_options.generate_expose_events = opts.expose;
        window_options.size_constraints = opts.constraints;
        alia::window window(initial_size, window_options);

        auto icon16 = checker_icon(16);
        auto icon32 = checker_icon(32);
        const std::array icons{icon16.view(), icon32.view()};
        window.set_icons(icons);
        screensaver_guard screensaver;

        alia::gfx_device_config device_config;
        device_config.adapter = opts.monitor;
        auto device = alia::gfx_device::create(opts.backend, device_config);
        alia::make_current(device);
        auto swapchain = device.create_swapchain({
            .target = window,
            .vsync = opts.vsync,
            .framebuffer = opts.framebuffer,
        });
        print_caps(device.caps());
        print_properties(swapchain.properties());

        alia::event_queue events;
        events.register_source(&window.get_event_source());
        alia::basic_effect effect;
        auto pipeline = alia::pipeline::create<alia::colored_vertex>(device, {.effect = &effect});
        const std::array triangle{
            alia::colored_vertex{{400.0f, 80.0f}, {1.0f, 0.2f, 0.15f}},
            alia::colored_vertex{{80.0f, 520.0f}, {0.15f, 0.9f, 0.3f}},
            alia::colored_vertex{{720.0f, 520.0f}, {0.15f, 0.35f, 1.0f}},
        };

        std::vector<alia::display_mode> modes = alia::get_display_modes(window.monitor());
        std::size_t mode_index = 0;
        bool running = true;
        bool partial_present = false;
        bool constraints_enabled = opts.constraints.min != alia::vec2i{} ||
            opts.constraints.max != alia::vec2i{};
        update_title(window);
        while (running) {
            window.poll();
            while (!events.empty()) {
                auto event = events.pop();
                if (event.get_if<alia::window_close_event>()) {
                    running = false;
                } else if (const auto *resize = event.get_if<alia::window_resize_event>()) {
                    if (resize->new_size.x > 0 && resize->new_size.y > 0)
                        swapchain.on_resize(resize->new_size);
                    std::cout << "resize " << resize->new_size.x << 'x' << resize->new_size.y << '\n';
                } else if (const auto *expose = event.get_if<alia::window_expose_event>()) {
                    std::cout << "expose " << expose->area.left() << ',' << expose->area.top()
                              << ' ' << expose->area.width() << 'x' << expose->area.height() << '\n';
                } else if (const auto *key = event.get_if<alia::window_key_down_event>()) {
                    switch (key->key) {
                    case alia::key::escape: running = false; break;
                    case alia::key::F:
                        if (window.fullscreen_mode() == alia::window_fullscreen_mode::fullscreen)
                            window.set_fullscreen(false);
                        else if (!modes.empty())
                            window.set_fullscreen(modes[mode_index]);
                        else
                            window.set_fullscreen(true);
                        break;
                    case alia::key::left_bracket:
                    case alia::key::right_bracket:
                        if (!modes.empty()) {
                            if (key->key == alia::key::right_bracket)
                                mode_index = (mode_index + 1) % modes.size();
                            else
                                mode_index = (mode_index + modes.size() - 1) % modes.size();
                            if (window.fullscreen_mode() == alia::window_fullscreen_mode::fullscreen)
                                window.set_fullscreen(modes[mode_index]);
                        }
                        break;
                    case alia::key::W:
                        window.set_fullscreen_window(!window.is_fullscreen_window());
                        break;
                    case alia::key::R: window.set_resizable(!window.is_resizable()); break;
                    case alia::key::B: window.set_borderless(!window.is_borderless()); break;
                    case alia::key::M:
                        if (window.is_maximized()) window.restore(); else window.maximize();
                        break;
                    case alia::key::N: window.minimize(); break;
                    case alia::key::C:
                        constraints_enabled = !constraints_enabled;
                        if (constraints_enabled)
                            window.set_size_constraints({{320, 240}, {1280, 720}});
                        window.apply_size_constraints(constraints_enabled);
                        break;
                    case alia::key::U: partial_present = true; break;
                    default: break;
                    }
                    update_title(window);
                }
            }
            if (!running) break;

            auto frame = swapchain.begin_frame();
            frame.clear(alia::color{0.08f, 0.1f, 0.16f, 1.0f});
            effect.projection = alia::transform::ortho_ui(frame.target_size());
            frame.set_pipeline(pipeline);
            frame.draw<alia::colored_vertex>(triangle);
            if (partial_present) {
                const auto size = frame.target_size();
                frame.present(alia::rect_i::pos_size({}, {size.x / 2, size.y / 2}));
                partial_present = false;
            } else {
                frame.present();
            }
        }
    } catch (const std::exception &error) {
        std::cerr << "display_config example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
