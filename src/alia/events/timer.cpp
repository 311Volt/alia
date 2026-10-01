#include "timer.hpp"
#include "timer_schedule.hpp"
#include "event_source_impl.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace alia {
namespace {

using timer_clock = detail::timer_schedule::clock;

struct timer_state {
    event_source source;
    detail::timer_schedule schedule;

    explicit timer_state(double seconds) : schedule(seconds) {}

    void settle(timer_clock::time_point now) {
        schedule.settle(now, [this](std::int64_t count) { source.emit(timer_event{count}); });
    }
};

class timer_scheduler {
public:
    timer_scheduler() : worker_([this](std::stop_token stop) { run(stop); }) {}

    ~timer_scheduler() {
        // Locking prevents a notification from racing the worker's entry into wait.
        {
            std::lock_guard lock(mutex);
            worker_.request_stop();
        }
        changed.notify_all();
        worker_.join();
    }

    void add(timer_state* state) {
        std::lock_guard lock(mutex);
        timers_.push_back(state);
        changed.notify_all();
    }

    void remove(timer_state* state) noexcept {
        // Also waits for any in-flight emission before the source is destroyed.
        std::lock_guard lock(mutex);
        std::erase(timers_, state);
        changed.notify_all();
    }

    std::mutex mutex;
    std::condition_variable changed;

private:
    void run(std::stop_token stop) {
        std::unique_lock lock(mutex);
        while (!stop.stop_requested()) {
            timer_state* next = nullptr;
            for (auto* state : timers_) {
                if (state->schedule.is_running() &&
                    (!next || state->schedule.deadline() < next->schedule.deadline()))
                    next = state;
            }

            if (!next) {
                changed.wait(lock);
                continue;
            }

            const auto now = timer_clock::now();
            if (now < next->schedule.deadline()) {
                changed.wait_until(lock, next->schedule.deadline());
                continue;
            }

            next->settle(now);
            // Let controls acquire the mutex even when overdue work remains.
            lock.unlock();
            std::this_thread::yield();
            lock.lock();
        }
    }

    std::vector<timer_state*> timers_;
    std::jthread worker_;
};

std::shared_ptr<timer_scheduler> acquire_scheduler() {
    static std::mutex mutex;
    static std::weak_ptr<timer_scheduler> shared;
    std::lock_guard lock(mutex);
    auto scheduler = shared.lock();
    if (!scheduler) {
        scheduler = std::make_shared<timer_scheduler>();
        shared = scheduler;
    }
    return scheduler;
}

} // namespace

struct timer::impl {
    timer_state state;
    std::shared_ptr<timer_scheduler> scheduler;

    explicit impl(double seconds) : state(seconds), scheduler(acquire_scheduler()) {
        scheduler->add(&state);
    }

    ~impl() { scheduler->remove(&state); }
};

timer::timer(double period_seconds) : impl_(std::make_unique<impl>(period_seconds)) {}
timer::~timer() = default;
timer::timer(timer&&) noexcept = default;
timer& timer::operator=(timer&&) noexcept = default;

void timer::start() {
    std::lock_guard lock(impl_->scheduler->mutex);
    impl_->state.schedule.start(timer_clock::now());
    impl_->scheduler->changed.notify_all();
}

void timer::stop() {
    std::lock_guard lock(impl_->scheduler->mutex);
    impl_->state.schedule.stop(timer_clock::now(), [this](std::int64_t count) {
        impl_->state.source.emit(timer_event{count});
    });
    impl_->scheduler->changed.notify_all();
}

void timer::resume() {
    std::lock_guard lock(impl_->scheduler->mutex);
    impl_->state.schedule.resume(timer_clock::now());
    impl_->scheduler->changed.notify_all();
}

bool timer::is_running() const {
    std::lock_guard lock(impl_->scheduler->mutex);
    return impl_->state.schedule.is_running();
}

double timer::period() const {
    std::lock_guard lock(impl_->scheduler->mutex);
    return impl_->state.schedule.period();
}

void timer::set_period(double seconds) {
    std::lock_guard lock(impl_->scheduler->mutex);
    impl_->state.schedule.set_period(seconds, timer_clock::now(), [this](std::int64_t count) {
        impl_->state.source.emit(timer_event{count});
    });
    impl_->scheduler->changed.notify_all();
}

std::int64_t timer::count() const {
    std::lock_guard lock(impl_->scheduler->mutex);
    return impl_->state.schedule.count();
}

void timer::set_count(std::int64_t value) {
    std::lock_guard lock(impl_->scheduler->mutex);
    impl_->state.schedule.set_count(value);
    impl_->scheduler->changed.notify_all();
}

event_source& timer::get_event_source() { return impl_->state.source; }

} // namespace alia
