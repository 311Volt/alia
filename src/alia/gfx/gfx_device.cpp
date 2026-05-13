#include "gfx_device.hpp"
#include "texture.hpp"
#include "../os/window.hpp"
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9
namespace alia {
    void register_d3d9_backend();
}
#endif

#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL
namespace alia {
    void register_ogl_backend();
}
#endif

namespace alia {

    // ── Backend registry ──────────────────────────────────────────────────

    static std::vector<gfx_backend_factory> &backend_registry() {
        static std::vector<gfx_backend_factory> reg;
        return reg;
    }

    void register_gfx_backend(gfx_backend_factory factory) {
        backend_registry().push_back(factory);
    }

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

    // ── Thread-local current-object setters ───────────────────────────────

    void make_current(gfx_device &d) {
        tl_current_device = &d;
    }
    void make_current(swapchain &s) {
        tl_current_swapchain = &s;
    }
    void make_current(window &w) {
        tl_current_window = &w;
    }

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

    // ── gfx_device ────────────────────────────────────────────────────────

    gfx_device::~gfx_device() {
        if (device_)
            backend_->destroy_device.get_or_throw()(device_);
        if (tl_current_device == this)
            tl_current_device = nullptr;
    }

    gfx_device::gfx_device(gfx_device &&other) noexcept
        : device_(std::exchange(other.device_, nullptr))
        , backend_(std::move(other.backend_)) {
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

    vec2f gfx_device::pixel_center_offset() const {
        if (!valid())
            throw std::runtime_error("gfx_device::pixel_center_offset: device is not valid");
        return backend_->pixel_center_offset;
    }

    gfx_device gfx_device::create(gfx_backend pref) {
        init_gfx_backends();

        auto try_factory = [](const gfx_backend_factory &f) -> gfx_device {
            auto [handle, iface] = f.create();
            if (!handle)
                return {};
            auto backend = std::make_unique<graphics_backend_interface>(std::move(iface));
            gfx_device d(handle, std::move(backend));
            make_current(d);
            return d;
        };

        if (pref != gfx_backend::auto_) {
            for (const auto &f : backend_registry()) {
                if (f.id != pref)
                    continue;
                if (auto d = try_factory(f); d.valid())
                    return d;
            }
        }

        for (const auto &f : backend_registry()) {
            if (auto d = try_factory(f); d.valid())
                return d;
        }

        throw std::runtime_error("gfx_device: no graphics backend available");
    }

    vec2f current_pixel_center_offset() {
        return current_device().pixel_center_offset();
    }

    swapchain gfx_device::create_swapchain(const swapchain_config &config) {
        if (!valid())
            throw std::runtime_error("gfx_device::create_swapchain: device is not valid");

        swapchain_handle *sc = backend_->create_swapchain.get_or_throw()(
            device_, config.target.native_handle(), config.target.size()
        );
        if (!sc)
            throw std::runtime_error("gfx_device::create_swapchain: failed to create swapchain");

        swapchain s(sc, backend_.get());
        make_current(s);
        make_current(config.target);
        return s;
    }

    // ── swapchain ─────────────────────────────────────────────────────────

    swapchain::~swapchain() {
        if (handle_)
            backend_->destroy_swapchain.get_or_throw()(handle_);
        if (tl_current_swapchain == this)
            tl_current_swapchain = nullptr;
    }

    swapchain::swapchain(swapchain &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr))
        , backend_(std::exchange(other.backend_, nullptr)) {
        if (tl_current_swapchain == &other)
            tl_current_swapchain = this;
    }

    swapchain &swapchain::operator=(swapchain &&other) noexcept {
        if (this != &other) {
            if (handle_)
                backend_->destroy_swapchain.get_or_throw()(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
            backend_ = std::exchange(other.backend_, nullptr);
            if (tl_current_swapchain == &other)
                tl_current_swapchain = this;
        }
        return *this;
    }

    // ── Swapchain free functions ───────────────────────────────────────────

    void clear(color c) {
        auto &sc = current_swapchain();
        sc.backend()->swapchain_clear.get_or_throw()(sc.handle(), c);
    }

    void present() {
        auto &sc = current_swapchain();
        sc.backend()->swapchain_present.get_or_throw()(sc.handle());
    }

    void on_resize(vec2i new_size) {
        auto &sc = current_swapchain();
        sc.backend()->swapchain_on_resize.get_or_throw()(sc.handle(), new_size);
    }

    // ── Draw free functions ───────────────────────────────────────────────

    void draw_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::type_index vtx_type, std::span<const vertex_element> elements
    ) {
        current_device().backend()->draw_prim.get_or_throw()(type, vertices, count, stride, vtx_type, elements);
    }

    void draw_indexed_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::span<const uint32_t> indices,
        std::type_index vtx_type, std::span<const vertex_element> elements
    ) {
        current_device().backend()->draw_indexed_prim.get_or_throw()(type, vertices, count, stride, indices, vtx_type, elements);
    }

    void draw_textured_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        texture &tex
    ) {
        current_device().backend()->draw_textured_prim.get_or_throw()(type, vertices, count, stride, vtx_type, elements, tex.impl());
    }

    void draw_textured_indexed_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::span<const uint32_t> indices,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        texture &tex
    ) {
        current_device().backend()->draw_textured_indexed_prim.get_or_throw()(type, vertices, count, stride, indices, vtx_type, elements, tex.impl());
    }

} // namespace alia
