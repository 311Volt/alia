#ifndef EVENT_SOURCE_IMPL_3A7F9B2E_5C8D_4E1A_9B6F_2D8A4C1E7B3D
#define EVENT_SOURCE_IMPL_3A7F9B2E_5C8D_4E1A_9B6F_2D8A4C1E7B3D

#include "event_source.hpp"
#include "event_queue.hpp"
#include "../core/get_time.hpp"

namespace alia {

// ── Template Method Implementations ───────────────────────────────────

template<event TEvent>
void event_source::emit(const TEvent& ev) {
    std::lock_guard<std::mutex> lock(identity_->mutex);
    if (identity_->queues.empty()) return;

    event_with_metadata<TEvent> event_data;
    event_data.meta.timestamp = get_time();
    event_data.meta.source = this;
    event_data.ev = ev;

    for (auto* rec : identity_->queues) {
        rec->push_event(event_data);
    }
}

template<event TEvent>
void event_source::emit(TEvent&& ev) {
    std::lock_guard<std::mutex> lock(identity_->mutex);
    if (identity_->queues.empty()) return;

    event_with_metadata<TEvent> event_data;
    event_data.meta.timestamp = get_time();
    event_data.meta.source = this;
    event_data.ev = std::move(ev);

    // For a single queue we can move; otherwise we copy to all but the last.
    if (identity_->queues.size() == 1) {
        (*identity_->queues.begin())->push_event(std::move(event_data));
    } else {
        for (auto* rec : identity_->queues) {
            rec->push_event(event_data);
        }
    }
}

} // namespace alia

#endif /* EVENT_SOURCE_IMPL_3A7F9B2E_5C8D_4E1A_9B6F_2D8A4C1E7B3D */
