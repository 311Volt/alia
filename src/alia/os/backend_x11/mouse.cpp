#include "x11_ops.hpp"
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_X11
#include "../../events/event_source_impl.hpp"
#include "../../gfx/bitmap/bitmap.hpp"
#include <X11/cursorfont.h>
#include <algorithm>
#include <cstdint>

namespace alia {
    namespace {
        unsigned cursor_shape(system_mouse_cursor shape) {
            switch (shape) {
            case system_mouse_cursor::busy:                return XC_watch;
            case system_mouse_cursor::question:            return XC_question_arrow;
            case system_mouse_cursor::edit:                return XC_xterm;
            case system_mouse_cursor::move:                return XC_fleur;
            case system_mouse_cursor::resize_north:        return XC_top_side;
            case system_mouse_cursor::resize_south:        return XC_bottom_side;
            case system_mouse_cursor::resize_east:         return XC_right_side;
            case system_mouse_cursor::resize_west:         return XC_left_side;
            case system_mouse_cursor::resize_northeast:    return XC_top_right_corner;
            case system_mouse_cursor::resize_northwest:    return XC_top_left_corner;
            case system_mouse_cursor::resize_southeast:    return XC_bottom_right_corner;
            case system_mouse_cursor::resize_southwest:    return XC_bottom_left_corner;
            case system_mouse_cursor::progress:            return XC_watch;
            case system_mouse_cursor::precision:           return XC_crosshair;
            case system_mouse_cursor::link:                return XC_hand2;
            case system_mouse_cursor::alternate_selection: return XC_center_ptr;
            case system_mouse_cursor::unavailable:         return XC_X_cursor;
            default:                                       return XC_left_ptr;
            }
        }

        void mode_changed(x11_window &w) {
            w.source.emit(mouse_mode_changed_event{w.requested, w.active});
        }
    } // namespace

    void x11_update_cursor(x11_window &w) {
        if (!w.xid)
            return;
        auto &c = *w.context;
        Cursor selected = w.cursor;
        if (w.custom_cursor)
            selected = static_cast<Cursor>(reinterpret_cast<std::uintptr_t>(w.custom_cursor->native_handle()));
        if (!w.cursor_visible || w.active == mouse_mode::relative) {
            if (!w.hidden_cursor) {
                const char bits[] = {0};
                XColor black{};
                Pixmap blank = XCreateBitmapFromData(c.display, w.xid, bits, 1, 1);
                if (blank) {
                    w.hidden_cursor = XCreatePixmapCursor(c.display, blank, blank, &black, &black, 0, 0);
                    XFreePixmap(c.display, blank);
                }
            }
            selected = w.hidden_cursor;
        }
        if (selected)
            XDefineCursor(c.display, w.xid, selected);
        else
            XUndefineCursor(c.display, w.xid);
        XFlush(c.display);
    }

    bool x11_set_system_cursor(os_window_handle *h, system_mouse_cursor value) {
        auto &w = *as_x11_window(h);
        auto &c = *w.context;
        Cursor next = XCreateFontCursor(c.display, cursor_shape(value));
        if (!next)
            return false;
        if (w.cursor)
            XFreeCursor(c.display, w.cursor);
        w.cursor = next;
        w.custom_cursor.reset();
        x11_update_cursor(w);
        return true;
    }

    void x11_refresh_mouse(x11_window &w) {
        auto &c = *w.context;
        if (c.mouse_owner != &w)
            return;
        const auto before = w.active;
        const bool activate = w.focused && w.mapped && !w.minimized && w.xid && w.requested != mouse_mode::normal;
        if (!activate) {
            if (w.active != mouse_mode::normal)
                XUngrabPointer(c.display, CurrentTime);
            w.active = mouse_mode::normal;
        } else if (w.active == mouse_mode::normal) {
            const int result = XGrabPointer(
                c.display,
                w.xid,
                True,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync,
                GrabModeAsync,
                w.xid,
                None,
                CurrentTime
            );
            if (result == GrabSuccess) {
                w.active = w.requested;
                w.raw_x = w.raw_y = 0;
                if (w.active == mouse_mode::relative) {
                    const auto center = w.client_size / 2;
                    w.warps.record(center);
                    XWarpPointer(c.display, None, w.xid, 0, 0, 0, 0, center.x, center.y);
                }
            }
        }
        if (before != w.active)
            mode_changed(w);
        x11_update_cursor(w);
    }

