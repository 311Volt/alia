#include "gfx_device.hpp"
#include "../os/window.hpp"
#include <vector>
#include <string_view>
#include <stdexcept>
#include <mutex>

// Forward declarations for each compiled-in backend
#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9
namespace alia { void register_d3d9_backend(); }
#endif
#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL
namespace alia { void register_ogl_backend(); }
#endif

namespace alia {

// ── Backend registry ──────────────────────────────────────────────────

static std::vector<gfx_backend_entry>& backend_registry() {
    // Meyers singleton: safe to call from static init of other TUs
    static std::vector<gfx_backend_entry> reg;
    return reg;
}

void register_gfx_backend(gfx_backend_entry entry) {
    backend_registry().push_back(std::move(entry));
}

// ── Backend initialization ─────────────────────────────────────────────

static void init_gfx_backends() {
    static std::once_flag flag;
    std::call_once(flag, []() {
#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9
        register_d3d9_backend();
#endif
#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL
        register_ogl_backend();
#endif
    });
}

// ── gfx_device ────────────────────────────────────────────────────────

gfx_device gfx_device::create(gfx_backend pref) {
    init_gfx_backends();
    const auto& reg = backend_registry();

    // Try explicitly requested backend first
    if (pref != gfx_backend::auto_) {
        for (const auto& e : reg) {
            if (e.id == pref) {
                if (auto dev = e.create_device())
                    return gfx_device(std::move(dev));
            }
        }
    }

    // Fall through: try all backends in registration order
    for (const auto& e : reg) {
        if (auto dev = e.create_device())
            return gfx_device(std::move(dev));
    }

    throw std::runtime_error("gfx_device: no graphics backend available");
}

const char* gfx_device::backend_name() const noexcept {
    return impl_ ? impl_->backend_name() : "none";
}

// ── swapchain ─────────────────────────────────────────────────────────

swapchain swapchain::create(gfx_device& device, window& win) {
    if (!device.valid())
        throw std::runtime_error("swapchain: gfx_device is not valid");

    const auto* dev_name = device.backend_name();
    void* native = win.native_handle();
    vec2i size = win.size();

    for (const auto& e : backend_registry()) {
        if (std::string_view(e.name) == dev_name) {
            if (auto sc = e.create_swapchain(*device.impl(), native, size))
                return swapchain(std::move(sc));
        }
    }

    throw std::runtime_error("swapchain: failed to create swapchain");
}

void swapchain::clear(color c)                                              { if (impl_) impl_->clear(c); }
void swapchain::draw_triangle(colored_vertex v0, colored_vertex v1, colored_vertex v2) { if (impl_) impl_->draw_triangle(v0, v1, v2); }
void swapchain::present()                                                   { if (impl_) impl_->present(); }
void swapchain::on_resize(vec2i new_size)                                   { if (impl_) impl_->on_resize(new_size); }

} // namespace alia
