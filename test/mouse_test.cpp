#include "alia/events/event_queue.hpp"
#include "alia/events/event_source_impl.hpp"
#include "alia/io/mouse.hpp"
#include "alia/io/mouse_impl.hpp"
#include "alia/os/platform.hpp"
#include "alia/os/window.hpp"

#include <cstdint>
#include <gtest/gtest.h>

namespace {

alia::window *fake_window(std::uintptr_t value) {
    return reinterpret_cast<alia::window *>(value);
}

TEST(mouse, button_indices_are_one_based_and_header_definitions_agree) {
    static_assert(static_cast<unsigned>(alia::mouse_button::left) == 1);
    static_assert(static_cast<unsigned>(alia::mouse_button::right) == 2);
    static_assert(static_cast<unsigned>(alia::mouse_button::middle) == 3);
    static_assert(static_cast<unsigned>(alia::mouse_button::x1) == 4);
    static_assert(static_cast<unsigned>(alia::mouse_button::x2) == 5);

    alia::mouse_state state;
    EXPECT_FALSE(state.is_button_down(0));
    EXPECT_FALSE(state.is_button_down(6));
    EXPECT_EQ(state.axis(4), 0);
}

TEST(mouse, tracks_state_before_publishing_window_events) {
    alia::detail::reset_mouse_state_for_testing();
    alia::event_source source;
    alia::event_queue queue;
    queue.register_source(&source);
    auto *window = fake_window(1);

    alia::detail::mouse_handle_enter(window, source, {10, 20});
    alia::detail::mouse_handle_axes(window, source, {13, 25});
    alia::detail::mouse_handle_button(
        window, source, alia::mouse_button::x2, true, {13, 25});

    const auto state = alia::get_mouse_state();
    EXPECT_EQ(state.position(), alia::vec2i(13, 25));
    EXPECT_EQ(state.associated_window(), window);
    EXPECT_TRUE(state[alia::mouse_button::x2]);
    EXPECT_FLOAT_EQ(state.pressure(), 0.0f);

    ASSERT_EQ(queue.size(), 3u);
    queue.discard();
    const auto axes = queue.pop();
    ASSERT_NE(axes.get_if<alia::mouse_axes_event>(), nullptr);
    EXPECT_EQ(axes.get_if<alia::mouse_axes_event>()->delta, alia::vec2i(3, 5));
    const auto button = queue.pop();
    ASSERT_NE(button.get_if<alia::mouse_button_down_event>(), nullptr);
    EXPECT_TRUE(alia::get_mouse_state()[alia::mouse_button::x2]);
}

TEST(mouse, accumulates_fractional_wheel_messages_independently) {
    alia::detail::reset_mouse_state_for_testing();
    alia::set_mouse_wheel_precision(1);
    alia::event_source source;
    alia::event_queue queue;
    queue.register_source(&source);
    auto *window = fake_window(2);

    for (int i = 0; i < 3; ++i)
        alia::detail::mouse_handle_wheel(window, source, {}, 30, -30);
    EXPECT_TRUE(queue.empty());
    alia::detail::mouse_handle_wheel(window, source, {}, 30, -30);

    ASSERT_EQ(queue.size(), 1u);
    const auto event = queue.pop();
    const auto *axes = event.get_if<alia::mouse_axes_event>();
    ASSERT_NE(axes, nullptr);
    EXPECT_EQ(axes->dz, 1);
    EXPECT_EQ(axes->dw, -1);
    EXPECT_EQ(alia::get_mouse_state().z(), 1);
    EXPECT_EQ(alia::get_mouse_state().w(), -1);
}

TEST(mouse, precision_changes_only_affect_subsequent_input) {
    alia::detail::reset_mouse_state_for_testing();
    alia::event_source source;
    alia::set_mouse_wheel_precision(1);
    alia::detail::mouse_handle_wheel(fake_window(3), source, {}, 60, 0);
    EXPECT_EQ(alia::get_mouse_state().z(), 0);

    alia::set_mouse_wheel_precision(2);
    alia::detail::mouse_handle_wheel(fake_window(3), source, {}, 30, 0);
    EXPECT_EQ(alia::get_mouse_state().z(), 1);
    EXPECT_EQ(alia::get_mouse_wheel_precision(), 2);
    alia::set_mouse_wheel_precision(0);
    EXPECT_EQ(alia::get_mouse_wheel_precision(), 1);
}

TEST(mouse, axis_resets_clear_remainders_and_use_only_platform_source) {
    alia::detail::reset_mouse_state_for_testing();
    alia::set_mouse_wheel_precision(1);
    alia::event_source window_source;
    alia::event_queue window_events;
    alia::event_queue platform_events;
    window_events.register_source(&window_source);
    platform_events.register_source(&alia::get_platform_event_source());

    alia::detail::mouse_handle_wheel(
        fake_window(4), window_source, {}, 60, 0);
    EXPECT_TRUE(window_events.empty());
    ASSERT_TRUE(alia::detail::set_mouse_axis_for_testing(2, 7));
    EXPECT_TRUE(window_events.empty());
    ASSERT_EQ(platform_events.size(), 1u);
    const auto reset = platform_events.pop();
    const auto *axis = reset.get_if<alia::mouse_axis_set_event>();
    ASSERT_NE(axis, nullptr);
    EXPECT_EQ(axis->axis, 2u);
    EXPECT_EQ(axis->value, 7);
    EXPECT_EQ(axis->delta, 7);

    alia::detail::mouse_handle_wheel(
        fake_window(4), window_source, {}, 60, 0);
    EXPECT_EQ(alia::get_mouse_state().z(), 7);
}

TEST(mouse, warps_are_distinct_and_native_echoes_are_filtered) {
    alia::detail::reset_mouse_state_for_testing();
    alia::event_source source;
    alia::event_queue queue;
    queue.register_source(&source);
    auto *window = fake_window(5);
    alia::detail::mouse_handle_enter(window, source, {2, 3});
    queue.clear();
    alia::detail::mouse_handle_warped(window, source, {20, 30});

    ASSERT_EQ(queue.size(), 1u);
    const auto event = queue.pop();
    const auto *warped = event.get_if<alia::mouse_warped_event>();
    ASSERT_NE(warped, nullptr);
    EXPECT_EQ(warped->delta, alia::vec2i(18, 27));

    alia::detail::mouse_warp_filter filter;
    filter.record({10, 10});
    filter.record({20, 20});
    EXPECT_TRUE(filter.should_suppress({20, 20}));
    EXPECT_TRUE(filter.empty());
    filter.record({30, 30});
    EXPECT_FALSE(filter.should_suppress({31, 30}));
    EXPECT_TRUE(filter.empty());
}

TEST(mouse, window_associations_follow_moves_and_clear_on_destruction) {
    alia::detail::reset_mouse_state_for_testing();
    alia::event_source source;
    auto *first = fake_window(6);
    auto *second = fake_window(7);
    alia::detail::mouse_handle_enter(first, source, {4, 5});
    alia::detail::mouse_window_moved(first, second);
    EXPECT_EQ(alia::get_mouse_state().associated_window(), second);
    alia::detail::mouse_window_destroyed(second);
    EXPECT_EQ(alia::get_mouse_state().associated_window(), nullptr);
}

} // namespace
