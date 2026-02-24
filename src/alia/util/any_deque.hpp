#pragma once

// any_deque – heterogeneous, bidirectional deque (C++23)
//
// STORAGE DESIGN
// ══════════════
// Underlying buffer: std::deque<Block> where Block contains a std::vector<std::byte>
// This guarantees that elements are contiguous in memory and their pointers are stable,
// even when the deque resizes or allocates new blocks.
//
// Record layout:
// Elements are bump-allocated within the blocks, tightly packed according to their
// alignment requirements. There is no header stored in the block itself.
//
// A parallel std::deque<slot_t> index_ tracks the payload pointer and vtable for each
// element, giving O(1) navigation and bidirectional iteration.
//
// Overhead per element: 16 bytes (for the index slot) + alignment padding.
// This is highly space-efficient for small events (8-24 bytes).
//
// push_back / push_front / pop_back / pop_front : O(1) amortized
// insert / erase (mid)                          : O(n)
// copy / move construction                      : O(total bytes)

#include "type_erasure.hpp"
#include <deque>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <limits>
#include <utility>
#include <type_traits>
#include <iterator>
#include <cassert>
#include <new>
#include <algorithm>

class any_deque {
public:
    // ── vtable ────────────────────────────────────────────────────────
    using vtable_t = type_erasure_vtable_t;

    template<typename T>
    static const vtable_t* vtable_for() noexcept {
        return type_erasure_vtable_for<T>();
    }

private:
    // ── Storage units ─────────────────────────────────────────────────
    static constexpr std::size_t DEFAULT_BLOCK_SIZE = 4096;

    struct Block {
        std::vector<std::byte> data;
        std::size_t head;
        std::size_t tail;
        Block(std::size_t cap, std::size_t h, std::size_t t) : data(cap), head(h), tail(t) {}
    };

    struct slot_t {
        void* payload;
        const vtable_t* vt;
    };

    std::deque<Block>  blocks_;
    std::deque<slot_t> index_;

    static std::size_t align_forward(std::size_t offset, std::size_t align, const void* base) {
        std::uintptr_t ptr = reinterpret_cast<std::uintptr_t>(base) + offset;
        std::size_t padding = (align - (ptr % align)) % align;
        return offset + padding;
    }

    static std::size_t align_backward(std::size_t offset, std::size_t size, std::size_t align, const void* base) {
        if (offset < size) return static_cast<std::size_t>(-1);
        std::size_t start_offset = offset - size;
        std::uintptr_t ptr = reinterpret_cast<std::uintptr_t>(base) + start_offset;
        std::size_t padding = ptr % align;
        if (start_offset < padding) return static_cast<std::size_t>(-1);
        return start_offset - padding;
    }

    void* allocate_back(std::size_t size, std::size_t align) {
        if (blocks_.empty()) {
            std::size_t cap = std::max(DEFAULT_BLOCK_SIZE, size + align);
            blocks_.emplace_back(cap, cap / 2, cap / 2);
        }
        Block& b = blocks_.back();
        std::size_t aligned_tail = align_forward(b.tail, align, b.data.data());
        
        if (aligned_tail + size <= b.data.size()) {
            b.tail = aligned_tail + size;
            return b.data.data() + aligned_tail;
        } else {
            std::size_t cap = std::max(DEFAULT_BLOCK_SIZE, size + align);
            blocks_.emplace_back(cap, 0, 0);
            Block& nb = blocks_.back();
            std::size_t nb_aligned_tail = align_forward(0, align, nb.data.data());
            nb.tail = nb_aligned_tail + size;
            return nb.data.data() + nb_aligned_tail;
        }
    }

    void* allocate_front(std::size_t size, std::size_t align) {
        if (blocks_.empty()) {
            std::size_t cap = std::max(DEFAULT_BLOCK_SIZE, size + align);
            blocks_.emplace_back(cap, cap / 2, cap / 2);
        }
        Block& b = blocks_.front();
        
        if (b.head >= size) {
            std::size_t new_head = align_backward(b.head, size, align, b.data.data());
            if (new_head != static_cast<std::size_t>(-1) && new_head <= b.head) {
                b.head = new_head;
                return b.data.data() + new_head;
            }
        }
        
        std::size_t cap = std::max(DEFAULT_BLOCK_SIZE, size + align);
        blocks_.emplace_front(cap, cap, cap);
        Block& nb = blocks_.front();
        std::size_t nb_head = align_backward(nb.head, size, align, nb.data.data());
        nb.head = nb_head;
        return nb.data.data() + nb_head;
    }

    void pop_back_raw() {
        index_.pop_back();
        if (!index_.empty()) {
            while (blocks_.size() > 1) {
                Block& last_block = blocks_.back();
                void* last_payload = index_.back().payload;
                std::byte* block_start = last_block.data.data();
                std::byte* block_end = block_start + last_block.data.size();
                if (last_payload >= block_start && last_payload < block_end) {
                    break;
                }
                blocks_.pop_back();
            }
        } else {
            blocks_.clear();
        }
    }

