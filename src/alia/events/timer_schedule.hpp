#ifndef ALIA_EVENTS_TIMER_SCHEDULE_HPP
#define ALIA_EVENTS_TIMER_SCHEDULE_HPP

// Internal deadline arithmetic, shared by the scheduler and deterministic tests.
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace alia::detail {

class timer_schedule {
public:
    using clock = std::chrono::steady_clock;
    using duration = clock::duration;
    using time_point = clock::time_point;

    explicit timer_schedule(double seconds)
        : period_(checked_period(seconds)), remaining_(period_), seconds_(seconds) {}

    [[nodiscard]] bool is_running() const noexcept { return running_; }
    [[nodiscard]] double period() const noexcept { return seconds_; }
    [[nodiscard]] std::int64_t count() const noexcept { return count_; }
    [[nodiscard]] time_point deadline() const noexcept { return deadline_; }

    void start(time_point now) {
        if (running_)
            return;
        deadline_ = checked_deadline(now, period_);
        running_ = true;
    }

    void resume(time_point now) {
        if (running_)
            return;
        deadline_ = checked_deadline(now, remaining_);
        running_ = true;
    }

    template<class Emit>
    void settle(time_point now, Emit&& emit) {
        while (running_ && deadline_ <= now) {
            const auto next = checked_deadline(deadline_, period_);
            // Define wraparound without signed-integer overflow.
            count_ = count_ == (std::numeric_limits<std::int64_t>::max)()
                ? (std::numeric_limits<std::int64_t>::min)() : count_ + 1;
            deadline_ = next;
            emit(count_);
        }
    }

    template<class Emit>
    void stop(time_point now, Emit&& emit) {
        if (!running_)
            return;
        settle(now, emit);
        remaining_ = deadline_ - now;
        running_ = false;
    }

    template<class Emit>
    void set_period(double seconds, time_point now, Emit&& emit) {
        const auto period = checked_period(seconds);
        // Validate the new deadline before settling or changing existing state.
        const auto deadline = running_ ? checked_deadline(now, period) : time_point{};
        settle(now, emit);
        period_ = remaining_ = period;
        seconds_ = seconds;
        if (running_)
            deadline_ = deadline;
    }

    void set_count(std::int64_t value) noexcept { count_ = value; }

private:
    static duration checked_period(double seconds) {
        if (!std::isfinite(seconds) || seconds <= 0.0)
            throw std::invalid_argument("timer period must be finite and positive");
        using ticks_duration = std::chrono::duration<long double, duration::period>;
        const long double ticks = ticks_duration(std::chrono::duration<long double>(seconds)).count();
        const long double rounded = std::ceil(ticks);
        // The strict upper bound is also safe when long double cannot represent
        // the exact maximum integer. Round up so the effective period is not shorter.
        if (ticks < 1.0L || !std::isfinite(rounded) ||
            rounded >= static_cast<long double>((std::numeric_limits<duration::rep>::max)()))
            throw std::invalid_argument("timer period is not representable by steady_clock");
        return duration(static_cast<duration::rep>(rounded));
    }

    static time_point checked_deadline(time_point now, duration delay) {
        if (now.time_since_epoch().count() >
            (std::numeric_limits<duration::rep>::max)() - delay.count())
            throw std::invalid_argument("timer deadline is not representable by steady_clock");
        return now + delay;
    }

    duration period_;
    duration remaining_;
    double seconds_;
    time_point deadline_{};
    std::int64_t count_ = 0;
    bool running_ = false;
};

} // namespace alia::detail

#endif // ALIA_EVENTS_TIMER_SCHEDULE_HPP
