#ifndef EVENT_SOURCE_B2A26DD0_0B3B_445F_A32F_38A6B241B375
#define EVENT_SOURCE_B2A26DD0_0B3B_445F_A32F_38A6B241B375

#include <vector>
#include <algorithm>
#include <mutex>
#include "event.hpp"

namespace alia {

class event_queue;
class event_bus;

class event_source {
public:
    event_source() = default;
    ~event_source();

    event_source(const event_source&) = delete;
    event_source& operator=(const event_source&) = delete;

    void register_queue(event_queue* queue);
    void unregister_queue(event_queue* queue);

    void register_bus(event_bus* bus);
    void unregister_bus(event_bus* bus);

    template <event T>
    void emit(T ev) {
        ev.source = this;
        // Logic for emitting to registered queues and busses will be added
        // once those classes are defined.
        emit_impl(&ev, sizeof(T), T::type_id);
    }

private:
    void emit_impl(const base_event* ev, std::size_t size, event_type_id type);

    std::mutex mutex_;
    std::vector<event_queue*> queues_;
    std::vector<event_bus*> busses_;
};

} // namespace alia

#endif /* EVENT_SOURCE_B2A26DD0_0B3B_445F_A32F_38A6B241B375 */
