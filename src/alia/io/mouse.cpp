#include "mouse.hpp"
#include "mouse_impl.hpp"

#include "../events/event_source_impl.hpp"
#include "../os/platform.hpp"
#include "../os/window_events.hpp"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <stdexcept>

namespace alia {

struct detail_mouse_state_access {
    mouse_state state;
    bool position_baseline = false;
    int wheel_precision = 1;
    std::int64_t z_remainder = 0;
    std::int64_t w_remainder = 0;

    void set_position(window *owner, vec2i position) {
        state.position_ = position;
        state.position_f_ = vec2f(position);
        state.window_ = owner;
        state.pressure_ = 0.0f;
    }

    vec2i update_position(window *owner, vec2i position, float pressure) {
        const vec2i delta = position_baseline && state.window_ == owner
            ? position - state.position_ : vec2i{};
        set_position(owner, position);
        state.pressure_ = pressure;
        position_baseline = true;
        return delta;
    }

    int consume_wheel_delta(int raw_delta, std::int64_t &remainder) {
        constexpr std::int64_t native_wheel_delta = 120;
        const std::int64_t scaled =
            remainder + static_cast<std::int64_t>(raw_delta) * wheel_precision;
        const auto units = scaled / native_wheel_delta;
        remainder = scaled - units * native_wheel_delta;
        return static_cast<int>(units);
    }

    int &wheel_axis(unsigned axis) {
        return axis == 2 ? state.z_ : state.w_;
    }

    void add_wheel_delta(int dz, int dw) {
        state.z_ += dz;
        state.w_ += dw;
    }

    void set_button(mouse_button button, bool down) {
        const auto index = static_cast<unsigned>(button);
        if (index < 1 || index > 32)
            return;
        const auto bit = std::uint32_t{1} << (index - 1);
        if (down)
            state.buttons_ |= bit;
        else
            state.buttons_ &= ~bit;
    }

    void replace_window(window *from, window *to) {
        if (state.window_ == from)
            state.window_ = to;
    }

