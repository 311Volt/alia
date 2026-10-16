#include "x11_ops.hpp"
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_X11
#include "../../events/event_source_impl.hpp"
#include "../../gfx/bitmap/bitmap.hpp"
#include <X11/keysym.h>
#include <algorithm>
#include <cstring>

namespace alia {
    namespace {
        key keysym_key(KeySym sym) {
            if (sym >= XK_a && sym <= XK_z)
                return static_cast<key>(static_cast<int>(key::A) + sym - XK_a);
            if (sym >= XK_A && sym <= XK_Z)
                return static_cast<key>(static_cast<int>(key::A) + sym - XK_A);
            if (sym >= XK_0 && sym <= XK_9)
                return static_cast<key>(static_cast<int>(key::num0) + sym - XK_0);
            if (sym >= XK_F1 && sym <= XK_F12)
                return static_cast<key>(static_cast<int>(key::f1) + sym - XK_F1);
            if (sym >= XK_KP_0 && sym <= XK_KP_9)
                return static_cast<key>(static_cast<int>(key::numpad0) + sym - XK_KP_0);
            switch (sym) {
            case XK_Shift_L:          return key::lshift;
            case XK_Shift_R:          return key::rshift;
            case XK_Control_L:        return key::lctrl;
            case XK_Control_R:        return key::rctrl;
            case XK_Alt_L:
            case XK_Meta_L:           return key::lalt;
            case XK_Alt_R:
            case XK_Meta_R:
            case XK_ISO_Level3_Shift: return key::ralt;
            case XK_Super_L:          return key::lsuper;
            case XK_Super_R:          return key::rsuper;
            case XK_Up:               return key::up;
            case XK_Down:             return key::down;
            case XK_Left:             return key::left;
            case XK_Right:            return key::right;
            case XK_Home:             return key::home;
            case XK_End:              return key::end;
            case XK_Prior:            return key::page_up;
            case XK_Next:             return key::page_down;
            case XK_Insert:           return key::insert;
            case XK_Delete:           return key::del;
            case XK_BackSpace:        return key::backspace;
            case XK_Tab:
            case XK_ISO_Left_Tab:     return key::tab;
            case XK_Return:           return key::enter;
            case XK_Escape:           return key::escape;
            case XK_space:            return key::space;
            case XK_minus:            return key::minus;
            case XK_equal:            return key::equals;
            case XK_bracketleft:      return key::left_bracket;
            case XK_bracketright:     return key::right_bracket;
            case XK_backslash:        return key::backslash;
            case XK_semicolon:        return key::semicolon;
            case XK_apostrophe:       return key::apostrophe;
            case XK_grave:            return key::grave;
            case XK_comma:            return key::comma;
            case XK_period:           return key::period;
            case XK_slash:            return key::slash;
            case XK_KP_Insert:        return key::numpad0;
            case XK_KP_End:           return key::numpad1;
            case XK_KP_Down:          return key::numpad2;
            case XK_KP_Next:          return key::numpad3;
            case XK_KP_Left:          return key::numpad4;
            case XK_KP_Begin:         return key::numpad5;
            case XK_KP_Right:         return key::numpad6;
            case XK_KP_Home:          return key::numpad7;
            case XK_KP_Up:            return key::numpad8;
            case XK_KP_Prior:         return key::numpad9;
            case XK_KP_Add:           return key::numpad_add;
            case XK_KP_Subtract:      return key::numpad_subtract;
            case XK_KP_Multiply:      return key::numpad_multiply;
            case XK_KP_Divide:        return key::numpad_divide;
            case XK_KP_Enter:         return key::numpad_enter;
            case XK_KP_Decimal:
            case XK_KP_Delete:        return key::numpad_decimal;
            case XK_KP_Equal:         return key::numpad_equals;
            case XK_Caps_Lock:        return key::caps_lock;
            case XK_Num_Lock:         return key::num_lock;
            case XK_Scroll_Lock:      return key::scroll_lock;
            case XK_Print:            return key::print_screen;
            case XK_Pause:            return key::pause;
            case XK_Menu:             return key::menu;
            default:                  return key::unknown;
            }
        }

        key_mod modifiers(x11_context &c, unsigned state) {
            key_mod result = key_mod::none;
            if (state & LockMask)
                result |= key_mod::caps_lock;
            if (state & c.num_lock_mask)
                result |= key_mod::num_lock;
            if (state & c.scroll_lock_mask)
                result |= key_mod::scroll_lock;
            return result;
        }

