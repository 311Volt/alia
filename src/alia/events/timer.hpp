#ifndef ALIA_EVENTS_TIMER_HPP
#define ALIA_EVENTS_TIMER_HPP

#include "event_source.hpp"

#include <cstdint>
#include <memory>

namespace alia {

struct timer_event {
    static constexpr event_type_id_t alia_event_type_id = 0x0300;
    std::int64_t count;
};

// Periodic event source with a shared background scheduler. All durations are
// seconds. Controls/queries are synchronized; synchronize lifetime changes with
// other users. Moving a timer preserves its source address and registrations.
class timer {
public:
    // Initially stopped with count zero. The period must be finite, positive,
    // and representable by steady_clock (at least one clock tick).
    explicit timer(double period_seconds);
    ~timer();

    timer(timer&&) noexcept;
    timer& operator=(timer&&) noexcept;
    timer(const timer&) = delete;
    timer& operator=(const timer&) = delete;

    // Start begins a full interval; resume uses the delay saved by stop. Neither
    // resets count. Both are no-ops while running.
    void start();
    // Settle all ticks due at the cutoff and finish emission before returning.
    // Events already in registered queues are retained.
    void stop();
    void resume();
    [[nodiscard]] bool is_running() const;

    [[nodiscard]] double period() const;
    // Settles due ticks and begins a fresh interval; also replaces a saved delay.
    void set_period(double seconds);

    // Latest serviced tick count, also updated when no queues are registered.
    [[nodiscard]] std::int64_t count() const;
    void set_count(std::int64_t value);

    // Register this source with event_queue. wait_for_event() does not pump
    // native window messages: window applications must still call window::poll().
    event_source& get_event_source();

    // A moved-from timer supports destruction and move assignment only.
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace alia

#endif // ALIA_EVENTS_TIMER_HPP