    bool x11_set_mouse_mode(os_window_handle *h, mouse_mode value) {
        auto &w = *as_x11_window(h);
        auto &c = *w.context;
        if (value != mouse_mode::normal && value != mouse_mode::confined && value != mouse_mode::relative)
            return false;
        if (value == mouse_mode::relative && !c.raw_motion)
            return false;
        if (w.requested == value) {
            x11_refresh_mouse(w);
            return true;
        }
        if (c.mouse_owner)
            x11_ungrab_mouse(&c);
        if (value == mouse_mode::normal)
            return true;
        c.mouse_owner = &w;
        w.requested = value;
        w.active = mouse_mode::normal;
        x11_refresh_mouse(w);
        if (w.focused && w.mapped && !w.minimized && w.active == mouse_mode::normal) {
            w.requested = mouse_mode::normal;
            c.mouse_owner = nullptr;
            mode_changed(w);
            return false;
        }
        if (w.active == mouse_mode::normal)
            mode_changed(w);
        return true;
    }

    bool x11_ungrab_mouse(os_context_handle *h) {
        auto &c = *as_x11_context(h);
        if (!c.mouse_owner)
            return true;
        auto &w = *c.mouse_owner;
        if (w.active != mouse_mode::normal)
            XUngrabPointer(c.display, CurrentTime);
        c.mouse_owner = nullptr;
        w.requested = w.active = mouse_mode::normal;
        w.raw_x = w.raw_y = 0;
        mode_changed(w);
        x11_update_cursor(w);
        return true;
    }

    bool x11_set_cursor_position(os_window_handle *h, vec2i position) {
        auto &w = *as_x11_window(h);
        auto &c = *w.context;
        if (!w.xid)
            return false;
        // Process physical motion already queued before recording the synthetic warp.
        x11_poll(&c);
        if (!w.xid)
            return false;
        XWarpPointer(c.display, None, w.xid, 0, 0, 0, 0, position.x, position.y);
        XSync(c.display, False);
        ::Window root, child;
        int root_x, root_y, x, y;
        unsigned mask;
        if (!XQueryPointer(c.display, w.xid, &root, &child, &root_x, &root_y, &x, &y, &mask))
            return false;
        w.mouse_position = {x, y};
        w.warps.record(w.mouse_position);
        detail::mouse_handle_warped(w.owner, w.source, w.mouse_position);
        return true;
    }

    os_cursor_handle *x11_create_cursor(os_context_handle *h, const any_bitmap_view &source, vec2i hotspot) {
        auto &c = *as_x11_context(h);
        using image_ptr = std::unique_ptr<XcursorImage, decltype(&XcursorImageDestroy)>;
        image_ptr image(XcursorImageCreate(source.width(), source.height()), XcursorImageDestroy);
        if (!image)
            return nullptr;
        bitmap converted(source.size(), px_bgra8888{});
        converting_blit_lossy(converted.view(), source);
        image->xhot = static_cast<XcursorDim>(hotspot.x);
        image->yhot = static_cast<XcursorDim>(hotspot.y);
        for (int y = 0; y < source.height(); ++y) {
            const auto *row = static_cast<const unsigned char *>(converted.line(y));
            for (int x = 0; x < source.width(); ++x) {
                const auto *p = row + x * 4;
                const unsigned alpha = p[3];
                image->pixels[static_cast<std::size_t>(y) * source.width() + x] =
                    (alpha << 24) | (((p[2] * alpha + 127) / 255) << 16) | (((p[1] * alpha + 127) / 255) << 8) |
                    ((p[0] * alpha + 127) / 255);
            }
        }
        auto result = std::make_unique<x11_cursor>();
        const Cursor cursor = XcursorImageLoadCursor(c.display, image.get());
        if (!cursor)
            return nullptr;
        result->context = &c;
        result->cursor = cursor;
        return result.release();
    }

    void x11_destroy_cursor(os_cursor_handle *h) {
        auto *cursor = static_cast<x11_cursor *>(h);
        XFreeCursor(cursor->context->display, cursor->cursor);
        delete cursor;
    }

    void *x11_cursor_native_handle(const os_cursor_handle *h) {
        return reinterpret_cast<void *>(static_cast<std::uintptr_t>(static_cast<const x11_cursor *>(h)->cursor));
    }

    unsigned x11_mouse_num_buttons(os_context_handle *h) {
        std::array<unsigned char, 256> mapping{};
        const int count =
            XGetPointerMapping(as_x11_context(h)->display, mapping.data(), static_cast<int>(mapping.size()));
        // Core buttons 4-7 are wheel axes rather than application buttons.
        return static_cast<unsigned>(std::clamp(count > 7 ? count - 4 : std::min(count, 3), 0, 32));
    }

    std::optional<vec2i> x11_mouse_cursor_position(os_context_handle *h) {
        auto &c = *as_x11_context(h);
        ::Window root, child;
        int x, y, local_x, local_y;
        unsigned mask;
        if (!XQueryPointer(c.display, c.root, &root, &child, &x, &y, &local_x, &local_y, &mask))
            return std::nullopt;
        return vec2i{x, y};
    }
} // namespace alia
#endif
