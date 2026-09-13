#ifndef ALIA_IO_MOUSE_IMPL_HPP
#define ALIA_IO_MOUSE_IMPL_HPP

#include "mouse.hpp"
#include "../events/event_source.hpp"

#include <memory>
#include <algorithm>
#include <vector>

namespace alia {

class any_bitmap_view;
class window;

namespace detail {

// Correlates SetCursorPos results with later coalesced WM_MOUSEMOVE messages.
// A nonmatching message is physical movement and invalidates older candidates.
class mouse_warp_filter {
public:
    void record(vec2i position) { pending_.push_back(position); }

    bool should_suppress(vec2i position) {
        const auto found = std::find(pending_.begin(), pending_.end(), position);
        if (found == pending_.end()) {
            pending_.clear();
            return false;
        }
        pending_.erase(pending_.begin(), found + 1);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept { return pending_.empty(); }

private:
    std::vector<vec2i> pending_;
};

class mouse_cursor_impl {
public:
    virtual ~mouse_cursor_impl() = default;
    [[nodiscard]] virtual void *native_handle() const noexcept = 0;
};

std::shared_ptr<const mouse_cursor_impl> create_platform_mouse_cursor(
    const any_bitmap_view &bitmap, vec2i hotspot);

unsigned platform_mouse_num_axes();
unsigned platform_mouse_num_buttons();
bool platform_can_get_mouse_cursor_position();
std::optional<vec2i> platform_get_mouse_cursor_position();
bool platform_ungrab_mouse();

void mouse_handle_axes(
    window *owner, event_source &source, vec2i position,
    int raw_dz = 0, int raw_dw = 0, float pressure = 0.0f);
void mouse_handle_wheel(
    window *owner, event_source &source, vec2i position,
    int raw_dz, int raw_dw, float pressure = 0.0f);
void mouse_handle_button(
    window *owner, event_source &source, mouse_button button, bool down,
    vec2i position, float pressure = 0.0f);
void mouse_handle_warped(
    window *owner, event_source &source, vec2i position,
    float pressure = 0.0f);
void mouse_handle_enter(
    window *owner, event_source &source, vec2i position);
void mouse_handle_leave(
    window *owner, event_source &source, vec2i position);
void mouse_handle_relative(event_source &source, vec2i delta);
void mouse_update_position(window *owner, vec2i position);

void mouse_window_moved(window *from, window *to);
void mouse_window_destroyed(window *owner);

// Test support for the backend-independent state machine.
void reset_mouse_state_for_testing();
bool set_mouse_axis_for_testing(unsigned axis, int value);

bool platform_set_mouse_mode(
    void *owner_token, void *native_window, event_source &source,
    mouse_mode requested);
mouse_mode platform_requested_mouse_mode(void *owner_token);
mouse_mode platform_active_mouse_mode(void *owner_token);
void platform_mouse_window_focus_changed(
    void *owner_token, bool focused, bool minimized);
void platform_refresh_mouse_bounds(void *owner_token);
void platform_mouse_window_destroyed(void *owner_token);
bool platform_mouse_handle_raw_input(void *owner_token, void *raw_input);

} // namespace detail
} // namespace alia

#endif // ALIA_IO_MOUSE_IMPL_HPP