    void pop_front_raw() {
        index_.pop_front();
        if (!index_.empty()) {
            while (blocks_.size() > 1) {
                Block& first_block = blocks_.front();
                void* first_payload = index_.front().payload;
                std::byte* block_start = first_block.data.data();
                std::byte* block_end = block_start + first_block.data.size();
                if (first_payload >= block_start && first_payload < block_end) {
                    break;
                }
                blocks_.pop_front();
            }
        } else {
            blocks_.clear();
        }
    }

    void splice_tail_out(std::size_t from_slot, any_deque& tail) {
        for (std::size_t i = from_slot; i < index_.size(); ++i) {
            slot_t s = index_[i];
            void* d = tail.allocate_back(s.vt->size, s.vt->align);
            s.vt->move_construct(d, s.payload);
            s.vt->destroy(s.payload);
            tail.index_.push_back({d, s.vt});
        }
        while (index_.size() > from_slot) {
            pop_back_raw();
        }
    }

    void splice_tail_in(any_deque& tail) {
        for (std::size_t i = 0; i < tail.index_.size(); ++i) {
            slot_t s = tail.index_[i];
            void* d = allocate_back(s.vt->size, s.vt->align);
            s.vt->move_construct(d, s.payload);
            index_.push_back({d, s.vt});
        }
        tail.clear();
    }

public:
    // ── Iterators ─────────────────────────────────────────────────────
    class const_iterator;

    class iterator {
        friend class any_deque;
        friend class const_iterator;
        any_deque*  owner_;
        std::size_t idx_;
        iterator(any_deque* o, std::size_t i) noexcept : owner_(o), idx_(i) {}
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = void*;
        using difference_type   = std::ptrdiff_t;
        using pointer           = void**;
        using reference         = void*;

        iterator() noexcept : owner_(nullptr), idx_(0) {}

        bool operator==(const iterator& o) const noexcept {
            return owner_ == o.owner_ && idx_ == o.idx_;
        }
        bool operator!=(const iterator& o) const noexcept { return !(*this == o); }
        iterator& operator++()    noexcept { ++idx_; return *this; }
        iterator  operator++(int) noexcept { auto t=*this; ++*this; return t; }
        iterator& operator--()    noexcept { --idx_; return *this; }
        iterator  operator--(int) noexcept { auto t=*this; --*this; return t; }

        void* operator*() const noexcept { return owner_->index_[idx_].payload; }
        void* get()       const noexcept { return owner_->index_[idx_].payload; }

        template<typename T> T& as() const {
            return *static_cast<T*>(owner_->index_[idx_].payload);
        }
        const vtable_t* vtable() const noexcept {
            return owner_->index_[idx_].vt;
        }
    };

    class const_iterator {
        friend class any_deque;
        const any_deque* owner_;
        std::size_t      idx_;
        const_iterator(const any_deque* o, std::size_t i) noexcept
            : owner_(o), idx_(i) {}
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = const void*;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const void**;
        using reference         = const void*;

        const_iterator() noexcept : owner_(nullptr), idx_(0) {}
        /* implicit */ const_iterator(iterator it) noexcept
            : owner_(it.owner_), idx_(it.idx_) {}

        bool operator==(const const_iterator& o) const noexcept {
            return owner_ == o.owner_ && idx_ == o.idx_;
        }
        bool operator!=(const const_iterator& o) const noexcept { return !(*this == o); }
        const_iterator& operator++()    noexcept { ++idx_; return *this; }
        const_iterator  operator++(int) noexcept { auto t=*this; ++*this; return t; }
        const_iterator& operator--()    noexcept { --idx_; return *this; }
        const_iterator  operator--(int) noexcept { auto t=*this; --*this; return t; }

        const void* operator*() const noexcept { return owner_->index_[idx_].payload; }
        const void* get()       const noexcept { return owner_->index_[idx_].payload; }

        template<typename T> const T& as() const {
            return *static_cast<const T*>(owner_->index_[idx_].payload);
        }
        const vtable_t* vtable() const noexcept {
            return owner_->index_[idx_].vt;
        }
    };

    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;

    // ── Special members ───────────────────────────────────────────────
    any_deque() = default;

    any_deque(const any_deque& o) {
        for (std::size_t i = 0; i < o.index_.size(); ++i) {
            slot_t s = o.index_[i];
            void* d = allocate_back(s.vt->size, s.vt->align);
            s.vt->copy_construct(d, s.payload);
            index_.push_back({d, s.vt});
        }
    }

    any_deque(any_deque&& o) noexcept
        : blocks_(std::move(o.blocks_))
        , index_(std::move(o.index_))
    {}

    ~any_deque() { clear(); }

    any_deque& operator=(const any_deque& o) {
        if (this != &o) { any_deque t(o); swap(t); }
        return *this;
    }
    any_deque& operator=(any_deque&& o) noexcept {
        if (this != &o) {
            clear();
            blocks_ = std::move(o.blocks_);
            index_  = std::move(o.index_);
        }
        return *this;
    }

