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
template<std::size_t SmallBufferSize = 32>
class soo_any {
private:
    enum class storage_type : unsigned char {
        empty,
        small,  // object lives in buffer_
        large   // object lives on heap; ptr_ holds the raw allocation
    };

    alignas(std::max_align_t) unsigned char buffer_[SmallBufferSize];
    void*                               ptr_;     // heap allocation for large objects
    const type_erasure_vtable_t*        ops_;
    storage_type                        storage_;

    template<typename T>
    static constexpr bool use_soo() noexcept {
        return sizeof(T) <= SmallBufferSize &&
               alignof(T) <= alignof(std::max_align_t) &&
               std::is_nothrow_move_constructible_v<T>;
    }

    // Returns pointer to the live object (regardless of storage path).
    void* get_ptr() noexcept {
        return storage_ == storage_type::small ? static_cast<void*>(buffer_) : ptr_;
    }
    const void* get_ptr() const noexcept {
        return storage_ == storage_type::small ? static_cast<const void*>(buffer_) : ptr_;
    }

    // Runs the destructor and, for large objects, frees the heap allocation.
    void destroy() noexcept {
        if (!ops_) return;
        void* obj = get_ptr();
        ops_->destroy(obj);
        if (storage_ == storage_type::large) {
            ::operator delete(ptr_, std::align_val_t{ops_->align});
        }
    }

public:
    // ── Constructors ──────────────────────────────────────────────────

    soo_any() noexcept : ptr_(nullptr), ops_(nullptr), storage_(storage_type::empty) {}

    soo_any(const soo_any& other) : ops_(other.ops_), storage_(other.storage_) {
        if (!ops_) { ptr_ = nullptr; return; }
        if (storage_ == storage_type::small) {
            ops_->copy_construct(buffer_, other.buffer_);
        } else {
            ptr_ = ::operator new(ops_->size, std::align_val_t{ops_->align});
            ops_->copy_construct(ptr_, other.ptr_);
        }
    }

    soo_any(soo_any&& other) noexcept : ops_(other.ops_), storage_(other.storage_) {
        if (!ops_) { ptr_ = nullptr; }
        else if (storage_ == storage_type::small) {
            ops_->move_construct(buffer_, other.buffer_);
            other.ops_->destroy(other.buffer_); // no dealloc needed
        } else {
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        other.ops_ = nullptr;
        other.storage_ = storage_type::empty;
    }

    // Value constructor – constructs T in-place.
    template<typename T,
             typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, soo_any>>>
    soo_any(T&& value) {
        using D = std::decay_t<T>;
        ops_ = type_erasure_vtable_for<D>();
        if constexpr (use_soo<D>()) {
            storage_ = storage_type::small;
            ::new(buffer_) D(std::forward<T>(value));
        } else {
            storage_ = storage_type::large;
            ptr_ = ::operator new(sizeof(D), std::align_val_t{alignof(D)});
            ::new(ptr_) D(std::forward<T>(value));
        }
    }

    // Tagged move-construct from an external object pointer and its vtable.
    // Useful when type information is only available at runtime (e.g. when
    // extracting an event payload from a type-erased queue).
    soo_any(move_construct_tag_t, const type_erasure_vtable_t* vt, void* obj) {
        ops_ = vt;
        bool small = vt->size <= SmallBufferSize &&
                     vt->align <= alignof(std::max_align_t) &&
                     vt->is_nothrow_move;
        if (small) {
            storage_ = storage_type::small;
            vt->move_construct(buffer_, obj);
        } else {
            storage_ = storage_type::large;
            ptr_ = ::operator new(vt->size, std::align_val_t{vt->align});
            vt->move_construct(ptr_, obj);
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
        ops_     = other.ops_;
        storage_ = other.storage_;
        if (!ops_) { ptr_ = nullptr; }
        else if (storage_ == storage_type::small) {
            ops_->move_construct(buffer_, other.buffer_);
            other.ops_->destroy(other.buffer_);
        } else {
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        other.ops_ = nullptr;
        other.storage_ = storage_type::empty;
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
            storage_ = storage_type::small;
            ::new(buffer_) D(std::forward<Args>(args)...);
            return *reinterpret_cast<D*>(buffer_);
        } else {
            storage_ = storage_type::large;
            ptr_ = ::operator new(sizeof(D), std::align_val_t{alignof(D)});
            ::new(ptr_) D(std::forward<Args>(args)...);
            return *static_cast<D*>(ptr_);
        }
    }

    // ── Modifiers ─────────────────────────────────────────────────────

    void reset() noexcept {
        destroy();
        ops_     = nullptr;
        storage_ = storage_type::empty;
        ptr_     = nullptr;
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
