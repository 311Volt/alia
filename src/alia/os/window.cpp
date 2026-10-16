#include "window.hpp"
#include "os_context.hpp"
#include "../gfx/bitmap/bitmap.hpp"
#include "../io/mouse_impl.hpp"
#include <utility>

namespace alia {
    namespace {
        thread_local window *current_window = nullptr;
        void validate_constraints(window_size_constraints c) {
            if (c.min.x < 0 || c.min.y < 0 || c.max.x < 0 || c.max.y < 0 || (c.max.x && c.min.x > c.max.x) ||
                (c.max.y && c.min.y > c.max.y))
                throw std::invalid_argument("invalid window size constraints");
        }
        void validate_size(vec2i value) {
            if (value.x <= 0 || value.y <= 0)
                throw std::invalid_argument("window size must be positive");
        }
        void validate_mode(window_fullscreen_mode mode) {
            if (mode != window_fullscreen_mode::windowed && mode != window_fullscreen_mode::fullscreen &&
                mode != window_fullscreen_mode::fullscreen_window)
                throw std::invalid_argument("invalid window fullscreen mode");
        }
    } // namespace
    window::window(vec2i size, window_options options) : window(get_os_context(), size, options) {
    }
    
    window::window(os_context &context, vec2i size, window_options options) {
        if (!context)
            throw std::invalid_argument("window requires a valid OS context");
        validate_size(size);
        validate_constraints(options.size_constraints);
        validate_mode(options.mode);
        if (options.mouse_mode != mouse_mode::normal && options.mouse_mode != mouse_mode::confined &&
            options.mouse_mode != mouse_mode::relative)
            throw std::invalid_argument("invalid window mouse mode");
        if (options.monitor < -1 || options.refresh_rate < 0 || (options.minimized && options.maximized))
            throw std::invalid_argument("invalid window options");
        context_ = context.handle();
        backend_ = context.backend();
        if (options.monitor >= 0) {
            const auto monitors = backend_->monitors.get_or_throw()(context_);
            if (options.monitor >= static_cast<int>(monitors.size()))
                throw std::out_of_range("window monitor index out of range");
        }
        preferred_monitor_ = options.monitor;
        preferred_refresh_rate_ = options.refresh_rate;
        windowed_size_ = size;
        const auto mode = options.mode;
        options.mode = window_fullscreen_mode::windowed;
        handle_ = backend_->create_window.get_or_throw()(context_, size, options);
        if (!handle_)
            throw std::runtime_error("window: native window creation failed");
        try {
            backend_->window_set_owner.get_or_throw()(handle_, this);
            if (mode != window_fullscreen_mode::windowed)
                set_fullscreen_mode(mode);
        } catch (...) {
            release();
            throw;
        }
    }
    
    void window::require_valid() const {
        if (!handle_)
            throw std::logic_error("window is not valid");
    }
    
    void window::release() noexcept {
        detail::mouse_window_destroyed(this);
        if (current_window == this)
            current_window = nullptr;
        if (handle_)
            backend_->destroy_window.get_or_throw()(std::exchange(handle_, nullptr));
    }
    
    window::~window() {
        release();
    }
    
