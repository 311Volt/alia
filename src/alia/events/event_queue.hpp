#ifndef EVENT_QUEUE_2F8D6E3A_4B1C_4D7E_9A5F_6E2B8C1D4A7E
#define EVENT_QUEUE_2F8D6E3A_4B1C_4D7E_9A5F_6E2B8C1D4A7E

#include "event.hpp"
#include "../util/any_deque.hpp"
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <unordered_set>
#include <memory>

namespace alia {

class event_source;
class event_queue;
struct source_identity; // defined in event_source.hpp

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

    template<event TEvent>
    void push_event(const event_with_metadata<TEvent>& event_data) {
        std::lock_guard<std::mutex> lock(mutex);
        events.push_back(event_data);
        cv.notify_one();
    }

    template<event TEvent>
    void push_event(event_with_metadata<TEvent>&& event_data) {
        std::lock_guard<std::mutex> lock(mutex);
        events.push_back(std::move(event_data));
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

    // ── Type-erased helpers (called with receiver_->mutex already held) ──

    [[nodiscard]] const void* peek_impl() const noexcept {
        if (receiver_->events.empty()) return nullptr;
        const void* payload = *receiver_->events.begin();
        return static_cast<const char*>(payload) + sizeof(event_metadata);
    }

    [[nodiscard]] const void* peek_with_metadata_impl() const noexcept {
        if (receiver_->events.empty()) return nullptr;
        return *receiver_->events.begin();
    }

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

    // ── Peek (without metadata) ───────────────────────────────────────

    template<event TEvent>
    [[nodiscard]] const TEvent* peek() const noexcept {
        std::lock_guard<std::mutex> lock(receiver_->mutex);
        if (peek_impl() == nullptr) return nullptr;
        auto it = receiver_->events.begin();
        if (it.vtable() != any_deque::vtable_for<event_with_metadata<TEvent>>())
            return nullptr;
        return &static_cast<const event_with_metadata<TEvent>*>(*it)->ev;
    }

    // ── Peek with metadata ────────────────────────────────────────────

    template<event TEvent>
    [[nodiscard]] const event_with_metadata<TEvent>* peek_with_metadata() const noexcept {
        std::lock_guard<std::mutex> lock(receiver_->mutex);
        if (peek_with_metadata_impl() == nullptr) return nullptr;
        auto it = receiver_->events.begin();
        if (it.vtable() != any_deque::vtable_for<event_with_metadata<TEvent>>())
            return nullptr;
        return static_cast<const event_with_metadata<TEvent>*>(*it);
    }

    // ── Pop (without metadata) ────────────────────────────────────────

    template<event TEvent>
    [[nodiscard]] std::optional<TEvent> pop() {
        std::lock_guard<std::mutex> lock(receiver_->mutex);
        if (receiver_->events.empty()) return std::nullopt;
        auto it = receiver_->events.begin();
        if (it.vtable() != any_deque::vtable_for<event_with_metadata<TEvent>>())
            return std::nullopt;
        auto* event_data = static_cast<event_with_metadata<TEvent>*>(*it);
        TEvent result = std::move(event_data->ev);
        receiver_->events.pop_front();
        return result;
    }

    // Pop front event without type checking (discards it)
    void pop() {
        std::lock_guard<std::mutex> lock(receiver_->mutex);
        if (!receiver_->events.empty())
            receiver_->events.pop_front();
    }

    // ── Pop with metadata ─────────────────────────────────────────────

    template<event TEvent>
    [[nodiscard]] std::optional<event_with_metadata<TEvent>> pop_with_metadata() {
        std::lock_guard<std::mutex> lock(receiver_->mutex);
        if (receiver_->events.empty()) return std::nullopt;
        auto it = receiver_->events.begin();
        if (it.vtable() != any_deque::vtable_for<event_with_metadata<TEvent>>())
            return std::nullopt;
        auto* event_data = static_cast<event_with_metadata<TEvent>*>(*it);
        event_with_metadata<TEvent> result = std::move(*event_data);
        receiver_->events.pop_front();
        return result;
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
