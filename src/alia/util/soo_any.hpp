#ifndef SOO_ANY_BB3690F1_CC6C_44CB_B46B_F8FA87F482E7
#define SOO_ANY_BB3690F1_CC6C_44CB_B46B_F8FA87F482E7

#include "type_erasure.hpp"
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <new>
#include <cstring>

// Exception thrown when bad_any_cast occurs
class bad_soo_any_cast : public std::bad_cast {
public:
    const char* what() const noexcept override {
        return "bad any_cast";
    }
};

// Small object optimized any with configurable buffer size.
//
// Uses type_erasure_vtable_t as its vtable. Vtable functions always receive
// direct object pointers; soo_any manages heap allocation separately.
//
// Layout (two fields only):
//   ops_ == nullptr  →  empty
//   is_small()       →  object lives in buffer_
//   else             →  heap pointer stored in buffer_
template<std::size_t SmallBufferSize = 32>
class soo_any {
    static_assert(SmallBufferSize >= sizeof(void*),
                  "SmallBufferSize must be at least sizeof(void*)");

private:
    alignas(std::max_align_t) unsigned char buffer_[SmallBufferSize];
    const type_erasure_vtable_t*            ops_;

    template<typename T>
    static constexpr bool use_soo() noexcept {
        return sizeof(T) <= SmallBufferSize &&
               alignof(T) <= alignof(std::max_align_t) &&
               std::is_nothrow_move_constructible_v<T>;
    }

    // Caller must ensure ops_ != nullptr.
    bool is_small() const noexcept {
        return ops_->size  <= SmallBufferSize &&
               ops_->align <= alignof(std::max_align_t) &&
               ops_->is_nothrow_move;
    }

    void* load_ptr() noexcept {
        void* p;
        std::memcpy(&p, buffer_, sizeof(void*));
        return p;
    }
    const void* load_ptr() const noexcept {
        void* p;
        std::memcpy(&p, buffer_, sizeof(void*));
        return p;
    }
    void store_ptr(void* p) noexcept {
        std::memcpy(buffer_, &p, sizeof(void*));
    }

    // Returns pointer to the live object (regardless of storage path).
    void* get_ptr() noexcept {
        return is_small() ? static_cast<void*>(buffer_) : load_ptr();
    }
    const void* get_ptr() const noexcept {
        return is_small() ? static_cast<const void*>(buffer_) : load_ptr();
    }

    // Runs the destructor and, for large objects, frees the heap allocation.
    void destroy() noexcept {
        if (!ops_) return;
        if (is_small()) {
            ops_->destroy(buffer_);
        } else {
            void* p = load_ptr();
            ops_->destroy(p);
            ::operator delete(p, std::align_val_t{ops_->align});
        }
    }

public:
    // ── Constructors ──────────────────────────────────────────────────

    soo_any() noexcept : ops_(nullptr) {}

    soo_any(const soo_any& other) : ops_(other.ops_) {
        if (!ops_) return;
        if (other.is_small()) {
            ops_->copy_construct(buffer_, other.buffer_);
        } else {
            void* p = ::operator new(ops_->size, std::align_val_t{ops_->align});
            ops_->copy_construct(p, other.load_ptr());
            store_ptr(p);
        }
    }

    soo_any(soo_any&& other) noexcept : ops_(other.ops_) {
        if (!ops_) { /* empty */ }
        else if (other.is_small()) {
            ops_->move_construct(buffer_, other.buffer_);
            other.ops_->destroy(other.buffer_); // no dealloc needed
        } else {
            // steal the heap pointer
            std::memcpy(buffer_, other.buffer_, sizeof(void*));
        }
        other.ops_ = nullptr;
    }

    // Value constructor – constructs T in-place.
    template<typename T,
             typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, soo_any>>>
    soo_any(T&& value) {
        using D = std::decay_t<T>;
        ops_ = type_erasure_vtable_for<D>();
        if constexpr (use_soo<D>()) {
            ::new(buffer_) D(std::forward<T>(value));
        } else {
            void* p = ::operator new(sizeof(D), std::align_val_t{alignof(D)});
            ::new(p) D(std::forward<T>(value));
            store_ptr(p);
        }
    }

    // Tagged move-construct from an external object pointer and its vtable.
    // Useful when type information is only available at runtime (e.g. when
    // extracting an event payload from a type-erased queue).
    soo_any(move_construct_tag_t, const type_erasure_vtable_t* vt, void* obj) {
        ops_ = vt;
        if (is_small()) {
            vt->move_construct(buffer_, obj);
        } else {
            void* p = ::operator new(vt->size, std::align_val_t{vt->align});
            vt->move_construct(p, obj);
            store_ptr(p);
        }
    }