    // ── Capacity ──────────────────────────────────────────────────────
    [[nodiscard]] bool        empty()    const noexcept { return index_.empty(); }
    [[nodiscard]] std::size_t size()     const noexcept { return index_.size(); }
    [[nodiscard]] std::size_t max_size() const noexcept { return index_.max_size(); }
    [[nodiscard]] std::size_t byte_size() const noexcept {
        std::size_t total = 0;
        for (const auto& b : blocks_) total += b.data.size();
        return total;
    }

    // ── Iterators ─────────────────────────────────────────────────────
    iterator begin() noexcept { return {this, 0}; }
    iterator end()   noexcept { return {this, index_.size()}; }

    const_iterator begin()  const noexcept { return {this, 0}; }
    const_iterator end()    const noexcept { return {this, index_.size()}; }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend()   const noexcept { return end(); }

    reverse_iterator       rbegin()  noexcept { return reverse_iterator(end()); }
    reverse_iterator       rend()    noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend()    const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    const_reverse_iterator crend()   const noexcept { return rend(); }

    // ── Element access ────────────────────────────────────────────────
    void*       front()       noexcept { return index_.front().payload; }
    const void* front() const noexcept { return index_.front().payload; }
    void*       back()        noexcept { return index_.back().payload; }
    const void* back()  const noexcept { return index_.back().payload; }

    template<typename T> T&       front_as()       { return *static_cast<T*>(front()); }
    template<typename T> const T& front_as() const { return *static_cast<const T*>(front()); }
    template<typename T> T&       back_as()        { return *static_cast<T*>(back()); }
    template<typename T> const T& back_as()  const { return *static_cast<const T*>(back()); }

    // ── Modifiers ─────────────────────────────────────────────────────
    template<typename T, typename... Args>
    T& emplace_back(Args&&... args) {
        const vtable_t* vt = vtable_for<T>();
        void* p = allocate_back(vt->size, vt->align);
        T* obj = ::new(p) T(std::forward<Args>(args)...);
        index_.push_back({p, vt});
        return *obj;
    }

    template<typename T>
    void push_back(T&& v) { emplace_back<std::decay_t<T>>(std::forward<T>(v)); }

    template<typename T, typename... Args>
    T& emplace_front(Args&&... args) {
        const vtable_t* vt = vtable_for<T>();
        void* p = allocate_front(vt->size, vt->align);
        T* obj = ::new(p) T(std::forward<Args>(args)...);
        index_.push_front({p, vt});
        return *obj;
    }

    template<typename T>
    void push_front(T&& v) { emplace_front<std::decay_t<T>>(std::forward<T>(v)); }

    void pop_back() {
        assert(!empty());
        slot_t s = index_.back();
        s.vt->destroy(s.payload);
        pop_back_raw();
    }

    void pop_front() {
        assert(!empty());
        slot_t s = index_.front();
        s.vt->destroy(s.payload);
        pop_front_raw();
    }

    void clear() noexcept {
        for (std::size_t i = 0; i < index_.size(); ++i) {
            slot_t s = index_[i];
            s.vt->destroy(s.payload);
        }
        index_.clear();
        blocks_.clear();
    }

    void swap(any_deque& o) noexcept {
        std::swap(blocks_, o.blocks_);
        std::swap(index_,  o.index_);
    }
    friend void swap(any_deque& a, any_deque& b) noexcept { a.swap(b); }

    // insert / emplace at arbitrary position (O(n))
    template<typename T>
    iterator insert(const_iterator pos, T&& v) {
        return emplace<std::decay_t<T>>(pos, std::forward<T>(v));
    }

    template<typename T, typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        std::size_t idx = pos.idx_;
        any_deque tail;
        splice_tail_out(idx, tail);
        const vtable_t* vt = vtable_for<T>();
        void* p = allocate_back(vt->size, vt->align);
        ::new(p) T(std::forward<Args>(args)...);
        index_.push_back({p, vt});
        std::size_t new_idx = index_.size() - 1;
        splice_tail_in(tail);
        return {this, new_idx};
    }

    iterator erase(const_iterator pos) {
        return erase(pos, const_iterator{this, pos.idx_ + 1});
    }

    iterator erase(const_iterator first, const_iterator last) {
        std::size_t f = first.idx_, l = last.idx_;
        if (f == l) return {this, f};

        any_deque tail;
        splice_tail_out(l, tail);

        for (std::size_t i = f; i < l; ++i) {
            slot_t s = index_[i];
            s.vt->destroy(s.payload);
        }
        while (index_.size() > f) {
            pop_back_raw();
        }

        splice_tail_in(tail);
        return {this, f};
    }

    // ── Type query ────────────────────────────────────────────────────
    template<typename T>
    static bool holds_type(iterator it) noexcept {
        return it.vtable() == vtable_for<T>();
    }
    template<typename T>
    static bool holds_type(const_iterator it) noexcept {
        return it.vtable() == vtable_for<T>();
    }
};
