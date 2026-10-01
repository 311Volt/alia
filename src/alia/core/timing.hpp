#ifndef ALIA_CORE_TIMING_HPP
#define ALIA_CORE_TIMING_HPP

#include "get_time.hpp"

#include <cmath>
#include <cstdint>
#include <deque>
#include <stdexcept>

namespace alia {

namespace detail {
inline void require_timing_timestamp(double now) {
    if (!std::isfinite(now))
        throw std::invalid_argument("timing timestamp must be finite");
}

inline void require_timing_interval(double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0)
        throw std::invalid_argument("timing interval must be finite and positive");
}

inline void require_timing_observation(double now, double last, double start) {
    require_timing_timestamp(now);
    if (now < last || !std::isfinite(now - start))
        throw std::invalid_argument("timing timestamps must be nondecreasing with a finite span");
}
} // namespace detail

// All durations are in seconds. A tick counts a call, independently of rendering.
struct frame_timing {
    std::uint64_t tick = 0;
    double delta = 0.0;
    double elapsed = 0.0;
};

class frame_clock {
public:
    explicit frame_clock(double now = get_time()) { reset(now); }

    frame_timing tick(double now = get_time()) {
        detail::require_timing_observation(now, last_, start_);
        const double delta = now - last_;
        last_ = now;
        return {++ticks_, delta, now - start_};
    }

    // Reset may establish an earlier baseline, for example for a replay.
    void reset(double now = get_time()) {
        detail::require_timing_timestamp(now);
        start_ = last_ = now;
        ticks_ = 0;
    }

private:
    double start_ = 0.0;
    double last_ = 0.0;
    std::uint64_t ticks_ = 0;
};

struct frame_statistics {
    std::uint64_t frames = 0;
    double elapsed = 0.0;

    [[nodiscard]] double fps() const noexcept {
        return frames == 0 || elapsed <= 0.0
            ? 0.0 : static_cast<double>(frames) / elapsed;
    }

    [[nodiscard]] double mean_frame_time() const noexcept {
        return frames == 0 || elapsed <= 0.0
            ? 0.0 : elapsed / static_cast<double>(frames);
    }
};

// Fixed-size, allocation-free storage. Observations advance only on count_frame().
// Construct/reset immediately before measurement, and count after presentation.
// Instances require external synchronization when accessed from multiple threads.
class fps_counter {
public:
    explicit fps_counter(double interval_seconds = 1.0, double now = get_time())
        : interval_seconds_(interval_seconds) {
        detail::require_timing_interval(interval_seconds);
        reset(now);
    }

    virtual ~fps_counter() = default;

    // Returns true when a new completed interval report is available.
    virtual bool count_frame(double now = get_time()) {
        validate_observation(now);
        last_ = now;
        ++frames_;
        ++interval_frames_;
        const double elapsed = now - interval_start_;
        if (elapsed < interval_seconds_)
            return false;

        interval_ = {interval_frames_, elapsed};
        interval_start_ = now;
        interval_frames_ = 0;
        return true;
    }

    virtual void reset(double now = get_time()) {
        detail::require_timing_timestamp(now);
        start_ = last_ = interval_start_ = now;
        frames_ = interval_frames_ = 0;
        interval_ = {};
    }

    // The last completed report, initially empty; reads do not sample the clock.
    [[nodiscard]] frame_statistics interval() const noexcept { return interval_; }

    [[nodiscard]] frame_statistics total() const noexcept {
        return {frames_, last_ - start_};
    }

protected:
    void validate_observation(double now) const {
        detail::require_timing_observation(now, last_, start_);
    }

private:
    double interval_seconds_;
    double start_ = 0.0;
    double last_ = 0.0;
    double interval_start_ = 0.0;
    std::uint64_t frames_ = 0;
    std::uint64_t interval_frames_ = 0;
    frame_statistics interval_;
};

// Adds history proportional to the frames in the requested window. Reports use
// whole frame intervals, retaining the boundary before the requested window.
class sliding_fps_counter : public fps_counter {
public:
    explicit sliding_fps_counter(
        double window_seconds = 1.0,
        double interval_seconds = 1.0,
        double now = get_time())
        : fps_counter(interval_seconds, now), window_seconds_(window_seconds) {
        detail::require_timing_interval(window_seconds);
        timestamps_.push_back(now);
    }

    bool count_frame(double now = get_time()) override {
        validate_observation(now);
        // Allocate before committing either counter's state.
        timestamps_.push_back(now);
        const bool published = fps_counter::count_frame(now);
        while (timestamps_.size() > 2 && now - timestamps_[1] >= window_seconds_)
            timestamps_.pop_front();
        return published;
    }

    void reset(double now = get_time()) override {
        detail::require_timing_timestamp(now);
        std::deque<double> baseline{now};
        fps_counter::reset(now);
        timestamps_.swap(baseline);
    }

    [[nodiscard]] frame_statistics sliding() const noexcept {
        return {
            static_cast<std::uint64_t>(timestamps_.size() - 1),
            timestamps_.back() - timestamps_.front(),
        };
    }

private:
    double window_seconds_;
    std::deque<double> timestamps_;
};

} // namespace alia

#endif // ALIA_CORE_TIMING_HPP
