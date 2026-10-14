#include "x11_ops.hpp"
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_X11
#include <algorithm>
#include <climits>
#include <poll.h>

namespace alia {
namespace {
constexpr std::size_t clipboard_limit = 16 * 1024 * 1024;
constexpr auto selection_timeout = std::chrono::seconds(2);
struct property_data {
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned char *data = nullptr;
    ~property_data() { if (data) XFree(data); }
};
bool read_property(x11_context &c, ::Window window, Atom property, property_data &out) {
    unsigned long remaining = 0;
    return XGetWindowProperty(c.display, window, property, 0, static_cast<long>(clipboard_limit / 4), True, AnyPropertyType,
        &out.type, &out.format, &out.count, &remaining, &out.data) == Success && remaining == 0;
}
template<class Predicate> bool wait_event(x11_context &c, XEvent &out, Predicate predicate, std::chrono::steady_clock::time_point deadline) {
    while (std::chrono::steady_clock::now() < deadline) {
        while (XPending(c.display)) {
            XNextEvent(c.display, &out);
            if (predicate(out)) return true;
            x11_dispatch(c, out);
        }
        XFlush(c.display);
        pollfd fd{ConnectionNumber(c.display), POLLIN, 0}; ::poll(&fd, 1, 10);
    }
    return false;
}
struct selection_result { Atom type = None; int format = 0; std::string text; std::vector<Atom> atoms; };
selection_result request_selection(x11_context &c, Atom target) {
    selection_result result;
    if (XGetSelectionOwner(c.display, c.clipboard) == None) return result;
    const ::Window requestor = XCreateSimpleWindow(c.display, c.root, 0, 0, 1, 1, 0, 0, 0);
    if (!requestor) return result;
    struct destroy_window { Display *d; ::Window w; ~destroy_window() { XDestroyWindow(d, w); XFlush(d); } } cleanup{c.display, requestor};
    XSelectInput(c.display, requestor, PropertyChangeMask);
    XConvertSelection(c.display, c.clipboard, target, c.selection_property, requestor, CurrentTime);
    XFlush(c.display);
    const auto deadline = std::chrono::steady_clock::now() + selection_timeout;
    XEvent event;
    if (!wait_event(c, event, [&](const XEvent &e) { return e.type == SelectionNotify && e.xselection.requestor == requestor && e.xselection.selection == c.clipboard && e.xselection.target == target; }, deadline) || event.xselection.property == None)
        return result;
    property_data initial;
    if (!read_property(c, requestor, c.selection_property, initial)) return result;
    if (initial.type == c.incr) {
        // Deleting the INCR property acknowledges it. Each chunk is likewise
        // deleted after reading; an empty final chunk terminates the transfer.
        XFlush(c.display);
        for (;;) {
            if (!wait_event(c, event, [&](const XEvent &e) { return e.type == PropertyNotify && e.xproperty.window == requestor && e.xproperty.atom == c.selection_property && e.xproperty.state == PropertyNewValue; }, deadline)) return {};
            property_data chunk;
            if (!read_property(c, requestor, c.selection_property, chunk) || chunk.format != 8 || chunk.type != target) return {};
            if (!chunk.count) { result.type = chunk.type; result.format = 8; return result; }
            if (chunk.count > clipboard_limit - result.text.size()) return {};
            result.text.append(reinterpret_cast<const char *>(chunk.data), chunk.count); XFlush(c.display);
        }
    }
    result.type = initial.type; result.format = initial.format;
    if (initial.format == 8 && initial.count) result.text.assign(reinterpret_cast<const char *>(initial.data), initial.count);
    else if (initial.format == 32 && initial.type == XA_ATOM && initial.count) {
        auto *atoms = reinterpret_cast<Atom *>(initial.data); result.atoms.assign(atoms, atoms + initial.count);
    }
    return result;
}
std::string latin1_from_utf8(std::string_view text) {
    std::string result;
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i++]);
        if (lead < 0x80) result.push_back(static_cast<char>(lead));
        else if ((lead == 0xc2 || lead == 0xc3) && i < text.size()) {
            const auto tail = static_cast<unsigned char>(text[i++]); result.push_back(static_cast<char>(((lead & 3) << 6) | (tail & 63)));
        } else { while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xc0) == 0x80) ++i; result.push_back('?'); }
    }
    return result;
}
std::string utf8_from_latin1(std::string_view text) {
    std::string result;
    for (const unsigned char ch : text) {
        if (ch < 0x80) result.push_back(static_cast<char>(ch));
        else { result.push_back(static_cast<char>(0xc0 | (ch >> 6))); result.push_back(static_cast<char>(0x80 | (ch & 63))); }
    }
    return result;
}
std::size_t chunk_size(x11_context &c) { return std::min<std::size_t>(65536, static_cast<std::size_t>(XMaxRequestSize(c.display) - 64) * 4); }
}
bool x11_selection_event(x11_context &c, XEvent &event) {
    if (event.type == SelectionClear && event.xselectionclear.window == c.clipboard_window && event.xselectionclear.selection == c.clipboard) {
        c.clipboard_text.clear(); return true;
    }
    if (event.type == PropertyNotify && event.xproperty.state == PropertyDelete) {
        auto found = std::find_if(c.transfers.begin(), c.transfers.end(), [&](auto &t) { return t.requestor == event.xproperty.window && t.property == event.xproperty.atom; });
        if (found == c.transfers.end()) return false;
        x11_error_scope errors(c.display);
        const auto count = std::min(chunk_size(c), found->data.size() - found->offset);
        XChangeProperty(c.display, found->requestor, found->property, found->target, 8, PropModeReplace,
            reinterpret_cast<const unsigned char *>(found->data.data() + found->offset), static_cast<int>(count));
        if (!count || errors.sync()) c.transfers.erase(found);
        else { found->offset += count; found->deadline = std::chrono::steady_clock::now() + selection_timeout; }
        return true;
    }
    if (event.type == DestroyNotify) {
        std::erase_if(c.transfers, [&](auto &t) { return t.requestor == event.xdestroywindow.window; });
        return false;
    }
    if (event.type != SelectionRequest || event.xselectionrequest.owner != c.clipboard_window) return false;
    x11_error_scope errors(c.display);
    const auto &request = event.xselectionrequest;
    XEvent reply{}; reply.xselection.type = SelectionNotify; reply.xselection.display = c.display;
    reply.xselection.requestor = request.requestor; reply.xselection.selection = request.selection;
    reply.xselection.target = request.target; reply.xselection.time = request.time; reply.xselection.property = None;
    const Atom property = request.property ? request.property : request.target;
    if (request.selection == c.clipboard && request.target == c.targets) {
        const Atom targets[] = {c.targets, c.utf8, XA_STRING};
        XChangeProperty(c.display, request.requestor, property, XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char *>(targets), 3);
        reply.xselection.property = property;
    } else if (request.selection == c.clipboard && (request.target == c.utf8 || request.target == XA_STRING)) {
        std::string data = request.target == c.utf8 ? c.clipboard_text : latin1_from_utf8(c.clipboard_text);
        if (data.size() <= chunk_size(c)) {
            XChangeProperty(c.display, request.requestor, property, request.target, 8, PropModeReplace,
                reinterpret_cast<const unsigned char *>(data.data()), static_cast<int>(data.size()));
        } else {
            XWindowAttributes attributes{};
            if (!XGetWindowAttributes(c.display, request.requestor, &attributes)) return true;
            XSelectInput(c.display, request.requestor, attributes.your_event_mask | PropertyChangeMask | StructureNotifyMask);
            const unsigned long length = static_cast<unsigned long>(data.size());
            XChangeProperty(c.display, request.requestor, property, c.incr, 32, PropModeReplace, reinterpret_cast<const unsigned char *>(&length), 1);
            std::erase_if(c.transfers, [&](auto &t) { return t.requestor == request.requestor && t.property == property; });
            c.transfers.push_back({request.requestor, property, request.target, std::move(data), 0, std::chrono::steady_clock::now() + selection_timeout});
        }
        reply.xselection.property = property;
    }
    XSendEvent(c.display, request.requestor, False, 0, &reply);
    if (errors.sync()) std::erase_if(c.transfers, [&](auto &t) { return t.requestor == request.requestor; });
    return true;
}
bool x11_clipboard_has_text(os_context_handle *h) {
    auto &c = *as_x11_context(h);
    if (XGetSelectionOwner(c.display, c.clipboard) == c.clipboard_window) return true;
    auto result = request_selection(c, c.targets);
    return std::find(result.atoms.begin(), result.atoms.end(), c.utf8) != result.atoms.end() ||
           std::find(result.atoms.begin(), result.atoms.end(), XA_STRING) != result.atoms.end();
}
bool x11_clipboard_has_image(os_context_handle *h) {
    auto &c = *as_x11_context(h); if (XGetSelectionOwner(c.display, c.clipboard) == c.clipboard_window) return false;
    auto result = request_selection(c, c.targets);
    for (auto name : {"image/png", "image/bmp", "image/jpeg", "image/tiff"})
        if (std::find(result.atoms.begin(), result.atoms.end(), c.atom(name)) != result.atoms.end()) return true;
    return false;
}
std::string x11_clipboard_get_text(os_context_handle *h) {
    auto &c = *as_x11_context(h);
    if (XGetSelectionOwner(c.display, c.clipboard) == c.clipboard_window) return c.clipboard_text;
    auto result = request_selection(c, c.utf8);
    if (result.type == c.utf8 && result.format == 8) return result.text;
    result = request_selection(c, XA_STRING);
    return result.type == XA_STRING && result.format == 8 ? utf8_from_latin1(result.text) : std::string{};
}
bool x11_clipboard_set_text(os_context_handle *h, std::string_view text) {
    auto &c = *as_x11_context(h);
    if (text.size() > clipboard_limit) throw std::length_error("X11 clipboard text exceeds 16 MiB");
    std::string copy(text);
    XSetSelectionOwner(c.display, c.clipboard, c.clipboard_window, CurrentTime);
    if (XGetSelectionOwner(c.display, c.clipboard) != c.clipboard_window) return false;
    c.clipboard_text = std::move(copy); XFlush(c.display); return true;
}
void x11_clipboard_clear(os_context_handle *h) {
    auto &c = *as_x11_context(h);
    if (XGetSelectionOwner(c.display, c.clipboard) == c.clipboard_window) XSetSelectionOwner(c.display, c.clipboard, None, CurrentTime);
    c.clipboard_text.clear(); XFlush(c.display);
}
}
#endif
