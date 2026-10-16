#include "os_context.hpp"
#include <mutex>
#include <utility>

#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
namespace alia {
    void register_win32_os_backend();
}
#endif
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_X11
namespace alia {
    void register_x11_os_backend();
}
#endif

namespace alia {
    namespace {
        std::mutex &registry_mutex() {
            static std::mutex value;
            return value;
        }

        std::vector<os_backend_factory> &registry() {
            static std::vector<os_backend_factory> value;
            return value;
        }

        void ensure_backends() {
            static const bool initialized = [] {
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
                register_win32_os_backend();
#endif
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_X11
                register_x11_os_backend();
#endif
                return true;
            }();
            (void)initialized;
        }
    } // namespace

    void register_os_backend(os_backend_factory factory) {
        if (factory.id == os_backend::auto_ || factory.id == os_backend::null || !factory.create)
            throw std::invalid_argument("invalid OS backend factory");
        std::lock_guard lock(registry_mutex());
        for (auto &entry : registry())
            if (entry.id == factory.id) {
                entry = factory;
                return;
            }
        registry().push_back(factory);
    }

    os_context os_context::create(os_backend pref) {
        ensure_backends();
        std::vector<os_backend_factory> snapshot;
        {
            std::lock_guard lock(registry_mutex());
            snapshot = registry();
        }
        auto attempt = [&](os_backend id) -> os_context {
            for (const auto &factory : snapshot)
                if (factory.id == id) {
                    auto created = factory.create();
                    if (!created.handle)
                        continue;
                    os_context result;
                    try {
                        result.backend_ = std::make_unique<os_backend_interface>(std::move(created.iface));
                    } catch (...) {
                        created.iface.destroy_context.get_or_throw()(created.handle);
                        throw;
                    }
                    result.handle_ = created.handle;
                    return result;
                }
            return {};
        };
        if (pref == os_backend::auto_) {
            for (auto id : {os_backend::win32, os_backend::x11})
                if (auto result = attempt(id))
                    return result;
        } else if (auto result = attempt(pref))
            return result;
        throw std::runtime_error("os_context::create: no usable OS backend");
    }

    os_context::~os_context() {
        if (handle_)
            backend_->destroy_context.get_or_throw()(handle_);
    }

    os_context::os_context(os_context &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)), backend_(std::move(other.backend_)) {
    }

    os_context &os_context::operator=(os_context &&other) noexcept {
        if (this != &other) {
            if (handle_)
                backend_->destroy_context.get_or_throw()(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
            backend_ = std::move(other.backend_);
        }
        return *this;
    }

    void os_context::require_valid() const {
        if (!handle_)
            throw std::logic_error("OS context is not valid");
    }

    void os_context::poll() {
        require_valid();
        backend_->poll.get_or_throw()(handle_);
    }

    void *os_context::native_display() const {
        require_valid();
        return backend_->native_display.get_or_throw()(handle_);
    }

    std::vector<monitor_info> os_context::monitors() const {
        require_valid();
        return backend_->monitors.get_or_throw()(handle_);
    }

    std::vector<display_mode> os_context::display_modes(int index) const {
        require_valid();
        return backend_->display_modes.get_or_throw()(handle_, index);
    }

    display_mode os_context::desktop_mode(int index) const {
        require_valid();
        return backend_->desktop_mode.get_or_throw()(handle_, index);
    }

    os_context &get_os_context() {
        thread_local auto value = os_context::create();
        return value;
    }
} // namespace alia
