#ifndef ALIA_CORE_BACKEND_OP_HPP
#define ALIA_CORE_BACKEND_OP_HPP

#include <optional>
#include <stdexcept>
#include <string>

namespace alia {
    struct unsupported_operation_exception : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    template <class Signature>
    struct backend_op;

    /// A null slot denotes an unavailable hardware/platform feature, never an
    /// unfinished backend implementation. reason_unsupported explains why.
    template <class R, class... Args>
    struct backend_op<R(Args...)> {
        R (*operation)(Args...) = nullptr;
        std::optional<std::string> reason_unsupported;

        [[nodiscard]] bool is_supported() const noexcept { return operation != nullptr; }
        auto get_or_throw() const -> R (*)(Args...) {
            if (operation)
                return operation;
            throw unsupported_operation_exception(reason_unsupported.value_or("operation not supported by this backend"));
        }
    };
}
#endif
