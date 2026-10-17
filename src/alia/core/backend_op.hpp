#ifndef ALIA_CORE_BACKEND_OP_HPP
#define ALIA_CORE_BACKEND_OP_HPP

#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace alia {
    struct unsupported_operation_exception : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    template <class Signature>
    struct optional_backend_op;

    /// An operation a backend may lack. A null slot means the hardware/platform
    /// lacks the feature or the backend does not implement it yet;
    /// reason_unsupported explains which.
    template <class R, class... Args>
    struct optional_backend_op<R(Args...)> {
        R (*operation)(Args...) = nullptr;
        std::optional<std::string> reason_unsupported;

        [[nodiscard]] bool is_supported() const noexcept { return operation != nullptr; }
        auto get_or_throw() const -> R (*)(Args...) {
            if (operation)
                return operation;
            throw unsupported_operation_exception(reason_unsupported.value_or("operation not supported by this backend"));
        }
    };

    template <class Signature>
    struct backend_op;

    /// An operation every backend provides unconditionally, regardless of
    /// hardware or version. Its slot is never null, so it is called directly.
    template <class R, class... Args>
    struct backend_op<R(Args...)> {
        R (*operation)(Args...) = nullptr;

        R operator()(Args... args) const {
            assert(operation && "backend left a required op unset");
            return operation(std::forward<Args>(args)...);
        }
    };
}
#endif
