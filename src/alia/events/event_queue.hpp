#ifndef EVENT_QUEUE_CEF61368_F8FF_4779_859A_378D14911DE6
#define EVENT_QUEUE_CEF61368_F8FF_4779_859A_378D14911DE6

#include <deque>
#include <mutex>
#include <optional>
#include "event.hpp"
#include "event_source.hpp"
#include "../util/soo_any.hpp"

namespace alia {

    /**
     * @brief Thread-safe event queue.
     */
    class event_queue {
    public:
        using event_owner = soo_any<64>;

        void push(event_owner ev) {
            std::lock_guard<std::mutex> lock(mutex_);
            events_.push_back(std::move(ev));
        }

        std::optional<event_owner> pop() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (events_.empty())
                return std::nullopt;
            event_owner ev = std::move(events_.front());
            events_.pop_front();
            return ev;
        }

        std::optional<event_owner> poll() { return pop(); }

        bool empty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return events_.empty();
        }

        void listen_to(event_source &source) { source.register_queue(this); }

        void stop_listening_to(event_source &source) { source.unregister_queue(this); }

    private:
        std::deque<event_owner> events_;
        mutable std::mutex mutex_;
    };

} // namespace alia

#endif /* EVENT_QUEUE_CEF61368_F8FF_4779_859A_378D14911DE6 */
