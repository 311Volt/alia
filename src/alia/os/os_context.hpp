#ifndef ALIA_OS_CONTEXT_HPP
#define ALIA_OS_CONTEXT_HPP

#include "os_backend_interface.hpp"

namespace alia {
    // Root OS owner. Create/use/destroy on one thread; destroy children first.
    // Moves preserve the backend table and native context addresses.
    class os_context {
    public:
        os_context() = default;
        ~os_context();
        os_context(os_context &&) noexcept;
        os_context &operator=(os_context &&) noexcept;
        os_context(const os_context &) = delete;
        os_context &operator=(const os_context &) = delete;
        static os_context create(os_backend pref = os_backend::auto_);
        [[nodiscard]] bool valid() const noexcept {
            return handle_ != nullptr;
        }
        [[nodiscard]] explicit operator bool() const noexcept {
            return valid();
        }
        void poll();
        [[nodiscard]] std::vector<monitor_info> monitors() const;
        [[nodiscard]] std::vector<display_mode> display_modes(int monitor_index = 0) const;
        [[nodiscard]] display_mode desktop_mode(int monitor_index = 0) const;
        [[nodiscard]] void *native_display() const;
        [[nodiscard]] const os_backend_interface *backend() const noexcept {
            return backend_.get();
        }
        [[nodiscard]] os_context_handle *handle() const noexcept {
            return handle_;
        }

    private:
        os_context_handle *handle_ = nullptr;
        std::unique_ptr<os_backend_interface> backend_;
        void require_valid() const;
    };
    // Lazily created, thread-local context used by the legacy convenience APIs.
    os_context &get_os_context();
} // namespace alia
#endif
