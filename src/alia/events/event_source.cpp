#include "event_source_impl.hpp"

namespace alia {

// ── queue_receiver ────────────────────────────────────────────────────

queue_receiver::~queue_receiver() {
    // Notify all registered sources that this receiver is going away.
    // We iterate directly — no concurrent modification expected during destruction.
    for (auto* sid : sources) {
        std::lock_guard<std::mutex> lock(sid->mutex);
        sid->queues.erase(this);
    }
}

void queue_receiver::notify_source_end_of_lifetime(source_identity* sid) {
    std::lock_guard<std::mutex> lock(mutex);
    sources.erase(sid);
}

// ── source_identity ───────────────────────────────────────────────────

source_identity::~source_identity() {
    // Notify all registered receivers that this source is going away.
    for (auto* rec : queues) {
        std::lock_guard<std::mutex> lock(rec->mutex);
        rec->sources.erase(this);
    }
}

void source_identity::notify_queue_end_of_lifetime(queue_receiver* rec) {
    std::lock_guard<std::mutex> lock(mutex);
    queues.erase(rec);
}

// ── event_queue ───────────────────────────────────────────────────────

void event_queue::register_source(event_source* source) {
    auto* rec = receiver_.get();
    auto* sid = source->identity_.get();
    {
        std::lock_guard<std::mutex> lock(sid->mutex);
        sid->queues.insert(rec);
    }
    {
        std::lock_guard<std::mutex> lock(rec->mutex);
        rec->sources.insert(sid);
    }
}

void event_queue::unregister_source(event_source* source) {
    auto* rec = receiver_.get();
    auto* sid = source->identity_.get();
    {
        std::lock_guard<std::mutex> lock(sid->mutex);
        sid->queues.erase(rec);
    }
    {
        std::lock_guard<std::mutex> lock(rec->mutex);
        rec->sources.erase(sid);
    }
}

} // namespace alia
