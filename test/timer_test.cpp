#include "alia/alia.hpp"
#include "alia/events/event_queue.hpp"
#include "alia/events/timer_schedule.hpp"

#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using schedule = alia::detail::timer_schedule;

schedule::time_point at(double seconds) {
    return schedule::time_point(std::chrono::duration_cast<schedule::duration>(
        std::chrono::duration<double>(seconds)));
}

TEST(timer_schedule, starts_stopped_and_emits_each_due_tick_without_drift) {
    schedule timer(1.0);
    std::vector<std::int64_t> ticks;
    const auto emit = [&](std::int64_t count) { ticks.push_back(count); };
    EXPECT_FALSE(timer.is_running());
    EXPECT_EQ(timer.count(), 0);
    timer.settle(at(10.0), emit);
    EXPECT_TRUE(ticks.empty());
    timer.start(at(10.0));
    timer.start(at(10.5)); // Already running: preserve the original deadline.
    EXPECT_EQ(timer.deadline(), at(11.0));
    timer.settle(at(10.75), emit);
    EXPECT_TRUE(ticks.empty());
    timer.settle(at(11.0), emit);
    timer.settle(at(13.25), emit);
    EXPECT_EQ(ticks, (std::vector<std::int64_t>{1, 2, 3}));
    EXPECT_EQ(timer.count(), 3);
    EXPECT_EQ(timer.deadline(), at(14.0));
}

TEST(timer_schedule, stop_settles_the_cutoff_and_resume_preserves_remaining_time) {
    schedule timer(1.0);
    std::vector<std::int64_t> ticks;
    const auto emit = [&](std::int64_t count) { ticks.push_back(count); };
    timer.start(at(0.0));
    timer.stop(at(2.25), emit);
    EXPECT_FALSE(timer.is_running());
    EXPECT_EQ(ticks, (std::vector<std::int64_t>{1, 2}));
    timer.stop(at(5.0), emit);
    timer.settle(at(6.0), emit);
    EXPECT_EQ(timer.count(), 2);
    timer.resume(at(10.0));
    timer.resume(at(10.25));
    EXPECT_EQ(timer.deadline(), at(10.75));
    timer.settle(at(10.5), emit);
    EXPECT_EQ(timer.count(), 2);
    timer.settle(at(10.75), emit);
    EXPECT_EQ(timer.count(), 3);
}

TEST(timer_schedule, start_uses_a_full_interval_without_resetting_count) {
    schedule timer(1.0);
    const auto emit = [](std::int64_t) {};
    timer.start(at(0.0));
    timer.stop(at(1.25), emit);
    timer.start(at(10.0));
    EXPECT_EQ(timer.deadline(), at(11.0));
    EXPECT_EQ(timer.count(), 1);
    timer.set_count(40);
    timer.settle(at(11.0), emit);
    EXPECT_EQ(timer.count(), 41);

    schedule initial(1.0);
    initial.resume(at(10.0));
    EXPECT_EQ(initial.deadline(), at(11.0));
}

TEST(timer_schedule, period_changes_settle_old_ticks_and_start_a_fresh_interval) {
    schedule timer(1.0);
    std::vector<std::int64_t> ticks;
    const auto emit = [&](std::int64_t count) { ticks.push_back(count); };
    timer.start(at(0.0));
    timer.set_period(0.5, at(2.25), emit);
    EXPECT_EQ(ticks, (std::vector<std::int64_t>{1, 2}));
    EXPECT_DOUBLE_EQ(timer.period(), 0.5);
    EXPECT_EQ(timer.deadline(), at(2.75));
    timer.settle(at(3.25), emit);
    EXPECT_EQ(ticks, (std::vector<std::int64_t>{1, 2, 3, 4}));

    timer.stop(at(3.5), emit);
    timer.set_period(2.0, at(10.0), emit);
    EXPECT_FALSE(timer.is_running());
    timer.resume(at(20.0));
    EXPECT_EQ(timer.deadline(), at(22.0));
    EXPECT_EQ(timer.count(), 4);
}

