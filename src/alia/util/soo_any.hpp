#ifndef SOO_ANY_A9188CF7_8309_48F9_ACBB_284E3029AA2B
#define SOO_ANY_A9188CF7_8309_48F9_ACBB_284E3029AA2B

#include <type_traits>
#include <typeinfo>
#include <utility>
#include <stdexcept>
#include <new>
#include <cstring>

// Exception thrown when bad_any_cast occurs
class bad_soo_any_cast : public std::bad_cast {
public:
    const char* what() const noexcept override {
        return "bad any_cast";
    }
};

// Small object optimized any with configurable buffer size
template<std::size_t SmallBufferSize = 32>
class soo_any {
private:
    // Type operations interface
    struct ops_table {
        void (*destroy)(void* storage) noexcept;
        void (*copy)(const void* src, void* dst);
        void (*move)(void* src, void* dst) noexcept;
        const std::type_info& (*type)() noexcept;
    };

    // Storage strategy enum
    enum class storage_type : unsigned char {
        empty,
        small,  // Stored in local buffer
        large   // Stored on heap
    };

    // Small buffer for SOO
    alignas(std::max_align_t) unsigned char buffer_[SmallBufferSize];
    
    // Pointer for large objects (reuses buffer space when possible)
    void* ptr_;
    
    // Operations table pointer
    const ops_table* ops_;
    
    // Storage type indicator
    storage_type storage_;

    // Determine if type T should use small object optimization
    template<typename T>
    static constexpr bool use_soo() {
        return sizeof(T) <= SmallBufferSize && 
               alignof(T) <= alignof(std::max_align_t) &&
               std::is_nothrow_move_constructible_v<T>;
    }

    // Get pointer to stored value
    void* get_ptr() noexcept {
        return storage_ == storage_type::small ? 
               static_cast<void*>(buffer_) : ptr_;
    }

    const void* get_ptr() const noexcept {
        return storage_ == storage_type::small ? 
               static_cast<const void*>(buffer_) : ptr_;
    }

    // Operations table implementations for small objects
    template<typename T>
    static void destroy_small(void* storage) noexcept {
        static_cast<T*>(storage)->~T();
    }

    template<typename T>
    static void copy_small(const void* src, void* dst) {
        new (dst) T(*static_cast<const T*>(src));
    }

    template<typename T>
    static void move_small(void* src, void* dst) noexcept {
        new (dst) T(std::move(*static_cast<T*>(src)));
    }

    // Operations table implementations for large objects
    template<typename T>
    static void destroy_large(void* storage) noexcept {
        delete static_cast<T*>(*static_cast<T**>(storage));
    }

    template<typename T>
    static void copy_large(const void* src, void* dst) {
        *static_cast<T**>(dst) = new T(**static_cast<T* const*>(src));
    }

    template<typename T>
    static void move_large(void* src, void* dst) noexcept {
        *static_cast<T**>(dst) = *static_cast<T**>(src);
        *static_cast<T**>(src) = nullptr;
    }

    // Type info getter
    template<typename T>
    static const std::type_info& get_type() noexcept {
        return typeid(T);
    }

    // Get operations table for type T
    template<typename T>
    static const ops_table* get_ops_table() noexcept {
        using decay_t = std::decay_t<T>;
        
        if constexpr (use_soo<decay_t>()) {
            static constexpr ops_table table = {
                &destroy_small<decay_t>,
                &copy_small<decay_t>,
                &move_small<decay_t>,
                &get_type<decay_t>
            };
            return &table;
        } else {
            static constexpr ops_table table = {
                &destroy_large<decay_t>,
                &copy_large<decay_t>,
                &move_large<decay_t>,
                &get_type<decay_t>
            };
            return &table;
        }
    }

    // Destroy currently stored value
    void destroy() noexcept {
        if (ops_) {
            ops_->destroy(storage_ == storage_type::small ? 
                         static_cast<void*>(buffer_) : 
                         static_cast<void*>(&ptr_));
        }
    }

public:
    // Default constructor - empty any
    soo_any() noexcept 
        : ptr_(nullptr)
        , ops_(nullptr)
        , storage_(storage_type::empty) {
    }

    // Copy constructor
    soo_any(const soo_any& other) 
        : ops_(other.ops_)
        , storage_(other.storage_) {
        
        if (ops_) {
            if (storage_ == storage_type::small) {
                ops_->copy(other.buffer_, buffer_);
            } else {
                ops_->copy(&other.ptr_, &ptr_);
            }
        } else {
            ptr_ = nullptr;
        }
    }

    // Move constructor
    soo_any(soo_any&& other) noexcept 
        : ops_(other.ops_)
        , storage_(other.storage_) {
        
        if (ops_) {
            if (storage_ == storage_type::small) {
                ops_->move(other.buffer_, buffer_);
                other.destroy();
            } else {
                ptr_ = other.ptr_;
                other.ptr_ = nullptr;
            }
        } else {
            ptr_ = nullptr;
        }
        
        other.ops_ = nullptr;
        other.storage_ = storage_type::empty;
    }

