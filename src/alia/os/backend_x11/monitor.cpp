#include "x11_ops.hpp"
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_X11
#include <algorithm>
#include <cmath>
#include <tuple>

namespace alia {
namespace {
using resources_ptr = std::unique_ptr<XRRScreenResources, decltype(&XRRFreeScreenResources)>;
using output_ptr = std::unique_ptr<XRROutputInfo, decltype(&XRRFreeOutputInfo)>;
using crtc_ptr = std::unique_ptr<XRRCrtcInfo, decltype(&XRRFreeCrtcInfo)>;
struct output_entry { RROutput output; RRCrtc crtc; bool primary; int x, y; };
resources_ptr resources(x11_context &c) {
    return {XRRGetScreenResourcesCurrent(c.display, c.root), XRRFreeScreenResources};
}
std::vector<output_entry> outputs(x11_context &c, XRRScreenResources *r) {
    std::vector<output_entry> result;
    const auto primary = XRRGetOutputPrimary(c.display, c.root);
    for (int i = 0; i < r->noutput; ++i) {
        output_ptr o(XRRGetOutputInfo(c.display, r, r->outputs[i]), XRRFreeOutputInfo);
        if (!o || o->connection != RR_Connected || !o->crtc) continue;
        crtc_ptr crtc(XRRGetCrtcInfo(c.display, r, o->crtc), XRRFreeCrtcInfo);
        if (!crtc || !crtc->mode) continue;
        result.push_back({r->outputs[i], o->crtc, r->outputs[i] == primary, crtc->x, crtc->y});
    }
    std::sort(result.begin(), result.end(), [](auto a, auto b) {
        return std::tuple(!a.primary, a.x, a.y, a.output) < std::tuple(!b.primary, b.x, b.y, b.output);
    });
    return result;
}
display_mode mode_from_native(const XRRModeInfo &m, int depth, Rotation rotation) {
    double rate = m.hTotal && m.vTotal ? static_cast<double>(m.dotClock) / m.hTotal / m.vTotal : 0;
    if (m.modeFlags & RR_Interlace) rate *= 2;
    if (m.modeFlags & RR_DoubleScan) rate /= 2;
    vec2i size{static_cast<int>(m.width), static_cast<int>(m.height)};
    if (rotation & (RR_Rotate_90 | RR_Rotate_270)) std::swap(size.x, size.y);
    return {size, static_cast<int>(std::lround(rate)), depth};
}
const XRRModeInfo *find_mode(XRRScreenResources &r, RRMode id) {
    for (int i = 0; i < r.nmode; ++i) if (r.modes[i].id == id) return &r.modes[i];
    return nullptr;
}
void restore_screen_size(x11_context &c) noexcept {
    if (c.saved_screen_size == vec2i{}) return;
    x11_error_scope errors(c.display);
    XWindowAttributes screen{};
    if (!XGetWindowAttributes(c.display, c.root, &screen)) return;
    if (vec2i{screen.width, screen.height} != c.expanded_screen_size) {
        // Another client changed the desktop while fullscreen was active.
        c.saved_screen_size = {}; return;
    }
    auto r = resources(c); if (!r) return;
    for (int i = 0; i < r->ncrtc; ++i) {
        crtc_ptr crtc(XRRGetCrtcInfo(c.display, r.get(), r->crtcs[i]), XRRFreeCrtcInfo);
        if (!crtc) return;
        if (crtc->mode && (crtc->x + static_cast<int>(crtc->width) > c.saved_screen_size.x ||
                           crtc->y + static_cast<int>(crtc->height) > c.saved_screen_size.y)) return;
    }
    XRRSetScreenSize(c.display, c.root, c.saved_screen_size.x, c.saved_screen_size.y, c.saved_screen_mm.x, c.saved_screen_mm.y);
    if (!errors.sync()) c.saved_screen_size = {};
}
}
std::vector<monitor_info> x11_monitors(os_context_handle *h) {
    auto &c = *as_x11_context(h);
    std::vector<monitor_info> result;
    if (c.randr) {
        auto r = resources(c);
        if (!r) throw std::runtime_error("X11: unable to query RandR resources");
        auto entries = outputs(c, r.get());
        for (const auto &entry : entries) {
            output_ptr o(XRRGetOutputInfo(c.display, r.get(), entry.output), XRRFreeOutputInfo);
            crtc_ptr crtc(XRRGetCrtcInfo(c.display, r.get(), entry.crtc), XRRFreeCrtcInfo);
            if (!o || !crtc) continue;
            monitor_info m{};
            m.index = static_cast<int>(result.size());
            m.name.assign(o->name, static_cast<std::size_t>(o->nameLen));
            m.position = {crtc->x, crtc->y}; m.size = {static_cast<int>(crtc->width), static_cast<int>(crtc->height)};
            m.size_mm = {static_cast<int>(o->mm_width), static_cast<int>(o->mm_height)};
            if (crtc->rotation & (RR_Rotate_90 | RR_Rotate_270)) std::swap(m.size_mm.x, m.size_mm.y);
            m.dpi = m.size_mm.x > 0 ? m.size.x * 25.4f / m.size_mm.x : 96.f;
            m.scale_factor = m.dpi / 96.f; m.is_primary = entry.primary;
            const int depth = DefaultDepth(c.display, c.screen);
            if (auto *native = find_mode(*r, crtc->mode)) m.current_mode = mode_from_native(*native, depth, crtc->rotation);
            for (int i = 0; i < o->nmode; ++i) {
                if (auto *native = find_mode(*r, o->modes[i])) m.modes.push_back(mode_from_native(*native, depth, crtc->rotation));
            }
            std::sort(m.modes.begin(), m.modes.end(), [](auto a, auto b) { return std::tuple(a.size.x, a.size.y, a.refresh_rate) < std::tuple(b.size.x, b.size.y, b.refresh_rate); });
            m.modes.erase(std::unique(m.modes.begin(), m.modes.end()), m.modes.end());
            result.push_back(std::move(m));
        }
    }
    if (result.empty()) {
        monitor_info m{}; m.index = 0; m.name = "X11 screen";
        m.size = {DisplayWidth(c.display, c.screen), DisplayHeight(c.display, c.screen)};
        m.size_mm = {DisplayWidthMM(c.display, c.screen), DisplayHeightMM(c.display, c.screen)};
        m.dpi = m.size_mm.x > 0 ? m.size.x * 25.4f / m.size_mm.x : 96.f;
        m.scale_factor = m.dpi / 96.f; m.is_primary = true;
        m.current_mode = {m.size, 0, DefaultDepth(c.display, c.screen)}; m.modes.push_back(m.current_mode);
        result.push_back(std::move(m));
    }
    if (std::none_of(result.begin(), result.end(), [](auto &m) { return m.is_primary; })) result.front().is_primary = true;
    return result;
}
std::vector<display_mode> x11_display_modes(os_context_handle *h, int index) {
    auto monitors = x11_monitors(h);
    if (index < 0 || index >= static_cast<int>(monitors.size())) throw std::out_of_range("X11 monitor index out of range");
    return std::move(monitors[static_cast<std::size_t>(index)].modes);
}
display_mode x11_desktop_mode(os_context_handle *h, int index) {
    auto monitors = x11_monitors(h);
    if (index < 0 || index >= static_cast<int>(monitors.size())) throw std::out_of_range("X11 monitor index out of range");
    return monitors[static_cast<std::size_t>(index)].current_mode;
}
int x11_window_monitor(const os_window_handle *h) {
    const auto &w = *as_x11_window(h);
    const auto monitors = x11_monitors(w.context);
    int best = 0; long long best_area = -1;
    for (auto &m : monitors) {
        const int x = std::max(0, std::min(w.client_position.x + w.client_size.x, m.position.x + m.size.x) - std::max(w.client_position.x, m.position.x));
        const int y = std::max(0, std::min(w.client_position.y + w.client_size.y, m.position.y + m.size.y) - std::max(w.client_position.y, m.position.y));
        const long long area = static_cast<long long>(x) * y;
        if (area > best_area) { best_area = area; best = m.index; }
    }
    return best;
}
void x11_apply_display(x11_window &w, const display_mode &mode) {
    auto &c = *w.context;
    x11_error_scope errors(c.display);
    if (!c.randr) throw unsupported_operation_exception("X11 server has no RandR 1.3 display-mode service");
    auto r = resources(c);
    if (!r) throw std::runtime_error("X11: unable to query RandR resources");
    auto entries = outputs(c, r.get());
    int index = w.preferred_monitor >= 0 ? w.preferred_monitor : x11_window_monitor(&w);
    if (w.saved_mode != None) {
        const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto &e) { return e.crtc == w.exclusive_crtc; });
        if (found == entries.end()) throw std::runtime_error("X11 fullscreen display is no longer connected");
        index = static_cast<int>(found - entries.begin());
    }
    if (index < 0 || index >= static_cast<int>(entries.size())) throw std::out_of_range("X11 fullscreen monitor no longer available");
    const auto entry = entries[static_cast<std::size_t>(index)];
    for (const auto &[id, other] : c.windows)
        if (other != &w && other->exclusive_crtc == entry.crtc && other->mode == window_fullscreen_mode::fullscreen)
            throw std::runtime_error("X11 display is already owned by another fullscreen window");
    output_ptr o(XRRGetOutputInfo(c.display, r.get(), entry.output), XRRFreeOutputInfo);
    crtc_ptr crtc(XRRGetCrtcInfo(c.display, r.get(), entry.crtc), XRRFreeCrtcInfo);
    if (!o || !crtc) throw std::runtime_error("X11 fullscreen monitor was disconnected");
    RRMode selected = None;
    for (int i = 0; i < o->nmode; ++i) {
        auto *native = find_mode(*r, o->modes[i]);
        if (!native) continue;
        const auto candidate = mode_from_native(*native, DefaultDepth(c.display, c.screen), crtc->rotation);
        if (candidate.size == mode.size && (!mode.refresh_rate || candidate.refresh_rate == mode.refresh_rate) &&
            (!mode.color_depth || candidate.color_depth == mode.color_depth)) { selected = native->id; break; }
    }
    if (!selected) throw std::invalid_argument("X11 display does not support the exact requested mode");
    // Mode changes must also be supported by every cloned output on this CRTC.
    for (int i = 0; i < crtc->noutput; ++i) {
        output_ptr clone(XRRGetOutputInfo(c.display, r.get(), crtc->outputs[i]), XRRFreeOutputInfo);
        if (!clone || std::find(clone->modes, clone->modes + clone->nmode, selected) == clone->modes + clone->nmode)
            throw std::invalid_argument("X11 mode is unavailable on a cloned output");
    }
    XWindowAttributes screen{};
    if (!XGetWindowAttributes(c.display, c.root, &screen)) throw std::runtime_error("X11: cannot query desktop framebuffer");
    const vec2i required{std::max(screen.width, crtc->x + mode.size.x), std::max(screen.height, crtc->y + mode.size.y)};
    if (w.saved_mode == None) {
        w.exclusive_crtc = entry.crtc; w.saved_mode = crtc->mode;
        w.saved_rotation = crtc->rotation; w.saved_crtc_position = {crtc->x, crtc->y};
        w.saved_outputs.assign(crtc->outputs, crtc->outputs + crtc->noutput);
    }
    if (required != vec2i{screen.width, screen.height}) {
        int min_width, min_height, max_width, max_height;
        if (!XRRGetScreenSizeRange(c.display, c.root, &min_width, &min_height, &max_width, &max_height) ||
            required.x > max_width || required.y > max_height)
            throw std::runtime_error("X11 requested mode exceeds the supported desktop framebuffer size");
        const bool first_expansion = c.saved_screen_size == vec2i{};
        if (first_expansion) {
            c.saved_screen_size = {screen.width, screen.height};
            c.saved_screen_mm = {std::max(1, DisplayWidthMM(c.display, c.screen)), std::max(1, DisplayHeightMM(c.display, c.screen))};
        }
        const int mm_width = std::max(1, static_cast<int>(static_cast<long long>(c.saved_screen_mm.x) * required.x / c.saved_screen_size.x));
        const int mm_height = std::max(1, static_cast<int>(static_cast<long long>(c.saved_screen_mm.y) * required.y / c.saved_screen_size.y));
        XRRSetScreenSize(c.display, c.root, required.x, required.y, mm_width, mm_height);
        if (errors.sync()) {
            if (first_expansion) c.saved_screen_size = {};
            throw std::runtime_error("X11: RandR rejected desktop framebuffer expansion");
        }
        c.expanded_screen_size = required;
        // The resize changes RandR configuration timestamps.
        r = resources(c);
        if (!r) { restore_screen_size(c); throw std::runtime_error("X11: cannot refresh RandR resources after resize"); }
        crtc.reset(XRRGetCrtcInfo(c.display, r.get(), entry.crtc));
        if (!crtc) { restore_screen_size(c); throw std::runtime_error("X11: fullscreen display disconnected during resize"); }
    }
    if (XRRSetCrtcConfig(c.display, r.get(), entry.crtc, CurrentTime, crtc->x, crtc->y, selected,
                         crtc->rotation, crtc->outputs, crtc->noutput) != RRSetConfigSuccess) {
        restore_screen_size(c);
        throw std::runtime_error("X11: RandR rejected the display mode");
    }
    if (errors.sync()) { restore_screen_size(c); throw std::runtime_error("X11 display configuration changed while applying fullscreen"); }
    w.display_mode_applied = true; w.exclusive_mode = mode;
}
void x11_restore_display(x11_window &w) noexcept {
    if (!w.display_mode_applied) return;
    auto &c = *w.context;
    x11_error_scope errors(c.display);
    auto r = resources(c);
    if (r) XRRSetCrtcConfig(c.display, r.get(), w.exclusive_crtc, CurrentTime,
        w.saved_crtc_position.x, w.saved_crtc_position.y, w.saved_mode, w.saved_rotation,
        w.saved_outputs.data(), static_cast<int>(w.saved_outputs.size()));
    w.display_mode_applied = false;
    errors.sync(); restore_screen_size(c); XFlush(c.display);
}
}
#endif
