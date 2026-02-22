#ifndef EVENT_SOURCE_8E3C4A21_9F76_4B2D_A8E2_1D5F7C8A9E3B
#define EVENT_SOURCE_8E3C4A21_9F76_4B2D_A8E2_1D5F7C8A9E3B

#include "event.hpp"
#include <unordered_set>
#include <mutex>
#include <memory>

namespace alia {

class event_queue;
class event_source;
struct queue_receiver; // defined in event_queue.hpp

// ── Source Identity ───────────────────────────────────────────────────

// Stable heap-allocated identity for event_source. Its address remains valid
// across moves of the owning event_source, so event queues can hold
// source_identity* without being invalidated by a move.
struct source_identity {
    event_source* owner;
    mutable std::mutex mutex;
    std::unordered_set<queue_receiver*> queues;

    explicit source_identity(event_source* owner) : owner(owner) {}

    // Removes this identity from all registered queue receivers.
    ~source_identity();

    // Called by queue_receiver::~queue_receiver() so we drop the dangling ptr.
    void notify_queue_end_of_lifetime(queue_receiver* rec);
};

// ── Event Source ──────────────────────────────────────────────────────

// Base class for all event sources (displays, keyboard, mouse, etc.).
// Event sources fire events from potentially different threads.
// Movable but not copyable.
class event_source {
private:
    std::unique_ptr<source_identity> identity_;

    friend class event_queue;
    friend struct queue_receiver;
    friend struct source_identity;

public:
    event_source() : identity_(std::make_unique<source_identity>(this)) {}

    // source_identity destructor notifies all queues automatically.
    virtual ~event_source() = default;

    event_source(event_source&& other) noexcept
        : identity_(std::move(other.identity_))
    {
        if (identity_) identity_->owner = this;
    }

    event_source& operator=(event_source&& other) noexcept {
        if (this != &other) {
            identity_.reset(); // notifies queues via source_identity destructor
            identity_ = std::move(other.identity_);
            if (identity_) identity_->owner = this;
        }
        return *this;
    }

    event_source(const event_source&) = delete;
    event_source& operator=(const event_source&) = delete;

    // ── Emit Event ────────────────────────────────────────────────────

    // Emit an event to all registered queues (copy version)
    template<event TEvent>
    void emit(const TEvent& ev);

    // Emit an event to all registered queues (move version)
    template<event TEvent>
    void emit(TEvent&& ev);
};

} // namespace alia

#endif /* EVENT_SOURCE_8E3C4A21_9F76_4B2D_A8E2_1D5F7C8A9E3B */
