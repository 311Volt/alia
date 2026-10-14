#ifndef ALIA_AUDIO_RING_BUFFER_HPP
#define ALIA_AUDIO_RING_BUFFER_HPP

#include "sample.hpp"
#include <algorithm>
#include <compare>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace alia {
    /// Move-only, fixed-capacity FIFO, without synchronization. Only construction and
    /// reserve() allocate. Spans/iterators are invalidated by reserve or moves;
    /// queue mutation invalidates logical iterators. External spans must not
    /// alias this buffer when passed to push/pop.
    template <sample_frame F> requires (!std::is_const_v<F>)
    class ring_buffer {
    public:
        explicit ring_buffer(std::size_t capacity) : storage_(capacity) {}
        ring_buffer(const ring_buffer &) = delete;
        ring_buffer &operator=(const ring_buffer &) = delete;
        ring_buffer(ring_buffer &&other) noexcept
            : storage_(std::move(other.storage_)), head_(std::exchange(other.head_, 0)), size_(std::exchange(other.size_, 0)) {}
        ring_buffer &operator=(ring_buffer &&other) noexcept {
            if (this != &other) {
                storage_ = std::move(other.storage_);
                head_ = std::exchange(other.head_, 0);
                size_ = std::exchange(other.size_, 0);
            }
            return *this;
        }
        void reserve(std::size_t capacity) {
            if (capacity <= this->capacity()) return;
            std::vector<F> replacement(capacity);
            for (std::size_t i = 0; i < size_; ++i) replacement[i] = (*this)[i];
            storage_.swap(replacement);
            head_ = 0;
        }
        [[nodiscard]] std::size_t size() const noexcept { return size_; }
        [[nodiscard]] std::size_t capacity() const noexcept { return storage_.size(); }
        [[nodiscard]] std::size_t free_space() const noexcept { return capacity() - size_; }
        [[nodiscard]] bool empty() const noexcept { return !size_; }
        [[nodiscard]] bool full() const noexcept { return size_ == capacity(); }
        void clear() noexcept { head_ = size_ = 0; }
        /// Appends as many frames as fit; returns the accepted frame count.
        std::size_t push(std::span<const F> input) noexcept {
            const auto count = (std::min)(input.size(), free_space());
            auto spans = writable_spans();
            const auto first = (std::min)(count, spans[0].size());
            std::copy_n(input.begin(), first, spans[0].begin());
            std::copy_n(input.begin() + first, count - first, spans[1].begin());
            commit_push(count);
            return count;
        }
        /// Removes as many frames as available; returns the delivered frame count.
        std::size_t pop(std::span<F> output) noexcept {
            const auto count = (std::min)(output.size(), size_);
            auto spans = readable_spans();
            const auto first = (std::min)(count, spans[0].size());
            std::copy_n(spans[0].begin(), first, output.begin());
            std::copy_n(spans[1].begin(), count - first, output.begin() + first);
            discard(count);
            return count;
        }
        /// All-or-nothing; false leaves both the buffer and span unchanged.
        bool try_push(std::span<const F> input) noexcept {
            if (input.size() > free_space()) return false;
            push(input);
            return true;
        }
        /// All-or-nothing; false leaves both the buffer and span unchanged.
        bool try_pop(std::span<F> output) noexcept {
            if (output.size() > size_) return false;
            pop(output);
            return true;
        }
        [[nodiscard]] auto readable_spans(this auto &&self) noexcept {
            using E = std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const F, F>;
            auto storage = std::span<E>(self.storage_);
            const auto first = (std::min)(self.size_, self.capacity() - self.head_);
            return std::array<std::span<E>, 2>{storage.subspan(self.head_, first), storage.first(self.size_ - first)};
        }
        [[nodiscard]] std::array<std::span<F>, 2> writable_spans() noexcept {
            if (!capacity()) return {};
            const auto tail = (head_ + size_) % capacity();
            const auto first = (std::min)(free_space(), capacity() - tail);
            auto storage = std::span<F>(storage_);
            return {storage.subspan(tail, first), storage.first(free_space() - first)};
        }
        /// Commit only frames actually written through writable_spans().
        void commit_push(std::size_t count) {
            if (count > free_space()) throw std::out_of_range("ring_buffer::commit_push");
            size_ += count;
        }
        void discard(std::size_t count) {
            if (count > size_) throw std::out_of_range("ring_buffer::discard");
            if (capacity()) head_ = (head_ + count) % capacity();
            size_ -= count;
        }
        auto &&operator[](this auto &&self, std::size_t i) noexcept { return self.storage_[(self.head_ + i) % self.capacity()]; }
        auto &&at(this auto &&self, std::size_t i) {
            if (i >= self.size_) throw std::out_of_range("ring_buffer::at");
            return self[i];
        }
        auto &&front(this auto &&self) noexcept { return self[0]; }
        auto &&back(this auto &&self) noexcept { return self[self.size_ - 1]; }

        template <bool Const> class iterator_impl {
            using owner_type = std::conditional_t<Const, const ring_buffer, ring_buffer>;
            owner_type *owner_ = nullptr;
            std::ptrdiff_t index_ = 0;
            template <bool> friend class iterator_impl;
        public:
            using value_type = F;
            using difference_type = std::ptrdiff_t;
            using reference = std::conditional_t<Const, const F &, F &>;
            using pointer = std::conditional_t<Const, const F *, F *>;
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::random_access_iterator_tag;
            iterator_impl() = default;
            iterator_impl(owner_type *owner, difference_type index) : owner_(owner), index_(index) {}
            template <bool C> requires (Const && !C)
            iterator_impl(const iterator_impl<C> &other) : owner_(other.owner_), index_(other.index_) {}
            reference operator*() const { return (*owner_)[static_cast<std::size_t>(index_)]; }
            pointer operator->() const { return std::addressof(**this); }
            reference operator[](difference_type n) const { return *(*this + n); }
            iterator_impl &operator++() { ++index_; return *this; }
            iterator_impl operator++(int) { auto copy = *this; ++*this; return copy; }
            iterator_impl &operator--() { --index_; return *this; }
            iterator_impl operator--(int) { auto copy = *this; --*this; return copy; }
            iterator_impl &operator+=(difference_type n) { index_ += n; return *this; }
            iterator_impl &operator-=(difference_type n) { index_ -= n; return *this; }
            friend iterator_impl operator+(iterator_impl it, difference_type n) { return it += n; }
            friend iterator_impl operator+(difference_type n, iterator_impl it) { return it += n; }
            friend iterator_impl operator-(iterator_impl it, difference_type n) { return it -= n; }
            friend difference_type operator-(iterator_impl a, iterator_impl b) { return a.index_ - b.index_; }
            bool operator==(const iterator_impl &) const = default;
            auto operator<=>(const iterator_impl &other) const { return index_ <=> other.index_; }
        };
        using iterator = iterator_impl<false>;
        using const_iterator = iterator_impl<true>;
        iterator begin() noexcept { return {this, 0}; }
        iterator end() noexcept { return {this, static_cast<std::ptrdiff_t>(size_)}; }
        const_iterator begin() const noexcept { return {this, 0}; }
        const_iterator end() const noexcept { return {this, static_cast<std::ptrdiff_t>(size_)}; }
        const_iterator cbegin() const noexcept { return begin(); }
        const_iterator cend() const noexcept { return end(); }
    private:
        std::vector<F> storage_;
        std::size_t head_ = 0, size_ = 0;
    };
    static_assert(std::ranges::random_access_range<ring_buffer<float>>);
    static_assert(std::ranges::random_access_range<const ring_buffer<float>>);
}
#endif
