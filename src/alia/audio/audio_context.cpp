#include "audio_context.hpp"
#include <mutex>
#include <utility>

#ifdef ALIA_COMPILE_AUDIO_BACKEND_WASAPI
namespace alia {
    void register_wasapi_audio_backend();
}
#endif
#ifdef ALIA_COMPILE_AUDIO_BACKEND_MME
namespace alia {
    void register_mme_audio_backend();
}
#endif
#ifdef ALIA_COMPILE_AUDIO_BACKEND_ALSA
namespace alia {
    void register_alsa_audio_backend();
}
#endif

namespace alia {
    namespace {
        std::mutex &factories_mutex() {
            static std::mutex m;
            return m;
        }

        std::vector<audio_backend_factory> &factories() {
            static std::vector<audio_backend_factory> f;
            return f;
        }

        void ensure_backends_registered() {
            static const bool registered = [] {
#ifdef ALIA_COMPILE_AUDIO_BACKEND_WASAPI
                register_wasapi_audio_backend();
#endif
#ifdef ALIA_COMPILE_AUDIO_BACKEND_MME
                register_mme_audio_backend();
#endif
#ifdef ALIA_COMPILE_AUDIO_BACKEND_ALSA
                register_alsa_audio_backend();
#endif
                return true;
            }();
            (void)registered;
        }
    } // namespace

    void register_audio_backend(audio_backend_factory factory) {
        if (factory.id == audio_backend::auto_ || !factory.create)
            throw std::invalid_argument("invalid audio backend factory");
        std::lock_guard lock(factories_mutex());
        for (auto &entry : factories())
            if (entry.id == factory.id) {
                entry = factory;
                return;
            }
        factories().push_back(factory);
    }

    audio_context audio_context::create(audio_backend pref, const audio_context_config &config) {
        ensure_backends_registered();
        std::vector<audio_backend_factory> snapshot;
        {
            std::lock_guard lock(factories_mutex());
            snapshot = factories();
        }
        const audio_backend order[] = {audio_backend::wasapi, audio_backend::mme, audio_backend::alsa};
        auto try_create = [&](audio_backend id) -> audio_context {
            for (const auto &factory : snapshot)
                if (factory.id == id) {
                    auto created = factory.create(config);
                    if (!created.handle)
                        continue;
                    audio_context result;
                    try {
                        result.backend_ = std::make_unique<audio_backend_interface>(std::move(created.iface));
                    } catch (...) {
                        created.iface.destroy_context(created.handle);
                        throw;
                    }
                    result.handle_ = created.handle;
                    return result;
                }
            return {};
        };
        if (pref == audio_backend::auto_) {
            for (auto id : order)
                if (auto context = try_create(id))
                    return context;
        } else if (auto context = try_create(pref))
            return context;
        throw std::runtime_error("audio_context::create: no usable audio backend");
    }

    audio_context::~audio_context() {
        if (handle_)
            backend_->destroy_context(handle_);
    }

    audio_context::audio_context(audio_context &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)), backend_(std::move(other.backend_)) {
    }

    audio_context &audio_context::operator=(audio_context &&other) noexcept {
        if (this != &other) {
            if (handle_)
                backend_->destroy_context(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
            backend_ = std::move(other.backend_);
        }
        return *this;
    }

    const audio_caps &audio_context::caps() const {
        if (!handle_)
            throw std::logic_error("audio context is not valid");
        return backend_->caps;
    }

    std::vector<audio_output_device> audio_context::output_devices() const {
        (void)caps();
        std::vector<audio_output_device> result;
        for (auto &d : backend_->enumerate_devices(handle_, audio_direction::output))
            result.push_back({std::move(d.id), std::move(d.name), d.is_default, d.preferred});
        return result;
    }

    std::vector<audio_input_device> audio_context::input_devices() const {
        (void)caps();
        std::vector<audio_input_device> result;
        for (auto &d : backend_->enumerate_devices(handle_, audio_direction::input))
            result.push_back({std::move(d.id), std::move(d.name), d.is_default, d.preferred});
        return result;
    }

    std::optional<audio_output_device> audio_context::default_output_device() const {
        for (auto &d : output_devices())
            if (d.is_default)
                return std::move(d);
        return std::nullopt;
    }

    std::optional<audio_input_device> audio_context::default_input_device() const {
        for (auto &d : input_devices())
            if (d.is_default)
                return std::move(d);
        return std::nullopt;
    }
} // namespace alia
