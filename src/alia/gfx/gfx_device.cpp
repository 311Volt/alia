#include "gfx_device.hpp"
#include "../os/window.hpp"

#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9
namespace alia { void register_d3d9_backend(); }
#endif
#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL
namespace alia { void register_ogl_backend(); }
#endif

namespace alia {
    namespace {
        std::vector<gfx_backend_factory> &backend_factories() {
            static std::vector<gfx_backend_factory> factories;
            return factories;
        }
        std::mutex &backend_factories_mutex() {
            static std::mutex mutex;
            return mutex;
        }
        void ensure_backends_registered() {
            static const bool registered = [] {
#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9
                register_d3d9_backend();
#endif
#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL
                register_ogl_backend();
#endif
                return true;
            }();
            (void)registered;
        }
    } // namespace

    void register_gfx_backend(gfx_backend_factory factory) {
        std::lock_guard lock(backend_factories_mutex());
        for (auto &registered : backend_factories()) {
            if (registered.id == factory.id) {
                registered = factory;
                return;
            }
        }
        backend_factories().push_back(factory);
    }

    gfx_device::~gfx_device() {
        if (device_)
            backend_->destroy_device.get_or_throw()(device_);
    }
    gfx_device::gfx_device(gfx_device &&other) noexcept
        : device_(std::exchange(other.device_, nullptr)), backend_(std::move(other.backend_)) {
        if (tl_current_device == &other)
            tl_current_device = this;
    }
    gfx_device &gfx_device::operator=(gfx_device &&other) noexcept {
        if (this != &other) {
            if (device_)
                backend_->destroy_device.get_or_throw()(device_);
            device_ = std::exchange(other.device_, nullptr);
            backend_ = std::move(other.backend_);
            if (tl_current_device == &other)
                tl_current_device = this;
        }
        return *this;
    }
    gfx_device gfx_device::create(gfx_backend pref) {
        ensure_backends_registered();
        std::lock_guard lock(backend_factories_mutex());
        for (const auto &factory : backend_factories()) {
            if (pref != gfx_backend::auto_ && factory.id != pref)
                continue;
            created_device created = factory.create();
            if (created.handle)
                return gfx_device(created.handle, std::make_unique<graphics_backend_interface>(std::move(created.iface)));
        }
        throw std::runtime_error("gfx_device::create: no usable graphics backend");
    }
    swapchain gfx_device::create_swapchain(const swapchain_config &config) {
        if (!*this)
            throw std::runtime_error("gfx_device::create_swapchain: device is not valid");
        const vec2i size = config.target.size();
        swapchain_handle *handle = backend_->create_swapchain.get_or_throw()(device_, config.target.native_handle(), size);
        if (!handle)
            throw std::runtime_error("gfx_device::create_swapchain: backend failed to create swapchain");
        return swapchain(handle, backend_.get(), size);
    }
    vec2f gfx_device::pixel_center_offset() const { return backend_ ? backend_->pixel_center_offset : vec2f{}; }

    swapchain::~swapchain() {
        if (handle_)
            backend_->destroy_swapchain.get_or_throw()(handle_);
    }
    swapchain::swapchain(swapchain &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)), backend_(std::exchange(other.backend_, nullptr)),
          size_(std::exchange(other.size_, {})), frame_active_(std::exchange(other.frame_active_, false)) {}
    swapchain &swapchain::operator=(swapchain &&other) noexcept {
        if (this != &other) {
            if (handle_)
                backend_->destroy_swapchain.get_or_throw()(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
            backend_ = std::exchange(other.backend_, nullptr);
            size_ = std::exchange(other.size_, {});
            frame_active_ = std::exchange(other.frame_active_, false);
        }
        return *this;
    }
    void swapchain::on_resize(vec2i new_size) {
        if (!handle_)
            throw std::logic_error("swapchain::on_resize: swapchain is not valid");
        if (frame_active_)
            throw std::logic_error("swapchain::on_resize: a frame is active");
        if (new_size.x <= 0 || new_size.y <= 0)
            throw std::invalid_argument("swapchain::on_resize: size must be positive");
        backend_->swapchain_on_resize.get_or_throw()(handle_, new_size);
        size_ = new_size;
    }

    void make_current(gfx_device &d) { tl_current_device = &d; }
    void make_current(window &w) { tl_current_window = &w; }
    gfx_device &current_device() {
        if (!tl_current_device)
            throw std::runtime_error("no current graphics device");
        return *tl_current_device;
    }
    vec2f current_pixel_center_offset() { return current_device().pixel_center_offset(); }
    window &current_window() {
        if (!tl_current_window)
            throw std::runtime_error("no current window");
        return *tl_current_window;
    }

    namespace detail {
        std::size_t register_vertex_definition(std::type_index vertex_type) {
            static std::mutex mutex;
            static std::unordered_map<std::type_index, std::size_t> indices;
            std::lock_guard lock(mutex);
            if (auto i = indices.find(vertex_type); i != indices.end())
                return i->second;
            const std::size_t index = indices.size();
            indices.emplace(vertex_type, index);
            return index;
        }
    } // namespace detail
} // namespace alia