        void emit_text(x11_window &w, XKeyEvent &event, key physical, bool repeat) {
            std::string text;
            KeySym sym = NoSymbol;
            if (w.input_context) {
                std::array<char, 64> initial{};
                Status status;
                int count = Xutf8LookupString(
                    w.input_context, &event, initial.data(), static_cast<int>(initial.size()), &sym, &status
                );
                if (status == XBufferOverflow) {
                    text.resize(static_cast<std::size_t>(count));
                    count = Xutf8LookupString(w.input_context, &event, text.data(), count, &sym, &status);
                    text.resize(static_cast<std::size_t>(std::max(0, count)));
                } else if (status == XLookupChars || status == XLookupBoth)
                    text.assign(initial.data(), static_cast<std::size_t>(count));
            } else {
                // Without XIM, keysym Unicode values still preserve non-ASCII input.
                char buffer[32];
                XLookupString(&event, buffer, sizeof(buffer), &sym, nullptr);
                char32_t cp = 0;
                if (sym >= 0x20 && sym <= 0xff)
                    cp = static_cast<char32_t>(sym);
                else if ((sym & 0xff000000) == 0x01000000)
                    cp = static_cast<char32_t>(sym & 0xffffff);
                else if (sym == XK_Return || sym == XK_KP_Enter)
                    cp = U'\r';
                else if (sym == XK_Tab)
                    cp = U'\t';
                if (cp && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff))
                    w.source.emit(window_key_char_event{physical, cp, get_keyboard_state().get_modifiers(), repeat});
                return;
            }
            for (std::size_t i = 0; i < text.size();) {
                const auto lead = static_cast<unsigned char>(text[i++]);
                char32_t cp = lead;
                unsigned tails = 0;
                if (lead >= 0xc2 && lead <= 0xdf) {
                    cp = lead & 0x1f;
                    tails = 1;
                } else if (lead >= 0xe0 && lead <= 0xef) {
                    cp = lead & 0xf;
                    tails = 2;
                } else if (lead >= 0xf0 && lead <= 0xf4) {
                    cp = lead & 7;
                    tails = 3;
                } else if (lead >= 0x80)
                    continue;
                if (i + tails > text.size())
                    break;
                bool valid = true;
                for (unsigned n = 0; n < tails; ++n) {
                    auto ch = static_cast<unsigned char>(text[i++]);
                    if ((ch & 0xc0) != 0x80)
                        valid = false;
                    cp = (cp << 6) | (ch & 0x3f);
                }
                if (valid && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff) && (tails != 1 || cp >= 0x80) &&
                    (tails != 2 || cp >= 0x800) && (tails != 3 || cp >= 0x10000))
                    w.source.emit(window_key_char_event{physical, cp, get_keyboard_state().get_modifiers(), repeat});
            }
        }

        void release_buttons(x11_window &w) {
            const auto buttons = std::exchange(w.buttons, 0);
            for (unsigned n = 1; n <= 32; ++n)
                if (buttons & (std::uint32_t{1} << (n - 1)))
                    detail::mouse_handle_button(
                        w.owner, w.source, static_cast<mouse_button>(n), false, w.mouse_position
                    );
        }

        void update_state(x11_window &w) {
            auto &c = *w.context;
            Atom type;
            int format;
            unsigned long count, remaining;
            unsigned char *data = nullptr;
            if (XGetWindowProperty(
                    c.display,
                    w.xid,
                    c.atom("_NET_WM_STATE"),
                    0,
                    128,
                    False,
                    XA_ATOM,
                    &type,
                    &format,
                    &count,
                    &remaining,
                    &data
                ) == Success &&
                type == XA_ATOM && format == 32) {
                auto *atoms = reinterpret_cast<Atom *>(data);
                const auto has = [&](const char *name) {
                    const auto a = c.atom(name);
                    return std::find(atoms, atoms + count, a) != atoms + count;
                };
                w.maximized = has("_NET_WM_STATE_MAXIMIZED_HORZ") && has("_NET_WM_STATE_MAXIMIZED_VERT");
                w.minimized = has("_NET_WM_STATE_HIDDEN");
            }
            if (data)
                XFree(data);
        }
    } // namespace

    void x11_update_keymap(x11_context &c) {
        c.keymap.fill(key::unknown);
        int first = 0, last = 0, per_key = 0;
        XDisplayKeycodes(c.display, &first, &last);
        auto *symbols = XGetKeyboardMapping(c.display, static_cast<KeyCode>(first), last - first + 1, &per_key);
        if (symbols) {
            for (int code = first; code <= last && code < 256; ++code)
                c.keymap[static_cast<std::size_t>(code)] = keysym_key(symbols[(code - first) * per_key]);
            XFree(symbols);
        }
        // XKB names preserve the physical key identity on non-US layouts.
        if (c.xkb) {
            XkbDescPtr map = XkbGetMap(c.display, 0, XkbUseCoreKbd);
            if (map) {
                if (XkbGetNames(c.display, XkbKeyNamesMask, map) == Success && map->names && map->names->keys) {
                    constexpr std::string_view rows[] = {"AE", "AD", "AC", "AB"};
                    const std::vector<key> keys[] = {
                        {key::num1,
                         key::num2,
                         key::num3,
                         key::num4,
                         key::num5,
                         key::num6,
                         key::num7,
                         key::num8,
                         key::num9,
                         key::num0,
                         key::minus,
                         key::equals},
                        {key::Q,
                         key::W,
                         key::E,
                         key::R,
                         key::T,
                         key::Y,
                         key::U,
                         key::I,
                         key::O,
                         key::P,
                         key::left_bracket,
                         key::right_bracket},
                        {key::A,
                         key::S,
                         key::D,
                         key::F,
                         key::G,
                         key::H,
                         key::J,
                         key::K,
                         key::L,
                         key::semicolon,
                         key::apostrophe},
                        {key::Z, key::X, key::C, key::V, key::B, key::N, key::M, key::comma, key::period, key::slash}
                    };
                    for (int code = map->min_key_code; code <= map->max_key_code && code < 256; ++code) {
                        const std::string_view name(map->names->keys[code].name, XkbKeyNameLength);
                        for (int row = 0; row < 4; ++row) {
                            if (name.substr(0, 2) != rows[row])
                                continue;
                            const int index = (name[2] - '0') * 10 + name[3] - '0' - 1;
                            if (index >= 0 && index < static_cast<int>(keys[row].size()))
                                c.keymap[static_cast<std::size_t>(code)] = keys[row][static_cast<std::size_t>(index)];
                        }
                        if (name == "LSGT")
                            c.keymap[static_cast<std::size_t>(code)] = key::oem_102;
                        else if (name == "TLDE")
                            c.keymap[static_cast<std::size_t>(code)] = key::grave;
                        else if (name == "BKSL")
                            c.keymap[static_cast<std::size_t>(code)] = key::backslash;
                    }
                }
                XkbFreeKeyboard(map, XkbAllComponentsMask, True);
            }
            c.num_lock_mask = XkbKeysymToModifiers(c.display, XK_Num_Lock);
            c.scroll_lock_mask = XkbKeysymToModifiers(c.display, XK_Scroll_Lock);
        }
    }

    x11_window::~x11_window() {
        if (!context)
            return;
        if (context->mouse_owner == this)
            x11_ungrab_mouse(context);
        release_buttons(*this);
        x11_restore_display(*this);
        if (input_context)
            XDestroyIC(input_context);
        if (cursor)
            XFreeCursor(context->display, cursor);
        if (hidden_cursor)
            XFreeCursor(context->display, hidden_cursor);
        if (xid) {
            context->windows.erase(xid);
            XDestroyWindow(context->display, xid);
        }
        XFlush(context->display);
    }

    bool x11_supports(const x11_context &c, const char *name) {
        Atom type;
        int format;
        unsigned long count, remaining;
        unsigned char *data = nullptr;
        bool supported = false;
        if (XGetWindowProperty(
                c.display,
                c.root,
                c.atom("_NET_SUPPORTED"),
                0,
                4096,
                False,
                XA_ATOM,
                &type,
                &format,
                &count,
                &remaining,
                &data
            ) == Success &&
            type == XA_ATOM && format == 32) {
            auto *values = reinterpret_cast<Atom *>(data);
            const auto atom = c.atom(name);
            supported = std::find(values, values + count, atom) != values + count;
        }
        if (data)
            XFree(data);
        return supported;
    }

    void x11_state(x11_window &w, bool add, const char *first, const char *second) {
        auto &c = *w.context;
        if (!x11_supports(c, first))
            throw unsupported_operation_exception(std::string("X11 window manager does not support ") + first);
        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.window = w.xid;
        event.xclient.message_type = c.atom("_NET_WM_STATE");
        event.xclient.format = 32;
        event.xclient.data.l[0] = add ? 1 : 0;
        event.xclient.data.l[1] = static_cast<long>(c.atom(first));
        event.xclient.data.l[2] = second ? static_cast<long>(c.atom(second)) : 0;
        event.xclient.data.l[3] = 1;
        XSendEvent(c.display, c.root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
        XFlush(c.display);
    }

    void x11_set_title(x11_window &w, cstring_view title) {
        w.title = title.data();
        auto &c = *w.context;
        XChangeProperty(
            c.display,
            w.xid,
            c.atom("_NET_WM_NAME"),
            c.utf8,
            8,
            PropModeReplace,
            reinterpret_cast<const unsigned char *>(w.title.data()),
            static_cast<int>(w.title.size())
        );
        XStoreName(c.display, w.xid, w.title.c_str());
        XFlush(c.display);
    }

    void x11_set_position(x11_window &w, vec2i position) {
        XMoveWindow(w.context->display, w.xid, position.x, position.y);
        XFlush(w.context->display);
    }

    void x11_resize(x11_window &w, vec2i size) {
        if (size.x > 65535 || size.y > 65535)
            throw std::invalid_argument("X11 window dimensions exceed the protocol limit");
        if (!w.resizable) {
            w.fixed_size = size;
            x11_update_hints(w);
            size = w.fixed_size;
        }
        XResizeWindow(w.context->display, w.xid, static_cast<unsigned>(size.x), static_cast<unsigned>(size.y));
        XFlush(w.context->display);
    }

    void x11_update_hints(x11_window &w) {
        XSizeHints hints{};
        if (w.mode == window_fullscreen_mode::windowed) {
            vec2i target = w.resizable ? w.client_size : w.fixed_size;
            if (w.constraints_enabled) {
                target.x = std::max(target.x, w.constraints.min.x);
                target.y = std::max(target.y, w.constraints.min.y);
                if (w.constraints.max.x)
                    target.x = std::min(target.x, w.constraints.max.x);
                if (w.constraints.max.y)
                    target.y = std::min(target.y, w.constraints.max.y);
            }
            if (!w.resizable)
                w.fixed_size = target;
            if (!w.resizable) {
                hints.flags = PMinSize | PMaxSize;
                hints.min_width = hints.max_width = target.x;
                hints.min_height = hints.max_height = target.y;
            } else if (w.constraints_enabled) {
                if (w.constraints.min.x || w.constraints.min.y) {
                    hints.flags |= PMinSize;
                    hints.min_width = std::max(1, w.constraints.min.x);
                    hints.min_height = std::max(1, w.constraints.min.y);
                }
                if (w.constraints.max.x || w.constraints.max.y) {
                    hints.flags |= PMaxSize;
                    hints.max_width = w.constraints.max.x ? w.constraints.max.x : 65535;
                    hints.max_height = w.constraints.max.y ? w.constraints.max.y : 65535;
                }
            }
            if (target != w.client_size)
                XResizeWindow(
                    w.context->display, w.xid, static_cast<unsigned>(target.x), static_cast<unsigned>(target.y)
                );
        }
        XSetWMNormalHints(w.context->display, w.xid, &hints);
        XFlush(w.context->display);
    }

    void x11_validate_constraints(window_size_constraints value) {
        if (value.min.x > 65535 || value.min.y > 65535)
            throw std::invalid_argument("X11 minimum window dimensions exceed the protocol limit");
    }

    void x11_set_borderless(x11_window &w, bool value) {
        // Motif decoration hints are understood by the usual ICCCM/EWMH managers.
        struct motif_hints {
            unsigned long flags, functions, decorations;
            long input_mode;
            unsigned long status;
        } hints{2, 0, value ? 0ul : 1ul, 0, 0};

        w.borderless = value;
        auto &c = *w.context;
        XChangeProperty(
            c.display,
            w.xid,
            c.atom("_MOTIF_WM_HINTS"),
            c.atom("_MOTIF_WM_HINTS"),
            32,
            PropModeReplace,
            reinterpret_cast<const unsigned char *>(&hints),
            5
        );
        XFlush(c.display);
    }

    os_window_handle *x11_create_window(os_context_handle *h, vec2i size, const window_options &options) {
        if (size.x > 65535 || size.y > 65535)
            throw std::invalid_argument("X11 window dimensions exceed the protocol limit");
        x11_validate_constraints(options.size_constraints);
        auto &c = *as_x11_context(h);
        auto result = std::make_unique<x11_window>();
        auto &w = *result;
        w.context = &c;
        w.client_size = w.fixed_size = size;
        w.resizable = options.resizable;
        w.expose = options.generate_expose_events;
        w.constraints = options.size_constraints;
        w.preferred_monitor = options.monitor;
        const auto monitors = x11_monitors(&c);
        const int index = options.monitor >= 0 ? options.monitor : 0;
        if (index >= static_cast<int>(monitors.size()))
            throw std::out_of_range("X11 monitor index out of range");
        const auto &m = monitors[static_cast<std::size_t>(index)];
        w.client_position = m.position + (m.size - size) / 2;
        if (auto *p = std::get_if<vec2i>(&options.position))
            w.client_position = *p;
        w.xid = XCreateSimpleWindow(
            c.display,
            c.root,
            w.client_position.x,
            w.client_position.y,
            static_cast<unsigned>(size.x),
            static_cast<unsigned>(size.y),
            0,
            0,
            0
        );
        if (!w.xid)
            return nullptr;
        c.windows.emplace(w.xid, &w);
        XSetWMProtocols(c.display, w.xid, &c.wm_delete, 1);
        XClassHint class_hint{const_cast<char *>("alia"), const_cast<char *>("Alia")};
        XSetClassHint(c.display, w.xid, &class_hint);
        XWMHints wm{};
        wm.flags = InputHint | StateHint;
        wm.input = True;
        wm.initial_state = options.minimized ? IconicState : NormalState;
        XSetWMHints(c.display, w.xid, &wm);
        long event_mask = StructureNotifyMask | ExposureMask | FocusChangeMask | KeyPressMask | KeyReleaseMask |
                          ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask |
                          PropertyChangeMask;
        if (c.input_method) {
            w.input_context = XCreateIC(
                c.input_method,
                XNInputStyle,
                XIMPreeditNothing | XIMStatusNothing,
                XNClientWindow,
                w.xid,
                XNFocusWindow,
                w.xid,
                nullptr
            );
            if (w.input_context) {
                long filter = 0;
                XGetICValues(w.input_context, XNFilterEvents, &filter, nullptr);
                event_mask |= filter;
            }
        }
        XSelectInput(c.display, w.xid, event_mask);
        x11_set_title(w, options.title);
        x11_set_borderless(w, options.borderless);
        x11_update_hints(w);
        if (options.maximized) {
            if (!x11_supports(c, "_NET_WM_STATE_MAXIMIZED_HORZ"))
                throw unsupported_operation_exception("X11 window manager does not support maximizing");
            const Atom state[] = {c.atom("_NET_WM_STATE_MAXIMIZED_HORZ"), c.atom("_NET_WM_STATE_MAXIMIZED_VERT")};
            XChangeProperty(
                c.display,
                w.xid,
                c.atom("_NET_WM_STATE"),
                XA_ATOM,
                32,
                PropModeReplace,
                reinterpret_cast<const unsigned char *>(state),
                2
            );
        }
        XMapWindow(c.display, w.xid);
        if (!x11_set_mouse_mode(&w, options.mouse_mode))
            throw std::runtime_error("X11: requested mouse mode is unavailable");
        XFlush(c.display);
        return result.release();
    }

    void x11_set_fullscreen(os_window_handle *h, window_fullscreen_mode requested, const display_mode *mode) {
        auto &w = *as_x11_window(h);
        auto &c = *w.context;
        if (!x11_supports(c, "_NET_WM_STATE_FULLSCREEN"))
            throw unsupported_operation_exception("X11 window manager has no EWMH fullscreen support");
        if (requested == w.mode &&
            (requested != window_fullscreen_mode::fullscreen || (mode && *mode == w.exclusive_mode)))
            return;
        if (requested == window_fullscreen_mode::fullscreen && !mode)
            throw std::invalid_argument("exclusive fullscreen requires an exact mode");
        const auto previous = w.mode;
        if (previous == window_fullscreen_mode::windowed && requested != previous) {
            w.saved_position = w.client_position;
            w.saved_size = w.client_size;
        }
        x11_restore_display(w);
        w.display_mode_suspended = false;
        w.saved_mode = None;
        w.exclusive_crtc = None;
        w.saved_outputs.clear();
        if (requested == window_fullscreen_mode::fullscreen) {
            try {
                x11_apply_display(w, *mode);
            } catch (...) {
                w.mode = window_fullscreen_mode::windowed;
                x11_state(w, false, "_NET_WM_STATE_FULLSCREEN");
                x11_update_hints(w);
                if (previous != window_fullscreen_mode::windowed) {
                    x11_resize(w, w.saved_size);
                    x11_set_position(w, w.saved_position);
                }
                throw;
            }
        }
        w.mode = requested;
        x11_update_hints(w);
        if (requested == window_fullscreen_mode::windowed) {
            x11_state(w, false, "_NET_WM_STATE_FULLSCREEN");
            x11_resize(w, w.saved_size);
            x11_set_position(w, w.saved_position);
        } else {
            const auto monitors = x11_monitors(&c);
            const int index = w.preferred_monitor >= 0 ? w.preferred_monitor : x11_window_monitor(&w);
            const auto &m = monitors.at(static_cast<std::size_t>(index));
            x11_set_position(w, m.position);
            x11_state(w, true, "_NET_WM_STATE_FULLSCREEN");
        }
    }

    void x11_set_icons(os_window_handle *h, std::span<const any_bitmap_view> icons) {
        auto &w = *as_x11_window(h);
        auto &c = *w.context;
        std::vector<unsigned long> values;
        for (const auto &icon : icons) {
            bitmap converted(icon.size(), px_bgra8888{});
            converting_blit_lossy(converted.view(), icon);
            values.push_back(static_cast<unsigned long>(icon.width()));
            values.push_back(static_cast<unsigned long>(icon.height()));
            for (int y = 0; y < icon.height(); ++y) {
                const auto *row = static_cast<const unsigned char *>(converted.line(y));
                for (int x = 0; x < icon.width(); ++x) {
                    const auto *p = row + x * 4;
                    values.push_back((static_cast<unsigned long>(p[3]) << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
                }
            }
        }
        if (values.empty())
            XDeleteProperty(c.display, w.xid, c.atom("_NET_WM_ICON"));
        else
            XChangeProperty(
                c.display,
                w.xid,
                c.atom("_NET_WM_ICON"),
                XA_CARDINAL,
                32,
                PropModeReplace,
                reinterpret_cast<const unsigned char *>(values.data()),
                static_cast<int>(values.size())
            );
        XFlush(c.display);
    }

    void x11_dispatch(x11_context &c, XEvent &event) {
        if (x11_selection_event(c, event))
            return;
        if (event.type == MappingNotify) {
            XRefreshKeyboardMapping(&event.xmapping);
            x11_update_keymap(c);
            return;
        }
        if (c.xkb && event.type == c.xkb_event) {
            auto *xkb = reinterpret_cast<XkbEvent *>(&event);
            if (xkb->any.xkb_type == XkbMapNotify)
                XkbRefreshKeyboardMapping(&xkb->map);
            x11_update_keymap(c);
            return;
        }
        if (c.randr && event.type == c.randr_event + RRScreenChangeNotify) {
            XRRUpdateConfiguration(&event);
            return;
        }
        if (event.type == GenericEvent && event.xcookie.extension == c.xi_opcode &&
            XGetEventData(c.display, &event.xcookie)) {
            if (event.xcookie.evtype == XI_RawMotion && c.mouse_owner &&
                c.mouse_owner->active == mouse_mode::relative) {
                auto &w = *c.mouse_owner;
                auto &raw = *static_cast<XIRawEvent *>(event.xcookie.data);
                auto *value = raw.raw_values;
                for (int axis = 0; axis < raw.valuators.mask_len * 8; ++axis)
                    if (XIMaskIsSet(raw.valuators.mask, axis)) {
                        if (axis == 0)
                            w.raw_x += *value;
                        if (axis == 1)
                            w.raw_y += *value;
                        ++value;
                    }
                vec2i delta{static_cast<int>(w.raw_x), static_cast<int>(w.raw_y)};
                w.raw_x -= delta.x;
                w.raw_y -= delta.y;
                detail::mouse_handle_relative(w.source, delta);
            }
            XFreeEventData(c.display, &event.xcookie);
            return;
        }
        if (auto log = c.logs.find(event.xany.window); log != c.logs.end()) {
            auto &l = *log->second;
            if (event.type == Expose)
                x11_draw_text_log(l);
            else if (event.type == ConfigureNotify) {
                l.width = event.xconfigure.width;
                l.height = event.xconfigure.height;
            } else if (event.type == MapNotify)
                l.visible = true;
            else if (event.type == UnmapNotify)
                l.visible = false;
            else if (event.type == DestroyNotify) {
                c.logs.erase(l.xid);
                l.xid = None;
                l.visible = false;
                l.source.emit(window_close_event{});
            } else if (event.type == ClientMessage && event.xclient.message_type == c.wm_protocols &&
                       static_cast<Atom>(event.xclient.data.l[0]) == c.wm_delete) {
                l.source.emit(window_close_event{});
                XUnmapWindow(c.display, l.xid);
                l.visible = false;
            }
            return;
        }
        auto found = c.windows.find(event.xany.window);
        if (found == c.windows.end())
            return;
        auto &w = *found->second;
        // XIM may consume key events, but physical state must still be updated.
        const bool filtered = XFilterEvent(&event, w.xid) != False;
        if (filtered && event.type != KeyPress && event.type != KeyRelease)
            return;
        switch (event.type) {
        case ClientMessage:
            if (event.xclient.message_type == c.wm_protocols &&
                static_cast<Atom>(event.xclient.data.l[0]) == c.wm_delete)
                w.source.emit(window_close_event{});
            break;
        case ConfigureNotify: {
            const vec2i size{event.xconfigure.width, event.xconfigure.height};
            if (size != w.client_size) {
                w.client_size = size;
                w.source.emit(window_resize_event{size});
            }
            ::Window child;
            int x, y;
            XTranslateCoordinates(c.display, w.xid, c.root, 0, 0, &x, &y, &child);
            w.client_position = {x, y};
            x11_refresh_mouse(w);
            break;
        }
        case MapNotify:
            w.mapped = true;
            w.minimized = false;
            x11_refresh_mouse(w);
            break;
        case UnmapNotify:
            w.mapped = false;
            w.minimized = true;
            release_buttons(w);
            x11_refresh_mouse(w);
            break;
        case PropertyNotify:
            if (event.xproperty.atom == c.atom("_NET_WM_STATE")) {
                update_state(w);
                x11_refresh_mouse(w);
            }
            break;
        case Expose:
            if (w.expose)
                w.source.emit(window_expose_event{
                    {{event.xexpose.x, event.xexpose.y},
                     {event.xexpose.x + event.xexpose.width, event.xexpose.y + event.xexpose.height}}
                });
            break;
        case FocusIn:
            if (event.xfocus.mode == NotifyGrab || event.xfocus.mode == NotifyUngrab ||
                event.xfocus.detail == NotifyInferior)
                break;
            w.focused = true;
            detail::set_current_window(w.owner);
            if (c.xkb) {
                XkbStateRec state{};
                if (XkbGetState(c.display, XkbUseCoreKbd, &state) == Success)
                    detail::refresh_keyboard_modifiers(modifiers(c, state.locked_mods));
            }
            if (w.input_context)
                XSetICFocus(w.input_context);
            if (w.display_mode_suspended) {
                try {
                    x11_apply_display(w, w.exclusive_mode);
                    w.display_mode_suspended = false;
                } catch (...) {
                }
            }
            x11_refresh_mouse(w);
            break;
        case FocusOut:
            if (event.xfocus.mode == NotifyGrab || event.xfocus.mode == NotifyUngrab ||
                event.xfocus.detail == NotifyInferior)
                break;
            w.focused = false;
            if (window::current() == w.owner)
                detail::set_current_window(nullptr);
            if (w.input_context)
                XUnsetICFocus(w.input_context);
            for (std::size_t code = 0; code < w.keys.size(); ++code)
                if (w.keys[code])
                    w.source.emit(window_key_up_event{c.keymap[code]});
            w.keys.fill(false);
            {
                XkbStateRec state{};
                unsigned locks = 0;
                if (c.xkb && XkbGetState(c.display, XkbUseCoreKbd, &state) == Success)
                    locks = state.locked_mods;
                detail::clear_keyboard_state(modifiers(c, locks));
            }
            release_buttons(w);
            if (w.display_mode_applied) {
                x11_restore_display(w);
                w.display_mode_suspended = true;
            }
            x11_refresh_mouse(w);
            break;
        case KeyPress:
        case KeyRelease: {
            const unsigned code = event.xkey.keycode;
            if (code >= w.keys.size())
                break;
            // Servers without detectable autorepeat send release/press pairs.
            if (event.type == KeyRelease && XPending(c.display)) {
                XEvent next;
                XPeekEvent(c.display, &next);
                if (next.type == KeyPress && next.xkey.window == w.xid && next.xkey.keycode == code &&
                    next.xkey.time == event.xkey.time)
                    break;
            }
            const auto key = c.keymap[code];
            const bool down = event.type == KeyPress, repeat = down && w.keys[code];
            w.keys[code] = down;
            XkbStateRec state{};
            unsigned mods = event.xkey.state;
            if (c.xkb && XkbGetState(c.display, XkbUseCoreKbd, &state) == Success)
                mods = state.locked_mods;
            detail::update_keyboard_state(key, down, modifiers(c, mods));
            if (down) {
                if (!repeat)
                    w.source.emit(window_key_down_event{key});
                if (!filtered)
                    emit_text(w, event.xkey, key, repeat);
            } else
                w.source.emit(window_key_up_event{key});
            break;
        }
        case EnterNotify:
            w.mouse_position = {event.xcrossing.x, event.xcrossing.y};
            detail::mouse_handle_enter(w.owner, w.source, w.mouse_position);
            x11_update_cursor(w);
            break;
        case LeaveNotify:
            w.mouse_position = {event.xcrossing.x, event.xcrossing.y};
            detail::mouse_handle_leave(w.owner, w.source, w.mouse_position);
            break;
        case MotionNotify:
            w.mouse_position = {event.xmotion.x, event.xmotion.y};
            if (w.warps.should_suppress(w.mouse_position))
                break;
            if (w.active == mouse_mode::relative) {
                detail::mouse_update_position(w.owner, w.mouse_position);
                const auto center = w.client_size / 2;
                if (w.mouse_position != center) {
                    w.warps.record(center);
                    XWarpPointer(c.display, None, w.xid, 0, 0, 0, 0, center.x, center.y);
                }
            } else
                detail::mouse_handle_axes(w.owner, w.source, w.mouse_position);
            break;
        case ButtonPress:
        case ButtonRelease: {
            const bool down = event.type == ButtonPress;
            const unsigned native = event.xbutton.button;
            w.mouse_position = {event.xbutton.x, event.xbutton.y};
            if (native >= 4 && native <= 7) {
                if (down)
                    detail::mouse_handle_wheel(
                        w.owner,
                        w.source,
                        w.mouse_position,
                        native == 4   ? 120
                        : native == 5 ? -120
                                      : 0,
                        native == 7   ? 120
                        : native == 6 ? -120
                                      : 0
                    );
            } else {
                const unsigned n = native == 2 ? 3 : native == 3 ? 2 : native >= 8 ? native - 4 : native;
                if (n < 1 || n > 32)
                    break;
                if (down)
                    w.buttons |= std::uint32_t{1} << (n - 1);
                else
                    w.buttons &= ~(std::uint32_t{1} << (n - 1));
                detail::mouse_handle_button(w.owner, w.source, static_cast<mouse_button>(n), down, w.mouse_position);
            }
            break;
        }
        case DestroyNotify:
            if (c.mouse_owner == &w)
                x11_ungrab_mouse(&c);
            release_buttons(w);
            x11_restore_display(w);
            w.source.emit(window_close_event{});
            c.windows.erase(w.xid);
            w.xid = None;
            break;
        }
    }

    void x11_poll(os_context_handle *h) {
        auto &c = *as_x11_context(h);
        while (XPending(c.display)) {
            XEvent event;
            XNextEvent(c.display, &event);
            x11_dispatch(c, event);
        }
        const auto now = std::chrono::steady_clock::now();
        std::erase_if(c.transfers, [&](auto &t) { return now >= t.deadline; });
        XFlush(c.display);
    }
} // namespace alia
#endif
