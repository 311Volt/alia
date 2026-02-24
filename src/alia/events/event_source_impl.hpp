#ifndef EVENT_SOURCE_IMPL_D5C7A87A_DEB4_4438_B9DE_C76EEF7532C2
#define EVENT_SOURCE_IMPL_D5C7A87A_DEB4_4438_B9DE_C76EEF7532C2

#include "event_source.hpp"
#include "event_queue.hpp"
#include "../core/get_time.hpp"

namespace alia {

// ── Template Method Implementations ───────────────────────────────────

template<event TEvent>
void event_source::emit(const TEvent& ev) {
    std::lock_guard<std::mutex> lock(identity_->mutex);
    if (identity_->queues.empty()) return;

    event_slot<TEvent> slot;
    slot.meta.timestamp     = get_time();
    slot.meta.source        = this;
    slot.meta.event_type_id = get_event_type_id<TEvent>();
    slot.event_vt           = event_slot<TEvent>::get_event_vtable();
    slot.ev                 = ev;

    for (auto* rec : identity_->queues) {
        rec->push_event(slot);
    }
}

template<event TEvent>
void event_source::emit(TEvent&& ev) {
    std::lock_guard<std::mutex> lock(identity_->mutex);
    if (identity_->queues.empty()) return;

    event_slot<TEvent> slot;
    slot.meta.timestamp     = get_time();
    slot.meta.source        = this;
    slot.meta.event_type_id = get_event_type_id<TEvent>();
    slot.event_vt           = event_slot<TEvent>::get_event_vtable();
    slot.ev                 = std::move(ev);

    // For a single queue we can move; otherwise we copy to all but the last.
    if (identity_->queues.size() == 1) {
        (*identity_->queues.begin())->push_event(std::move(slot));
    } else {
        for (auto* rec : identity_->queues) {
            rec->push_event(slot);
        }
    }
}

} // namespace alia

#endif /* EVENT_SOURCE_IMPL_D5C7A87A_DEB4_4438_B9DE_C76EEF7532C2 */