    // Value constructor
    template<typename T, 
             typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, soo_any>>>
    soo_any(T&& value) {
        using decay_t = std::decay_t<T>;
        
        ops_ = get_ops_table<decay_t>();
        
        if constexpr (use_soo<decay_t>()) {
            storage_ = storage_type::small;
            new (buffer_) decay_t(std::forward<T>(value));
        } else {
            storage_ = storage_type::large;
            ptr_ = new decay_t(std::forward<T>(value));
        }
    }

    // Destructor
    ~soo_any() {
        destroy();
    }

    // Copy assignment
    soo_any& operator=(const soo_any& other) {
        if (this != &other) {
            soo_any temp(other);
            swap(temp);
        }
        return *this;
    }

    // Move assignment
    soo_any& operator=(soo_any&& other) noexcept {
        if (this != &other) {
            destroy();
            
            ops_ = other.ops_;
            storage_ = other.storage_;
            
            if (ops_) {
                if (storage_ == storage_type::small) {
                    ops_->move(other.buffer_, buffer_);
                    other.destroy();
                } else {
                    ptr_ = other.ptr_;
                    other.ptr_ = nullptr;
                }
            } else {
                ptr_ = nullptr;
            }
            
            other.ops_ = nullptr;
            other.storage_ = storage_type::empty;
        }
        return *this;
    }

    // Value assignment
    template<typename T,
             typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, soo_any>>>
    soo_any& operator=(T&& value) {
        soo_any temp(std::forward<T>(value));
        swap(temp);
        return *this;
    }

    // Emplace construction
    template<typename T, typename... Args>
    std::decay_t<T>& emplace(Args&&... args) {
        using decay_t = std::decay_t<T>;
        
        destroy();
        
        ops_ = get_ops_table<decay_t>();
        
        if constexpr (use_soo<decay_t>()) {
            storage_ = storage_type::small;
            new (buffer_) decay_t(std::forward<Args>(args)...);
            return *reinterpret_cast<decay_t*>(buffer_);
        } else {
            storage_ = storage_type::large;
            ptr_ = new decay_t(std::forward<Args>(args)...);
            return *static_cast<decay_t*>(ptr_);
        }
    }

    // Reset to empty state
    void reset() noexcept {
        destroy();
        ops_ = nullptr;
        storage_ = storage_type::empty;
        ptr_ = nullptr;
    }

    // Swap
    void swap(soo_any& other) noexcept {
        soo_any temp(std::move(other));
        other = std::move(*this);
        *this = std::move(temp);
    }

    // Check if contains a value
    bool has_value() const noexcept {
        return ops_ != nullptr;
    }

    // Get type info
    const std::type_info& type() const noexcept {
        return ops_ ? ops_->type() : typeid(void);
    }

    // Friend cast functions
    template<typename T, std::size_t Size>
    friend T* any_cast(soo_any<Size>* operand) noexcept;

    template<typename T, std::size_t Size>
    friend const T* any_cast(const soo_any<Size>* operand) noexcept;
};

// any_cast for pointers
template<typename T, std::size_t Size>
T* any_cast(soo_any<Size>* operand) noexcept {
    using decay_t = std::decay_t<T>;
    
    if (!operand || operand->type() != typeid(decay_t)) {
        return nullptr;
    }
    
    if (operand->storage_ == soo_any<Size>::storage_type::small) {
        return reinterpret_cast<decay_t*>(operand->buffer_);
    } else {
        return static_cast<decay_t*>(operand->ptr_);
    }
}

template<typename T, std::size_t Size>
const T* any_cast(const soo_any<Size>* operand) noexcept {
    using decay_t = std::decay_t<T>;
    
    if (!operand || operand->type() != typeid(decay_t)) {
        return nullptr;
    }
    
    if (operand->storage_ == soo_any<Size>::storage_type::small) {
        return reinterpret_cast<const decay_t*>(operand->buffer_);
    } else {
        return static_cast<const decay_t*>(operand->ptr_);
    }
}

// any_cast for references
template<typename T, std::size_t Size>
T any_cast(soo_any<Size>& operand) {
    using nonref = std::remove_cv_t<std::remove_reference_t<T>>;
    auto* result = any_cast<nonref>(&operand);
    if (!result) {
        throw bad_soo_any_cast();
    }
    return static_cast<T>(*result);
}

template<typename T, std::size_t Size>
T any_cast(const soo_any<Size>& operand) {
    using nonref = std::remove_cv_t<std::remove_reference_t<T>>;
    auto* result = any_cast<nonref>(&operand);
    if (!result) {
        throw bad_soo_any_cast();
    }
    return static_cast<T>(*result);
}

template<typename T, std::size_t Size>
T any_cast(soo_any<Size>&& operand) {
    using nonref = std::remove_cv_t<std::remove_reference_t<T>>;
    auto* result = any_cast<nonref>(&operand);
    if (!result) {
        throw bad_soo_any_cast();
    }
    return static_cast<T>(std::move(*result));
}

// Helper function to create soo_any
template<typename T, std::size_t Size = 32, typename... Args>
soo_any<Size> make_any(Args&&... args) {
    soo_any<Size> result;
    result.template emplace<T>(std::forward<Args>(args)...);
    return result;
}

#endif /* SOO_ANY_A9188CF7_8309_48F9_ACBB_284E3029AA2B */