TEST(timer_schedule, invalid_period_changes_do_not_settle_or_change_state) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double seconds : {0.0, -1.0, nan, inf,
                                 std::numeric_limits<double>::denorm_min(),
                                 (std::numeric_limits<double>::max)()})
        EXPECT_THROW((schedule(seconds)), std::invalid_argument);

    schedule timer(1.0);
    std::vector<std::int64_t> ticks;
    const auto emit = [&](std::int64_t count) { ticks.push_back(count); };
    timer.start(at(0.0));
    EXPECT_THROW(timer.set_period(0.0, at(3.0), emit), std::invalid_argument);
    EXPECT_EQ(timer.count(), 0);
    EXPECT_TRUE(ticks.empty());
    EXPECT_EQ(timer.deadline(), at(1.0));
    EXPECT_DOUBLE_EQ(timer.period(), 1.0);
    timer.settle(at(3.0), emit);
    EXPECT_EQ(ticks, (std::vector<std::int64_t>{1, 2, 3}));
}

TEST(timer_schedule, rejects_an_unrepresentable_deadline_before_starting) {
    schedule timer(1.0);
    EXPECT_THROW(timer.start((schedule::time_point::max)()), std::invalid_argument);
    EXPECT_FALSE(timer.is_running());
    EXPECT_EQ(timer.count(), 0);
}

// Worker integration tests use bounded waits, with no assertions about exact
// wake-up latency. Exact cadence and phase behavior are covered above.
TEST(timer, wakes_a_queue_and_stop_preserves_pending_events) {
    alia::timer timer(0.01);
    alia::event_queue events;
    events.register_source(&timer.get_event_source());
    EXPECT_FALSE(timer.is_running());
    EXPECT_EQ(timer.count(), 0);
    timer.start();
    ASSERT_TRUE(events.wait_for_event(2.0));
    timer.stop();
    EXPECT_FALSE(timer.is_running());
    const auto stopped_count = timer.count();
    ASSERT_GT(stopped_count, 0);

    std::int64_t expected = 1;
    while (!events.empty()) {
        const auto event = events.pop();
        ASSERT_NE(event.get_if<alia::timer_event>(), nullptr);
        EXPECT_EQ(event.get_if<alia::timer_event>()->count, expected++);
        EXPECT_EQ(event.meta.source, &timer.get_event_source());
        EXPECT_GE(event.meta.timestamp, 0.0);
    }
    EXPECT_EQ(expected - 1, stopped_count);
    EXPECT_FALSE(events.wait_for_event(0.03));
    EXPECT_EQ(timer.count(), stopped_count);
}

TEST(timer, sends_the_same_tick_counts_to_every_registered_queue) {
    alia::timer timer(0.01);
    alia::event_queue first;
    alia::event_queue second;
    first.register_source(&timer.get_event_source());
    second.register_source(&timer.get_event_source());
    timer.start();
    ASSERT_TRUE(first.wait_for_event(2.0));
    ASSERT_TRUE(second.wait_for_event(2.0));
    timer.stop();
    ASSERT_EQ(first.size(), second.size());
    while (!first.empty()) {
        const auto a = first.pop();
        const auto b = second.pop();
        ASSERT_NE(a.get_if<alia::timer_event>(), nullptr);
        ASSERT_NE(b.get_if<alia::timer_event>(), nullptr);
        EXPECT_EQ(a.get_if<alia::timer_event>()->count, b.get_if<alia::timer_event>()->count);
        EXPECT_DOUBLE_EQ(a.meta.timestamp, b.meta.timestamp);
    }
}

TEST(timer, counts_without_subscribers) {
    alia::timer counter(0.01);
    alia::timer gate(0.05);
    alia::event_queue events;
    events.register_source(&gate.get_event_source());
    counter.start();
    gate.start();
    ASSERT_TRUE(events.wait_for_event(2.0));
    counter.stop();
    gate.stop();
    EXPECT_GT(counter.count(), 0);
}

TEST(timer, changing_a_long_period_wakes_the_scheduler) {
    alia::timer timer(60.0);
    alia::event_queue events;
    events.register_source(&timer.get_event_source());
    timer.start();
    timer.set_period(0.01);
    ASSERT_TRUE(events.wait_for_event(2.0));
    timer.stop();
    EXPECT_DOUBLE_EQ(timer.period(), 0.01);
}

