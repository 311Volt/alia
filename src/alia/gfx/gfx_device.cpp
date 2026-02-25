#include "gfx_device.hpp"
#include "../os/window.hpp"
#include <vector>
#include <string_view>
#include <stdexcept>
#include <mutex>
#include <span>

// Forward declarations for each compiled-in backend
#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9
namespace alia {
    void register_d3d9_backend();
}
#endif
#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL
namespace alia {
    void register_ogl_backend();
}
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
namespace alia {
    void register_win32_ogl_platform();
}
#endif
#endif

namespace alia {

    // ── Context API ───────────────────────────────────────────────────────

    void make_current(gfx_device &d) { tl_current_device = &d; }
    void make_current(swapchain &s) { tl_current_swapchain = &s; }
    void make_current(window &w) { tl_current_window = &w; }

    gfx_device &current_device() {
        if (!tl_current_device)
            throw std::runtime_error("No current gfx_device");
        return *tl_current_device;
    }

    swapchain &current_swapchain() {
        if (!tl_current_swapchain)
            throw std::runtime_error("No current swapchain");
        return *tl_current_swapchain;
    }

    window &current_window() {
        if (!tl_current_window)
            throw std::runtime_error("No current window");
        return *tl_current_window;
    }

    void on_resize(vec2i new_size) { current_swapchain().on_resize(new_size); }

    // ── Backend registry ──────────────────────────────────────────────────

    static std::vector<gfx_backend_entry> &backend_registry() {
        // Meyers singleton: safe to call from static init of other TUs
        static std::vector<gfx_backend_entry> reg;
        return reg;
    }

    void register_gfx_backend(gfx_backend_entry entry) { backend_registry().push_back(std::move(entry)); }

    // ── OpenGL platform ops registry ──────────────────────────────────────

    static ogl_platform_ops s_ogl_platform_ops = {};

    void register_ogl_platform(ogl_platform_ops ops) { s_ogl_platform_ops = ops; }

    const ogl_platform_ops &get_ogl_platform() { return s_ogl_platform_ops; }

    // ── Backend initialization ─────────────────────────────────────────────

    static void init_gfx_backends() {
        static std::once_flag flag;
        std::call_once(flag, []() {
        // Register platform-specific OpenGL support before the backend
#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
            register_win32_ogl_platform();
#endif
#endif
#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9
            register_d3d9_backend();
#endif
#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL
            register_ogl_backend();
#endif
        });
    }

    // ── gfx_device ────────────────────────────────────────────────────────

    gfx_device::~gfx_device() {
        if (tl_current_device == this)
            tl_current_device = nullptr;
    }

    gfx_device::gfx_device(gfx_device &&other) noexcept : impl_(std::move(other.impl_)) {
        if (tl_current_device == &other)
            tl_current_device = this;
    }

    gfx_device &gfx_device::operator=(gfx_device &&other) noexcept {
        impl_ = std::move(other.impl_);
        if (tl_current_device == &other)
            tl_current_device = this;
        return *this;
    }

    gfx_device gfx_device::create(gfx_backend pref) {
        init_gfx_backends();
        const auto &reg = backend_registry();

        // Try explicitly requested backend first
        if (pref != gfx_backend::auto_) {
            for (const auto &e : reg) {
                if (e.id == pref) {
                    if (auto dev = e.create_device()) {
                        gfx_device d(std::move(dev));
                        make_current(d);
                        return d;
                    }
                }
            }
        }

        // Fall through: try all backends in registration order
        for (const auto &e : reg) {
            if (auto dev = e.create_device()) {
                gfx_device d(std::move(dev));
                make_current(d);
                return d;
            }
        }

        throw std::runtime_error("gfx_device: no graphics backend available");
    }

    const char *gfx_device::backend_name() const noexcept { return impl_ ? impl_->backend_name() : "none"; }

    // ── swapchain ─────────────────────────────────────────────────────────

    swapchain::~swapchain() {
        if (tl_current_swapchain == this)
            tl_current_swapchain = nullptr;
    }

    swapchain::swapchain(swapchain &&other) noexcept : impl_(std::move(other.impl_)) {
        if (tl_current_swapchain == &other)
            tl_current_swapchain = this;
    }

    swapchain &swapchain::operator=(swapchain &&other) noexcept {
        impl_ = std::move(other.impl_);
        if (tl_current_swapchain == &other)
            tl_current_swapchain = this;
        return *this;
    }

    swapchain swapchain::create(gfx_device &device, window &win) {
        if (!device.valid())
            throw std::runtime_error("swapchain: gfx_device is not valid");

        const auto *dev_name = device.backend_name();
        void *native = win.native_handle();
        vec2i size = win.size();

        for (const auto &e : backend_registry()) {
            if (std::string_view(e.name) == dev_name) {
                if (auto sc = e.create_swapchain(*device.impl(), native, size)) {
                    swapchain s(std::move(sc));
                    make_current(s);
                    return s;
                }
            }
        }

        throw std::runtime_error("swapchain: failed to create swapchain");
    }

    void swapchain::clear(color c) {
        if (impl_)
            impl_->clear(c);
    }
    void swapchain::draw_triangle(colored_vertex v0, colored_vertex v1, colored_vertex v2) {
        if (impl_)
            impl_->draw_triangle(v0, v1, v2);
    }
    void swapchain::present() {
        if (impl_)
            impl_->present();
    }
    void swapchain::on_resize(vec2i new_size) {
        if (impl_)
            impl_->on_resize(new_size);
    }

    void swapchain::set_transform(std::span<const float, 16> m) {
        if (impl_)
            impl_->set_transform(m);
    }
    void swapchain::get_transform(std::span<float, 16> m) const {
        if (impl_)
            impl_->get_transform(m);
    }
    void swapchain::set_projection(std::span<const float, 16> m) {
        if (impl_)
            impl_->set_projection(m);
    }
    void swapchain::get_projection(std::span<float, 16> m) const {
        if (impl_)
            impl_->get_projection(m);
    }

    void swapchain::draw_triangles(std::span<const colored_vertex> v) {
        if (impl_)
            impl_->draw_triangles(v);
    }
    void swapchain::draw_triangle_strip(std::span<const colored_vertex> v) {
        if (impl_)
            impl_->draw_triangle_strip(v);
    }
    void swapchain::draw_triangle_fan(std::span<const colored_vertex> v) {
        if (impl_)
            impl_->draw_triangle_fan(v);
    }
    void swapchain::draw_triangles(std::span<const colored_vertex> v, std::span<const uint32_t> idx) {
        if (impl_)
            impl_->draw_triangles(v, idx);
    }
    void swapchain::draw_triangle_strip(std::span<const colored_vertex> v, std::span<const uint32_t> idx) {
        if (impl_)
            impl_->draw_triangle_strip(v, idx);
    }
    void swapchain::draw_triangle_fan(std::span<const colored_vertex> v, std::span<const uint32_t> idx) {
        if (impl_)
            impl_->draw_triangle_fan(v, idx);
    }

} // namespace alia
