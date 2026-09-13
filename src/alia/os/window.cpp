#include "window.hpp"
#include "../gfx/bitmap/bitmap.hpp"
#include "../io/mouse_impl.hpp"
#include <stdexcept>
#include <vector>
#include <mutex>

// Forward declarations for each compiled-in backend
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
namespace alia { void register_win32_window_backend(); }
#endif
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_X11
namespace alia { void register_x11_window_backend(); }
#endif

namespace alia {

// ── Backend registry ──────────────────────────────────────────────────

static std::vector<window_backend_entry>& backend_registry() {
    static std::vector<window_backend_entry> reg;
    return reg;
}

void register_window_backend(window_backend_entry entry) {
    backend_registry().push_back(std::move(entry));
}

static void init_window_backends() {
    static std::once_flag flag;
    std::call_once(flag, []() {
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
        register_win32_window_backend();
#endif
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_X11
        register_x11_window_backend();
#endif
    });
}

// ── Current window (most recently focused) ────────────────────────────

static thread_local window* s_current_window = nullptr;

// ── window ────────────────────────────────────────────────────────────

window::window(vec2i size, window_options opts) {
    init_window_backends();
    for (const auto& e : backend_registry()) {
        if (auto i = e.create(size, opts)) {
            impl_ = std::move(i);
            impl_->set_owner(this);
            return;
        }
    }
    throw std::runtime_error("window: no platform backend available");
}

window::window(window&& o) noexcept : impl_(std::move(o.impl_)) {
    if (impl_)
        impl_->set_owner(this);
    detail::mouse_window_moved(&o, this);
    if (s_current_window == &o)
        s_current_window = this;
}

window& window::operator=(window&& o) noexcept {
    if (this != &o) {
        detail::mouse_window_destroyed(this);
        if (s_current_window == this)
            s_current_window = nullptr;
        impl_.reset();
        impl_ = std::move(o.impl_);
        if (impl_)
            impl_->set_owner(this);
        detail::mouse_window_moved(&o, this);
        if (s_current_window == &o)
            s_current_window = this;
    }
    return *this;
}

window::~window() {
    detail::mouse_window_destroyed(this);
    if (s_current_window == this)
        s_current_window = nullptr;
}

void window::set_icon(const any_bitmap_view &icon) {
    impl_->set_icons(std::span<const any_bitmap_view>(&icon, 1));
}

void window::set_icons(std::span<const any_bitmap_view> icons) {
    impl_->set_icons(icons);
}

window* window::current() { return s_current_window; }

namespace detail {
void set_current_window(window *value) noexcept {
    s_current_window = value;
}
}

} // namespace alia
