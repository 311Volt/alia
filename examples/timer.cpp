#include "alia/events/event_queue.hpp"
#include "alia/events/timer.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        alia::timer timer(0.25);
        alia::event_queue events;
        events.register_source(&timer.get_event_source());
        timer.start();

        // This example has no window. Window applications must also pump poll()
        // regularly: wait_for_event() itself does not process native messages.
        for (;;) {
            if (!events.wait_for_event(2.0))
                throw std::runtime_error("timed out waiting for a timer tick");
            const auto event = events.pop();
            const auto* tick = event.get_if<alia::timer_event>();
            if (!tick)
                continue;
            std::cout << "tick=" << tick->count << " at " << event.meta.timestamp << " s\n";
            if (tick->count == 4) {
                timer.stop();
                timer.resume(); // Preserve count and the remainder of the interval.
            } else if (tick->count == 8) {
                timer.set_period(0.1);
            } else if (tick->count >= 12) {
                timer.stop();
                break;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "timer example failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