TEST(timer, moving_a_running_handle_preserves_source_address_and_registration) {
    alia::timer original(0.01);
    auto* source = &original.get_event_source();
    alia::event_queue events;
    events.register_source(source);
    original.start();
    alia::timer moved(std::move(original));
    EXPECT_EQ(&moved.get_event_source(), source);
    ASSERT_TRUE(events.wait_for_event(2.0));
    moved.stop();
    const auto event = events.pop();
    ASSERT_NE(event.get_if<alia::timer_event>(), nullptr);
    EXPECT_EQ(event.meta.source, source);
}

TEST(timer, move_assignment_unregisters_the_old_source_and_preserves_the_new_one) {
    alia::timer original(0.01);
    alia::timer destination(60.0);
    alia::event_queue original_events;
    alia::event_queue destination_events;
    auto* source = &original.get_event_source();
    original_events.register_source(source);
    destination_events.register_source(&destination.get_event_source());
    original.start();
    destination.start();
    destination = std::move(original);
    EXPECT_EQ(&destination.get_event_source(), source);
    ASSERT_TRUE(original_events.wait_for_event(2.0));
    destination.stop();
    EXPECT_TRUE(destination_events.empty());
    const auto event = original_events.pop();
    EXPECT_EQ(event.meta.source, source);
}

TEST(timer, destruction_unregisters_sources_and_allows_a_new_scheduler) {
    alia::event_queue events;
    {
        alia::timer timer(0.01);
        events.register_source(&timer.get_event_source());
        timer.start();
        ASSERT_TRUE(events.wait_for_event(2.0));
    }
    // Queued payloads remain valid; their metadata source pointer must not be dereferenced.
    ASSERT_FALSE(events.empty());
    while (!events.empty()) {
        const auto event = events.pop();
        EXPECT_NE(event.get_if<alia::timer_event>(), nullptr);
    }
    EXPECT_FALSE(events.wait_for_event(0.03));

    alia::timer replacement(0.01);
    events.register_source(&replacement.get_event_source());
    replacement.resume();
    ASSERT_TRUE(events.wait_for_event(2.0));
    replacement.stop();
}

TEST(timer, other_timer_owners_keep_the_shared_scheduler_alive) {
    auto first = std::make_unique<alia::timer>(60.0);
    alia::timer second(0.01);
    alia::event_queue events;
    events.register_source(&second.get_event_source());
    first->start();
    second.start();
    first.reset();
    ASSERT_TRUE(events.wait_for_event(2.0));
    second.stop();
}

TEST(timer, queues_can_unregister_and_be_destroyed_while_a_timer_is_running) {
    alia::timer timer(0.01);
    timer.start();
    {
        alia::event_queue transient;
        transient.register_source(&timer.get_event_source());
        ASSERT_TRUE(transient.wait_for_event(2.0));
    }
    alia::event_queue events;
    events.register_source(&timer.get_event_source());
    ASSERT_TRUE(events.wait_for_event(2.0));
    events.unregister_source(&timer.get_event_source());
    timer.stop();
    while (!events.empty())
        events.discard();
    EXPECT_FALSE(events.wait_for_event(0.03));
}

TEST(timer, public_controls_preserve_count_and_reject_invalid_periods) {
    alia::timer timer(60.0);
    timer.set_count(40);
    timer.start();
    EXPECT_THROW(timer.set_period(0.0), std::invalid_argument);
    EXPECT_TRUE(timer.is_running());
    EXPECT_DOUBLE_EQ(timer.period(), 60.0);
    timer.stop();
    EXPECT_EQ(timer.count(), 40);
    timer.resume();
    EXPECT_TRUE(timer.is_running());
    timer.stop();
    EXPECT_EQ(timer.count(), 40);
    timer.set_count(0);
    EXPECT_EQ(timer.count(), 0);
    EXPECT_THROW((alia::timer(std::numeric_limits<double>::quiet_NaN())),
                 std::invalid_argument);
}

} // namespace
