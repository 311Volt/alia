#ifndef EVENT_QUEUE_2F8D6E3A_4B1C_4D7E_9A5F_6E2B8C1D4A7E
#define EVENT_QUEUE_2F8D6E3A_4B1C_4D7E_9A5F_6E2B8C1D4A7E

#include "event.hpp"
#include "../util/any_deque.hpp"
#include "../util/soo_any.hpp"
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <unordered_set>
#include <memory>

namespace alia {

/* TODO optimize storage!!!
 * currently we waste:
 * - sizeof(void*) for event_slot_base vtable pointer
 * - sizeof(void*) for any_deque vtable pointer 
 * - 4 bytes for event_type_id (could be inside vtable)
 * - 
 */

class event_source;
class event_queue;
struct source_identity; // defined in event_source.hpp

// ── any_event_handle ──────────────────────────────────────────────────
//
// Non-owning view of the front event in an event_queue, returned by peek().
// Valid only while the event remains in the queue (i.e. until the next pop()
// or clear() on the same queue).

struct any_event_handle {
    event_metadata meta;
    void*          event; ///< pointer to the TEvent payload inside the queue

    /// Returns a pointer to the event as TEvent, or nullptr if the type
    /// does not match.
    template<alia::event EventT>
    const EventT* get_if() const noexcept {
        if (meta.event_type_id != get_event_type_id<EventT>()) return nullptr;
        return static_cast<const EventT*>(event);
    }
};

// ── any_event_owner ───────────────────────────────────────────────────
//
// Owning handle to a popped event, returned by pop(). The event payload is
// move-constructed out of the queue into an internal soo_any<80> buffer.

struct any_event_owner {
    event_metadata meta;
    soo_any<80>    event; ///< owns the moved-out TEvent payload

    /// Returns a pointer to the event as TEvent, or nullptr if the type
    /// does not match.
    template<alia::event EventT>
    EventT* get_if() noexcept {
        if (meta.event_type_id != get_event_type_id<EventT>()) return nullptr;
        return static_cast<EventT*>(event.get_ptr_unchecked());
    }

    template<alia::event EventT>
    const EventT* get_if() const noexcept {
        if (meta.event_type_id != get_event_type_id<EventT>()) return nullptr;
        return static_cast<const EventT*>(event.get_ptr_unchecked());
    }
};

// ── Queue Receiver ────────────────────────────────────────────────────

// Stable heap-allocated identity for event_queue. Its address remains valid
// across moves of the owning event_queue, so event sources can hold
// queue_receiver* without being invalidated by a move.
struct queue_receiver {
    event_queue* owner;
    mutable std::mutex mutex;
    std::condition_variable cv;
    any_deque events;
    std::unordered_set<source_identity*> sources;

    explicit queue_receiver(event_queue* owner) : owner(owner) {}

    // Removes this receiver from all registered source identities.
    ~queue_receiver();

    // Called by source_identity::~source_identity() so we drop the dangling ptr.
    void notify_source_end_of_lifetime(source_identity* sid);

    template<alia::event TEvent>
    void push_event(const event_slot<TEvent>& slot_data) {
        std::lock_guard<std::mutex> lock(mutex);
        events.push_back(slot_data);
        cv.notify_one();
    }

    template<alia::event TEvent>
    void push_event(event_slot<TEvent>&& slot_data) {
        std::lock_guard<std::mutex> lock(mutex);
        events.push_back(std::move(slot_data));
        cv.notify_one();
    }
};

// ── Event Queue ───────────────────────────────────────────────────────

// Thread-safe event queue for heterogeneous events.
// Movable but not copyable.
class event_queue {
private:
    std::unique_ptr<queue_receiver> receiver_;

    friend class event_source;
    friend struct source_identity;
    friend struct queue_receiver;

public:
    event_queue() : receiver_(std::make_unique<queue_receiver>(this)) {}

    // queue_receiver destructor notifies all sources automatically.
    ~event_queue() = default;

    event_queue(event_queue&& other) noexcept
        : receiver_(std::move(other.receiver_))
    {
        if (receiver_) receiver_->owner = this;
    }

    event_queue& operator=(event_queue&& other) noexcept {
        if (this != &other) {
            receiver_.reset(); // notifies sources via queue_receiver destructor
            receiver_ = std::move(other.receiver_);
            if (receiver_) receiver_->owner = this;
        }
        return *this;
    }

    event_queue(const event_queue&) = delete;
    event_queue& operator=(const event_queue&) = delete;

    // ── Event Source Registration ─────────────────────────────────────

    void register_source(event_source* source);
    void unregister_source(event_source* source);

    // ── Query ─────────────────────────────────────────────────────────

    [[nodiscard]] bool empty() const noexcept {
        std::lock_guard<std::mutex> lock(receiver_->mutex);
        return receiver_->events.empty();
    }

    [[nodiscard]] std::size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(receiver_->mutex);
        return receiver_->events.size();
    }

    // ── Peek ──────────────────────────────────────────────────────────

    // Returns a non-owning view of the front event without removing it.
    // The returned handle is valid until the next pop() or clear().
    // Returns an empty handle (null event pointer) if the queue is empty.
    [[nodiscard]] any_event_handle peek() const noexcept {
        std::lock_guard<std::mutex> lock(receiver_->mutex);
        if (receiver_->events.empty()) return {};
        auto* slot = static_cast<event_slot_base*>(receiver_->events.front());
        return { slot->meta, slot->event_vt->get_event_ptr(slot) };
    }

    // ── Pop ───────────────────────────────────────────────────────────

    // Removes the front event and returns ownership of its payload.
    // The payload is move-constructed into the returned any_event_owner.
    // Asserts if the queue is empty.
    [[nodiscard]] any_event_owner pop() {
        std::lock_guard<std::mutex> lock(receiver_->mutex);
        auto* slot = static_cast<event_slot_base*>(receiver_->events.front());
        event_metadata meta = slot->meta;
        void* event_ptr = slot->event_vt->get_event_ptr(slot);
        const type_erasure_vtable_t* evt_vt = slot->event_vt->event_type_erasure_vt;
        soo_any<80> owned(move_construct_tag, evt_vt, event_ptr);
        receiver_->events.pop_front();
        return { meta, std::move(owned) };
    }

    // Discards the front event without returning it.
    void discard() {
        std::lock_guard<std::mutex> lock(receiver_->mutex);
        if (!receiver_->events.empty())
            receiver_->events.pop_front();
    }

    // ── Wait for Event ────────────────────────────────────────────────

    // Returns true if an event is available, false if timeout occurred.
    // max_seconds < 0: wait forever; == 0: no wait; > 0: timed wait.
    [[nodiscard]] bool wait_for_event(double max_seconds = -1.0) {
        auto* rec = receiver_.get();
        std::unique_lock<std::mutex> lock(rec->mutex);
        if (!rec->events.empty()) return true;
        if (max_seconds == 0.0) return false;
        if (max_seconds < 0.0) {
            rec->cv.wait(lock, [rec] { return !rec->events.empty(); });
            return true;
        }
        auto duration = std::chrono::duration<double>(max_seconds);
        return rec->cv.wait_for(lock, duration, [rec] { return !rec->events.empty(); });
    }

    // ── Clear ─────────────────────────────────────────────────────────

    void clear() {
        std::lock_guard<std::mutex> lock(receiver_->mutex);
        receiver_->events.clear();
    }
};

} // namespace alia

#endif /* EVENT_QUEUE_2F8D6E3A_4B1C_4D7E_9A5F_6E2B8C1D4A7E */
