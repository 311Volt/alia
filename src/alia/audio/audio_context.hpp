#ifndef ALIA_AUDIO_CONTEXT_HPP
#define ALIA_AUDIO_CONTEXT_HPP

#include "audio_backend_interface.hpp"
#include <memory>

namespace alia {
    struct audio_output_device {
        std::string id, name;
        bool is_default = false;
        audio_stream_format preferred;
    };

    struct audio_input_device {
        std::string id, name;
        bool is_default = false;
        audio_stream_format preferred;
    };

    /// Root audio owner. Every stream holds raw aliases into this object:
    /// destroy streams first. Moving a context preserves those aliases.
    class audio_context {
    public:
        audio_context() = default;
        ~audio_context();
        audio_context(audio_context &&) noexcept;
        audio_context &operator=(audio_context &&) noexcept;
        audio_context(const audio_context &) = delete;
        audio_context &operator=(const audio_context &) = delete;
        static audio_context create(audio_backend pref = audio_backend::auto_, const audio_context_config & = {});

        [[nodiscard]] bool valid() const noexcept {
            return handle_ != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return valid();
        }

        [[nodiscard]] std::vector<audio_output_device> output_devices() const;
        [[nodiscard]] std::vector<audio_input_device> input_devices() const;
        [[nodiscard]] std::optional<audio_output_device> default_output_device() const;
        [[nodiscard]] std::optional<audio_input_device> default_input_device() const;

        [[nodiscard]] const audio_backend_interface *backend() const noexcept {
            return backend_.get();
        }

        [[nodiscard]] audio_context_handle *handle() const noexcept {
            return handle_;
        }

        [[nodiscard]] const audio_caps &caps() const;

    private:
        audio_context_handle *handle_ = nullptr;
        std::unique_ptr<audio_backend_interface> backend_;
    };
} // namespace alia
#endif