    void clear_window(window *owner) {
        if (state.window_ == owner) {
            state.window_ = nullptr;
            position_baseline = false;
        }
    }
};

namespace {

detail_mouse_state_access &tracked_mouse() {
    static detail_mouse_state_access tracker;
    return tracker;
}

std::mutex &mouse_mutex() {
    static std::mutex mutex;
    return mutex;
}

constexpr bool valid_button(unsigned button) {
    return button >= 1 && button <= 32;
}

constexpr std::uint32_t button_bit(unsigned button) {
    return valid_button(button) ? (std::uint32_t{1} << (button - 1)) : 0;
}

} // namespace

mouse_cursor::mouse_cursor(const any_bitmap_view &bitmap, vec2i hotspot)
    : impl_(detail::create_platform_mouse_cursor(bitmap, hotspot)) {
    if (!impl_)
        throw std::runtime_error("mouse_cursor: native cursor creation failed");
}

mouse_cursor::~mouse_cursor() = default;
mouse_cursor::mouse_cursor(mouse_cursor &&) noexcept = default;
mouse_cursor &mouse_cursor::operator=(mouse_cursor &&) noexcept = default;

mouse_state::mouse_state() = default;
vec2i mouse_state::position() const { return position_; }
vec2f mouse_state::position_f() const { return position_f_; }
int mouse_state::z() const { return z_; }
int mouse_state::w() const { return w_; }

int mouse_state::axis(unsigned index) const {
    switch (index) {
    case 0: return position_.x;
    case 1: return position_.y;
    case 2: return z_;
    case 3: return w_;
    default: return 0;
    }
}

std::uint32_t mouse_state::button_mask() const { return buttons_; }
float mouse_state::pressure() const { return pressure_; }
window *mouse_state::associated_window() const { return window_; }

bool mouse_state::is_button_down(mouse_button button) const {
    return is_button_down(static_cast<unsigned>(button));
}

bool mouse_state::is_button_down(unsigned button) const {
    const auto bit = button_bit(button);
    return bit != 0 && (buttons_ & bit) != 0;
}

mouse_state get_mouse_state() {
    const std::lock_guard lock(mouse_mutex());
    return tracked_mouse().state;
}

bool is_mouse_button_down(mouse_button button) {
    return is_mouse_button_down(static_cast<unsigned>(button));
}

bool is_mouse_button_down(unsigned button) {
    const std::lock_guard lock(mouse_mutex());
    return tracked_mouse().state.is_button_down(button);
}

bool is_mouse_button_down(const mouse_state &state, mouse_button button) {
    return state.is_button_down(button);
}

bool is_mouse_button_down(const mouse_state &state, unsigned button) {
    return state.is_button_down(button);
}

unsigned get_mouse_num_axes() { return detail::platform_mouse_num_axes(); }
unsigned get_mouse_num_buttons() { return detail::platform_mouse_num_buttons(); }

bool can_get_mouse_cursor_position() {
    return detail::platform_can_get_mouse_cursor_position();
}

std::optional<vec2i> get_mouse_cursor_position() {
    return detail::platform_get_mouse_cursor_position();
}

namespace {
bool set_tracked_mouse_axis(unsigned axis, int value) {
    if (axis < 2 || axis > 3)
        return false;
    int delta = 0;
    {
        const std::lock_guard lock(mouse_mutex());
        auto &tracker = tracked_mouse();
        int &current = tracker.wheel_axis(axis);
        delta = value - current;
        if (axis == 2)
            tracker.z_remainder = 0;
        else
            tracker.w_remainder = 0;
        if (delta == 0)
            return true;
        current = value;
    }
    get_platform_event_source().emit(mouse_axis_set_event{axis, value, delta});
    return true;
}
} // namespace

bool set_mouse_z(int value) { return set_mouse_axis(2, value); }
bool set_mouse_w(int value) { return set_mouse_axis(3, value); }

bool set_mouse_axis(unsigned axis, int value) {
    if (axis < 2 || axis > 3 || axis >= get_mouse_num_axes())
        return false;
    return set_tracked_mouse_axis(axis, value);
}

int get_mouse_wheel_precision() {
    const std::lock_guard lock(mouse_mutex());
    return tracked_mouse().wheel_precision;
}

void set_mouse_wheel_precision(int precision) {
    const std::lock_guard lock(mouse_mutex());
    tracked_mouse().wheel_precision = (std::max)(1, precision);
}

bool ungrab_mouse() { return detail::platform_ungrab_mouse(); }

namespace detail {

void mouse_handle_axes(
    window *owner, event_source &source, vec2i position,
    int raw_dz, int raw_dw, float pressure) {
    mouse_axes_event event{};
    {
        const std::lock_guard lock(mouse_mutex());
        auto &tracker = tracked_mouse();
        event.delta = tracker.update_position(owner, position, pressure);
        event.dz = tracker.consume_wheel_delta(raw_dz, tracker.z_remainder);
        event.dw = tracker.consume_wheel_delta(raw_dw, tracker.w_remainder);
        tracker.add_wheel_delta(event.dz, event.dw);
        event.position = tracker.state.position();
        event.z = tracker.state.z();
        event.w = tracker.state.w();
        event.pressure = tracker.state.pressure();
    }
    if (event.delta != vec2i{} || event.dz != 0 || event.dw != 0)
        source.emit(event);
}

void mouse_handle_wheel(
    window *owner, event_source &source, vec2i position,
    int raw_dz, int raw_dw, float pressure) {
    mouse_axes_event event{};
    {
        const std::lock_guard lock(mouse_mutex());
        auto &tracker = tracked_mouse();
        tracker.update_position(owner, position, pressure);
        event.dz = tracker.consume_wheel_delta(raw_dz, tracker.z_remainder);
        event.dw = tracker.consume_wheel_delta(raw_dw, tracker.w_remainder);
        tracker.add_wheel_delta(event.dz, event.dw);
        event.position = tracker.state.position();
        event.z = tracker.state.z();
        event.w = tracker.state.w();
        event.pressure = tracker.state.pressure();
    }
    if (event.dz != 0 || event.dw != 0)
        source.emit(event);
}

void mouse_handle_button(
    window *owner, event_source &source, mouse_button button, bool down,
    vec2i position, float pressure) {
    mouse_button_down_event down_event{};
    mouse_button_up_event up_event{};
    {
        const std::lock_guard lock(mouse_mutex());
        auto &tracker = tracked_mouse();
        tracker.update_position(owner, position, pressure);
        tracker.set_button(button, down);
        down_event = {
            button, tracker.state.position(), tracker.state.z(), tracker.state.w(),
            tracker.state.pressure()};
        up_event = {
            button, tracker.state.position(), tracker.state.z(), tracker.state.w(),
            tracker.state.pressure()};
    }
    if (down)
        source.emit(down_event);
    else
        source.emit(up_event);
}

void mouse_handle_warped(
    window *owner, event_source &source, vec2i position, float pressure) {
    mouse_warped_event event{};
    {
        const std::lock_guard lock(mouse_mutex());
        auto &tracker = tracked_mouse();
        event.delta = tracker.update_position(owner, position, pressure);
        event.position = tracker.state.position();
        event.z = tracker.state.z();
        event.w = tracker.state.w();
        event.pressure = tracker.state.pressure();
    }
    source.emit(event);
}

void mouse_handle_enter(
    window *owner, event_source &source, vec2i position) {
    mouse_enter_window_event event{};
    {
        const std::lock_guard lock(mouse_mutex());
        auto &tracker = tracked_mouse();
        tracker.set_position(owner, position);
        tracker.position_baseline = true;
        event = {position, tracker.state.z(), tracker.state.w()};
    }
    source.emit(event);
}

void mouse_handle_leave(
    window *owner, event_source &source, vec2i position) {
    mouse_leave_window_event event{};
    {
        const std::lock_guard lock(mouse_mutex());
        auto &tracker = tracked_mouse();
        tracker.set_position(owner, position);
        event = {position, tracker.state.z(), tracker.state.w()};
        tracker.clear_window(owner);
    }
    source.emit(event);
}

void mouse_handle_relative(event_source &source, vec2i delta) {
    if (delta != vec2i{})
        source.emit(mouse_relative_event{delta});
}

void mouse_update_position(window *owner, vec2i position) {
    const std::lock_guard lock(mouse_mutex());
    tracked_mouse().update_position(owner, position, 0.0f);
}

void mouse_window_moved(window *from, window *to) {
    const std::lock_guard lock(mouse_mutex());
    tracked_mouse().replace_window(from, to);
}

void mouse_window_destroyed(window *owner) {
    const std::lock_guard lock(mouse_mutex());
    tracked_mouse().clear_window(owner);
}

void reset_mouse_state_for_testing() {
    const std::lock_guard lock(mouse_mutex());
    tracked_mouse() = detail_mouse_state_access{};
}

bool set_mouse_axis_for_testing(unsigned axis, int value) {
    return set_tracked_mouse_axis(axis, value);
}

#ifndef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
std::shared_ptr<const mouse_cursor_impl> create_platform_mouse_cursor(
    const any_bitmap_view &, vec2i) {
    throw std::runtime_error("mouse_cursor: unsupported platform");
}
unsigned platform_mouse_num_axes() { return 0; }
unsigned platform_mouse_num_buttons() { return 0; }
bool platform_can_get_mouse_cursor_position() { return false; }
std::optional<vec2i> platform_get_mouse_cursor_position() { return std::nullopt; }
bool platform_ungrab_mouse() { return false; }
bool platform_set_mouse_mode(
    void *, void *, event_source &, mouse_mode requested) {
    return requested == mouse_mode::normal;
}
mouse_mode platform_requested_mouse_mode(void *) { return mouse_mode::normal; }
mouse_mode platform_active_mouse_mode(void *) { return mouse_mode::normal; }
void platform_mouse_window_focus_changed(void *, bool, bool) {}
void platform_refresh_mouse_bounds(void *) {}
void platform_mouse_window_destroyed(void *) {}
bool platform_mouse_handle_raw_input(void *, void *) { return false; }
#endif

} // namespace detail
} // namespace alia
