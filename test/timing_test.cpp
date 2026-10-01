#include "alia/core/timing.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

namespace {

TEST(frame_clock, measures_first_delta_and_numbers_ticks_from_one) {
    alia::frame_clock clock(10.0);
    const auto first = clock.tick(10.25);
    EXPECT_EQ(first.tick, 1u);
    EXPECT_DOUBLE_EQ(first.delta, 0.25);
    EXPECT_DOUBLE_EQ(first.elapsed, 0.25);

    const auto second = clock.tick(12.0);
    EXPECT_EQ(second.tick, 2u);
    EXPECT_DOUBLE_EQ(second.delta, 1.75);
    EXPECT_DOUBLE_EQ(second.elapsed, 2.0);
    EXPECT_DOUBLE_EQ(clock.tick(12.0).delta, 0.0);

    clock.reset(0.0);
    const auto reset = clock.tick(0.5);
    EXPECT_EQ(reset.tick, 1u);
    EXPECT_DOUBLE_EQ(reset.delta, 0.5);
    EXPECT_DOUBLE_EQ(reset.elapsed, 0.5);
}

TEST(frame_clock, rejects_invalid_observations_without_advancing) {
    alia::frame_clock clock(1.0);
    EXPECT_THROW(clock.tick(0.0), std::invalid_argument);
    EXPECT_THROW(clock.tick(std::numeric_limits<double>::infinity()), std::invalid_argument);
    EXPECT_THROW(clock.reset(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    const auto tick = clock.tick(2.0);
    EXPECT_EQ(tick.tick, 1u);
    EXPECT_DOUBLE_EQ(tick.delta, 1.0);
    EXPECT_THROW((alia::frame_clock(std::numeric_limits<double>::quiet_NaN())),
                 std::invalid_argument);
}

TEST(frame_statistics, guards_empty_and_zero_duration_reports) {
    EXPECT_DOUBLE_EQ(alia::frame_statistics{}.fps(), 0.0);
    EXPECT_DOUBLE_EQ(alia::frame_statistics{}.mean_frame_time(), 0.0);
    const alia::frame_statistics empty{0, 2.0};
    const alia::frame_statistics simultaneous{3, 0.0};
    EXPECT_DOUBLE_EQ(empty.fps(), 0.0);
    EXPECT_DOUBLE_EQ(empty.mean_frame_time(), 0.0);
    EXPECT_DOUBLE_EQ(simultaneous.fps(), 0.0);
    EXPECT_DOUBLE_EQ(simultaneous.mean_frame_time(), 0.0);
}

TEST(fps_counter, publishes_completed_intervals_and_retains_whole_run_totals) {
    alia::fps_counter counter(1.0, 0.0);
    EXPECT_EQ(counter.interval().frames, 0u);
    EXPECT_EQ(counter.total().frames, 0u);
    EXPECT_FALSE(counter.count_frame(0.25));
    EXPECT_FALSE(counter.count_frame(0.5));
    EXPECT_EQ(counter.interval().frames, 0u);
    EXPECT_TRUE(counter.count_frame(1.0));
    EXPECT_EQ(counter.interval().frames, 3u);
    EXPECT_DOUBLE_EQ(counter.interval().elapsed, 1.0);
    EXPECT_DOUBLE_EQ(counter.interval().fps(), 3.0);

    EXPECT_FALSE(counter.count_frame(1.5));
    EXPECT_EQ(counter.interval().frames, 3u);
    EXPECT_DOUBLE_EQ(counter.interval().elapsed, 1.0);
    EXPECT_EQ(counter.total().frames, 4u);
    EXPECT_DOUBLE_EQ(counter.total().elapsed, 1.5);
    EXPECT_TRUE(counter.count_frame(2.0));
    EXPECT_EQ(counter.interval().frames, 2u);
    EXPECT_EQ(counter.total().frames, 5u);
    EXPECT_DOUBLE_EQ(counter.total().elapsed, 2.0);
}

TEST(fps_counter, uses_full_duration_for_a_long_gap_and_one_report_per_observation) {
    alia::fps_counter counter(0.25, 0.0);
    EXPECT_TRUE(counter.count_frame(2.0));
    EXPECT_EQ(counter.interval().frames, 1u);
    EXPECT_DOUBLE_EQ(counter.interval().elapsed, 2.0);
    EXPECT_DOUBLE_EQ(counter.interval().fps(), 0.5);
    EXPECT_FALSE(counter.count_frame(2.0));
    EXPECT_EQ(counter.interval().frames, 1u);
    EXPECT_EQ(counter.total().frames, 2u);
    EXPECT_TRUE(counter.count_frame(2.25));
    EXPECT_EQ(counter.interval().frames, 2u);
    EXPECT_DOUBLE_EQ(counter.interval().elapsed, 0.25);
}

TEST(fps_counter, averages_durations_instead_of_instantaneous_fps) {
    alia::fps_counter counter(1.0, 0.0);
    counter.count_frame(0.01);
    counter.count_frame(0.04);
    EXPECT_DOUBLE_EQ(counter.total().fps(), 50.0);
    EXPECT_DOUBLE_EQ(counter.total().mean_frame_time(), 0.02);
}

TEST(fps_counter, reset_clears_reports_and_may_rebase_time) {
    alia::fps_counter counter(1.0, 10.0);
    counter.count_frame(12.0);
    counter.reset(0.0);
    EXPECT_EQ(counter.interval().frames, 0u);
    EXPECT_EQ(counter.total().frames, 0u);
    EXPECT_DOUBLE_EQ(counter.total().elapsed, 0.0);
    EXPECT_FALSE(counter.count_frame(0.0));
    EXPECT_DOUBLE_EQ(counter.total().fps(), 0.0);
    EXPECT_TRUE(counter.count_frame(1.0));
    EXPECT_EQ(counter.interval().frames, 2u);
}

TEST(fps_counter, rejects_invalid_configuration_and_observations_before_mutation) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double interval : {0.0, -1.0, nan, inf})
        EXPECT_THROW((alia::fps_counter(interval, 0.0)), std::invalid_argument);
    EXPECT_THROW((alia::fps_counter(1.0, nan)), std::invalid_argument);

    alia::fps_counter counter(1.0, 0.0);
    counter.count_frame(1.0);
    for (const double now : {0.5, nan, inf})
        EXPECT_THROW(counter.count_frame(now), std::invalid_argument);
    EXPECT_THROW(counter.reset(nan), std::invalid_argument);
    EXPECT_EQ(counter.total().frames, 1u);
    EXPECT_DOUBLE_EQ(counter.total().elapsed, 1.0);
    EXPECT_EQ(counter.interval().frames, 1u);
    EXPECT_TRUE(counter.count_frame(2.0));
    EXPECT_EQ(counter.total().frames, 2u);
}

TEST(sliding_fps_counter, warms_up_then_evicts_old_complete_frame_intervals) {
    alia::sliding_fps_counter counter(1.0, 1.0, 0.0);
    EXPECT_EQ(counter.sliding().frames, 0u);
    EXPECT_DOUBLE_EQ(counter.sliding().fps(), 0.0);
    counter.count_frame(0.25);
    EXPECT_EQ(counter.sliding().frames, 1u);
    EXPECT_DOUBLE_EQ(counter.sliding().elapsed, 0.25);
    counter.count_frame(0.5);
    counter.count_frame(1.0);
    counter.count_frame(1.5);
    counter.count_frame(2.0);
    EXPECT_EQ(counter.sliding().frames, 2u);
    EXPECT_DOUBLE_EQ(counter.sliding().elapsed, 1.0);
    EXPECT_DOUBLE_EQ(counter.sliding().fps(), 2.0);
    EXPECT_EQ(counter.total().frames, 5u);
    EXPECT_DOUBLE_EQ(counter.total().fps(), 2.5);
    EXPECT_EQ(counter.interval().frames, 2u);
}

TEST(sliding_fps_counter, retains_the_boundary_before_the_requested_window) {
    alia::sliding_fps_counter counter(1.0, 1.0, 0.0);
    for (const double now : {0.25, 0.5, 1.0, 1.4, 1.8})
        counter.count_frame(now);
    EXPECT_EQ(counter.sliding().frames, 3u);
    EXPECT_DOUBLE_EQ(counter.sliding().elapsed, 1.3);
    EXPECT_DOUBLE_EQ(counter.sliding().mean_frame_time(), 1.3 / 3.0);

    counter.count_frame(4.0);
    EXPECT_EQ(counter.sliding().frames, 1u);
    EXPECT_DOUBLE_EQ(counter.sliding().elapsed, 2.2);
}

TEST(sliding_fps_counter, base_reference_updates_and_resets_history) {
    alia::sliding_fps_counter sliding(1.0, 0.25, 0.0);
    alia::fps_counter& counter = sliding;
    EXPECT_TRUE(counter.count_frame(0.25));
    EXPECT_EQ(sliding.sliding().frames, 1u);
    counter.reset(10.0);
    EXPECT_EQ(sliding.sliding().frames, 0u);
    EXPECT_EQ(counter.total().frames, 0u);
    EXPECT_TRUE(counter.count_frame(10.5));
    EXPECT_EQ(sliding.sliding().frames, 1u);
    EXPECT_DOUBLE_EQ(sliding.sliding().elapsed, 0.5);
}

TEST(sliding_fps_counter, invalid_input_preserves_history_and_base_reports) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double window : {0.0, -1.0, nan, inf})
        EXPECT_THROW((alia::sliding_fps_counter(window, 1.0, 0.0)), std::invalid_argument);

    alia::sliding_fps_counter sliding(1.0, 1.0, 0.0);
    alia::fps_counter& counter = sliding;
    counter.count_frame(1.0);
    EXPECT_THROW(counter.count_frame(0.5), std::invalid_argument);
    EXPECT_THROW(counter.count_frame(nan), std::invalid_argument);
    EXPECT_THROW(counter.reset(inf), std::invalid_argument);
    EXPECT_EQ(sliding.sliding().frames, 1u);
    EXPECT_DOUBLE_EQ(sliding.sliding().elapsed, 1.0);
    EXPECT_EQ(counter.total().frames, 1u);
    EXPECT_EQ(counter.interval().frames, 1u);
}

} // namespace