    window::window(window &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)), context_(other.context_), backend_(other.backend_),
          preferred_monitor_(other.preferred_monitor_), preferred_refresh_rate_(other.preferred_refresh_rate_),
          windowed_size_(other.windowed_size_) {
        if (handle_)
            backend_->window_set_owner.get_or_throw()(handle_, this);
        detail::mouse_window_moved(&other, this);
        if (current_window == &other)
            current_window = this;
    }
    
    window &window::operator=(window &&other) noexcept {
        if (this != &other) {
            release();
            handle_ = std::exchange(other.handle_, nullptr);
            context_ = other.context_;
            backend_ = other.backend_;
            preferred_monitor_ = other.preferred_monitor_;
            preferred_refresh_rate_ = other.preferred_refresh_rate_;
            windowed_size_ = other.windowed_size_;
            if (handle_)
                backend_->window_set_owner.get_or_throw()(handle_, this);
            detail::mouse_window_moved(&other, this);
            if (current_window == &other)
                current_window = this;
        }
        return *this;
    }
    
    vec2i window::size() const {
        require_valid();
        return backend_->window_size.get_or_throw()(handle_);
    }
    
    vec2i window::position() const {
        require_valid();
        return backend_->window_position.get_or_throw()(handle_);
    }
    
    cstring_view window::title() const {
        require_valid();
        return backend_->window_title.get_or_throw()(handle_);
    }
    
    bool window::is_focused() const {
        require_valid();
        return backend_->window_is_focused.get_or_throw()(handle_);
    }
    
    bool window::is_minimized() const {
        require_valid();
        return backend_->window_is_minimized.get_or_throw()(handle_);
    }
    
    bool window::is_maximized() const {
        require_valid();
        return backend_->window_is_maximized.get_or_throw()(handle_);
    }
    
    window_fullscreen_mode window::fullscreen_mode() const {
        require_valid();
        return backend_->window_fullscreen_mode.get_or_throw()(handle_);
    }
    
    bool window::is_resizable() const {
        require_valid();
        return backend_->window_is_resizable.get_or_throw()(handle_);
    }
    
    bool window::is_borderless() const {
        require_valid();
        return backend_->window_is_borderless.get_or_throw()(handle_);
    }
    
    bool window::generates_expose_events() const {
        require_valid();
        return backend_->window_generates_expose_events.get_or_throw()(handle_);
    }
    
    int window::refresh_rate() const {
        require_valid();
        return backend_->window_refresh_rate.get_or_throw()(handle_);
    }
    
    int window::monitor() const {
        require_valid();
        return backend_->window_monitor.get_or_throw()(handle_);
    }
    
    window_size_constraints window::size_constraints() const {
        require_valid();
        return backend_->window_constraints.get_or_throw()(handle_);
    }
    
    bool window::is_cursor_visible() const {
        require_valid();
        return backend_->window_is_cursor_visible.get_or_throw()(handle_);
    }
    
    mouse_mode window::requested_mouse_mode() const {
        require_valid();
        return backend_->window_requested_mouse_mode.get_or_throw()(handle_);
    }
    
    mouse_mode window::active_mouse_mode() const {
        require_valid();
        return backend_->window_active_mouse_mode.get_or_throw()(handle_);
    }
    
    void *window::native_handle() const {
        require_valid();
        return backend_->window_native_handle.get_or_throw()(handle_);
    }
    
    void window::minimize() {
        require_valid();
        backend_->window_minimize.get_or_throw()(handle_);
    }
    
    void window::maximize() {
        require_valid();
        backend_->window_maximize.get_or_throw()(handle_);
    }
    
    void window::restore() {
        require_valid();
        backend_->window_restore.get_or_throw()(handle_);
    }
    
    void window::set_title(cstring_view value) {
        require_valid();
        backend_->window_set_title.get_or_throw()(handle_, value);
    }
    
    void window::set_position(vec2i value) {
        require_valid();
        backend_->window_set_position.get_or_throw()(handle_, value);
    }
    
    void window::set_resizable(bool value) {
        require_valid();
        backend_->window_set_resizable.get_or_throw()(handle_, value);
    }
    
    void window::set_borderless(bool value) {
        require_valid();
        backend_->window_set_borderless.get_or_throw()(handle_, value);
    }
    
    void window::apply_size_constraints(bool value) {
        require_valid();
        backend_->window_apply_constraints.get_or_throw()(handle_, value);
    }
    
    void window::show_cursor(bool value) {
        require_valid();
        backend_->window_show_cursor.get_or_throw()(handle_, value);
    }
    
    float window::aspect_ratio() const {
        const auto s = size();
        return s.y ? static_cast<float>(s.x) / s.y : 1.f;
    }
    
    void window::resize(vec2i value) {
        require_valid();
        validate_size(value);
        backend_->window_resize.get_or_throw()(handle_, value);
    }
    
    void window::set_size_constraints(window_size_constraints value) {
        require_valid();
        validate_constraints(value);
        backend_->window_set_constraints.get_or_throw()(handle_, value);
    }
    
    void window::set_fullscreen(bool value) {
        set_fullscreen_mode(value ? window_fullscreen_mode::fullscreen : window_fullscreen_mode::windowed);
    }
    
    void window::set_fullscreen(const display_mode &mode) {
        require_valid();
        validate_size(mode.size);
        if (mode.refresh_rate < 0 || mode.color_depth < 0)
            throw std::invalid_argument("invalid display mode");
        if (fullscreen_mode() == window_fullscreen_mode::windowed)
            windowed_size_ = size();
        backend_->window_set_fullscreen_mode.get_or_throw()(handle_, window_fullscreen_mode::fullscreen, &mode);
    }
    
    void window::set_fullscreen_mode(window_fullscreen_mode value) {
        require_valid();
        validate_mode(value);
        const auto previous = fullscreen_mode();
        if (value == previous && value != window_fullscreen_mode::fullscreen)
            return;
        if (previous == window_fullscreen_mode::windowed)
            windowed_size_ = size();
        if (value == window_fullscreen_mode::fullscreen) {
            const int index = preferred_monitor_ >= 0 ? preferred_monitor_ : monitor();
            const auto desktop = backend_->desktop_mode.get_or_throw()(context_, index);
            const auto modes = backend_->display_modes.get_or_throw()(context_, index);
            const auto selected =
                choose_closest_mode(modes, windowed_size_, preferred_refresh_rate_, 0, desktop).value_or(desktop);
            set_fullscreen(selected);
        } else
            backend_->window_set_fullscreen_mode.get_or_throw()(handle_, value, nullptr);
    }
    
    void window::set_icon(const any_bitmap_view &icon) {
        set_icons({&icon, 1});
    }
    
    void window::set_icons(std::span<const any_bitmap_view> icons) {
        require_valid();
        for (const auto &icon : icons) {
            if (icon.width() <= 0 || icon.height() <= 0 || !icon.line(0))
                throw std::invalid_argument("invalid window icon");
        }
        backend_->window_set_icons.get_or_throw()(handle_, icons);
    }
    
    bool window::set_cursor(system_mouse_cursor value) {
        require_valid();
        return backend_->window_set_system_cursor.get_or_throw()(handle_, value);
    }
    
    bool window::set_cursor(const mouse_cursor &value) {
        require_valid();
        if (!value.impl_ || value.impl_->context != context_ || value.impl_->backend != backend_)
            return false;
        return backend_->window_set_cursor.get_or_throw()(handle_, value.impl_);
    }
    
    bool window::set_cursor_position(vec2i value) {
        require_valid();
        return backend_->window_set_cursor_position.get_or_throw()(handle_, value);
    }
    
    bool window::set_mouse_mode(mouse_mode value) {
        require_valid();
        return backend_->window_set_mouse_mode.get_or_throw()(handle_, value);
    }
    
    event_source &window::get_event_source() {
        require_valid();
        return backend_->window_event_source.get_or_throw()(handle_);
    }
    
    void window::poll() {
        require_valid();
        backend_->poll.get_or_throw()(context_);
    }
    
    void *window::native_display() const {
        require_valid();
        return backend_->native_display.get_or_throw()(context_);
    }
    
    
    window *window::current() {
        return current_window;
    }
    
    
    namespace detail {
        void set_current_window(window *value) noexcept {
            current_window = value;
        }
    } // namespace detail
    
    bool inhibit_screensaver(bool value) {
        auto &c = get_os_context();
        return c.backend()->inhibit_screensaver.get_or_throw()(c.handle(), value);
    }
    
} // namespace alia
