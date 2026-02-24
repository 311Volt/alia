#pragma once

// type_erasure.hpp – shared vtable for soo_any and any_deque
//
// Functions take direct object pointers (never pointer-to-pointer).
// Callers are responsible for heap allocation/deallocation; vtable functions
// only construct/destruct – they never allocate or free memory.

#include <cstddef>
#include <typeinfo>
#include <type_traits>
#include <utility>

struct type_erasure_vtable_t {
    std::size_t size;
    std::size_t align;
    bool        is_nothrow_move; ///< true iff T is nothrow-move-constructible
    void (*copy_construct)(void* dst, const void* src);
    void (*move_construct)(void* dst, void* src) noexcept; ///< move-constructs into dst; caller owns both allocations
    void (*destroy)       (void* obj)            noexcept; ///< calls destructor only – does NOT free memory
    const std::type_info& (*type_id)() noexcept;
};

struct move_construct_tag_t {};
inline constexpr move_construct_tag_t move_construct_tag{};

template<typename T>
const type_erasure_vtable_t* type_erasure_vtable_for() noexcept {
    static const type_erasure_vtable_t vt = {
        sizeof(T),
        alignof(T),
        std::is_nothrow_move_constructible_v<T>,
        [](void* d, const void* s) { ::new(d) T(*static_cast<const T*>(s)); },
        [](void* d, void* s) noexcept { ::new(d) T(std::move(*static_cast<T*>(s))); },
        [](void* p) noexcept { static_cast<T*>(p)->~T(); },
        []() noexcept -> const std::type_info& { return typeid(T); },
    };
    return &vt;
}