    ~soo_any() { destroy(); }

    // ── Assignment ────────────────────────────────────────────────────

    soo_any& operator=(const soo_any& other) {
        if (this != &other) { soo_any temp(other); swap(temp); }
        return *this;
    }

    soo_any& operator=(soo_any&& other) noexcept {
        if (this == &other) return *this;
        destroy();
        ops_ = other.ops_;
        if (!ops_) { /* empty */ }
        else if (other.is_small()) {
            ops_->move_construct(buffer_, other.buffer_);
            other.ops_->destroy(other.buffer_);
        } else {
            std::memcpy(buffer_, other.buffer_, sizeof(void*));
        }
        other.ops_ = nullptr;
        return *this;
    }

    template<typename T,
             typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, soo_any>>>
    soo_any& operator=(T&& value) {
        soo_any temp(std::forward<T>(value));
        swap(temp);
        return *this;
    }

    // ── Emplace ───────────────────────────────────────────────────────

    template<typename T, typename... Args>
    std::decay_t<T>& emplace(Args&&... args) {
        using D = std::decay_t<T>;
        destroy();
        ops_ = type_erasure_vtable_for<D>();
        if constexpr (use_soo<D>()) {
            ::new(buffer_) D(std::forward<Args>(args)...);
            return *reinterpret_cast<D*>(buffer_);
        } else {
            void* p = ::operator new(sizeof(D), std::align_val_t{alignof(D)});
            ::new(p) D(std::forward<Args>(args)...);
            store_ptr(p);
            return *static_cast<D*>(p);
        }
    }

    // ── Modifiers ─────────────────────────────────────────────────────

    void reset() noexcept {
        destroy();
        ops_ = nullptr;
    }

    void swap(soo_any& other) noexcept {
        soo_any temp(std::move(other));
        other = std::move(*this);
        *this = std::move(temp);
    }

    // ── Observers ─────────────────────────────────────────────────────

    bool has_value() const noexcept { return ops_ != nullptr; }

    const std::type_info& type() const noexcept {
        return ops_ ? ops_->type_id() : typeid(void);
    }

    // Unchecked raw pointer to the stored object.
    // Caller must ensure the type is correct.
    void*       get_ptr_unchecked() noexcept       { return get_ptr(); }
    const void* get_ptr_unchecked() const noexcept { return get_ptr(); }

    // ── Friend casts ──────────────────────────────────────────────────

    template<typename T, std::size_t Size>
    friend T* any_cast(soo_any<Size>* operand) noexcept;

    template<typename T, std::size_t Size>
    friend const T* any_cast(const soo_any<Size>* operand) noexcept;
};

// ── any_cast ──────────────────────────────────────────────────────────

template<typename T, std::size_t Size>
T* any_cast(soo_any<Size>* operand) noexcept {
    using D = std::decay_t<T>;
    if (!operand || operand->type() != typeid(D)) return nullptr;
    return static_cast<D*>(operand->get_ptr());
}

template<typename T, std::size_t Size>
const T* any_cast(const soo_any<Size>* operand) noexcept {
    using D = std::decay_t<T>;
    if (!operand || operand->type() != typeid(D)) return nullptr;
    return static_cast<const D*>(operand->get_ptr());
}

template<typename T, std::size_t Size>
T any_cast(soo_any<Size>& operand) {
    using nonref = std::remove_cv_t<std::remove_reference_t<T>>;
    auto* result = any_cast<nonref>(&operand);
    if (!result) throw bad_soo_any_cast();
    return static_cast<T>(*result);
}

template<typename T, std::size_t Size>
T any_cast(const soo_any<Size>& operand) {
    using nonref = std::remove_cv_t<std::remove_reference_t<T>>;
    auto* result = any_cast<nonref>(&operand);
    if (!result) throw bad_soo_any_cast();
    return static_cast<T>(*result);
}

template<typename T, std::size_t Size>
T any_cast(soo_any<Size>&& operand) {
    using nonref = std::remove_cv_t<std::remove_reference_t<T>>;
    auto* result = any_cast<nonref>(&operand);
    if (!result) throw bad_soo_any_cast();
    return static_cast<T>(std::move(*result));
}

// ── make_any ──────────────────────────────────────────────────────────

template<typename T, std::size_t Size = 32, typename... Args>
soo_any<Size> make_any(Args&&... args) {
    soo_any<Size> result;
    result.template emplace<T>(std::forward<Args>(args)...);
    return result;
}

#endif /* SOO_ANY_BB3690F1_CC6C_44CB_B46B_F8FA87F482E7 */
