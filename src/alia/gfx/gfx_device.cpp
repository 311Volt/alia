#include "gfx_device.hpp"
#include "texture.hpp"
#include "../os/window.hpp"
#include <mutex>
#include <stdexcept>
#include <unordered_map>
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
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
namespace alia {
void register_win32_ogl_platform();
}
#endif
#endif

namespace alia {

void make_current(gfx_device& d) { tl_current_device = &d; }
void make_current(swapchain& s) { tl_current_swapchain = &s; }
void make_current(window& w) { tl_current_window = &w; }

gfx_device& current_device() {
    if (!tl_current_device)
        throw std::runtime_error("No current gfx_device");
    return *tl_current_device;
}

swapchain& current_swapchain() {
    if (!tl_current_swapchain)
        throw std::runtime_error("No current swapchain");
    return *tl_current_swapchain;
}

window& current_window() {
    if (!tl_current_window)
        throw std::runtime_error("No current window");
    return *tl_current_window;
}

static std::vector<gfx_backend_entry>& backend_registry() {
    static std::vector<gfx_backend_entry> reg;
    return reg;
}

static std::unordered_map<gfx_device_impl*, const gfx_draw_ops*>& device_draw_ops() {
    static std::unordered_map<gfx_device_impl*, const gfx_draw_ops*> ops;
    return ops;
}

static const gfx_draw_ops& current_draw_ops() {
    auto& device = current_device();
    auto it = device_draw_ops().find(device.impl());
    if (it == device_draw_ops().end())
        throw std::runtime_error("No draw operations registered for current gfx_device");
    return *it->second;
}

void register_gfx_backend(gfx_backend_entry entry) {
    backend_registry().push_back(std::move(entry));
}

static ogl_platform_ops s_ogl_platform_ops = {};

void register_ogl_platform(ogl_platform_ops ops) { s_ogl_platform_ops = ops; }

const ogl_platform_ops& get_ogl_platform() { return s_ogl_platform_ops; }

static void init_gfx_backends() {
    static std::once_flag flag;
    std::call_once(flag, []() {
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

gfx_device::~gfx_device() {
    if (impl_)
        device_draw_ops().erase(impl_.get());
    if (tl_current_device == this)
        tl_current_device = nullptr;
}

gfx_device::gfx_device(gfx_device&& other) noexcept {
    auto* old_impl = other.impl_.get();
    impl_ = std::move(other.impl_);
    if (impl_) {
        auto node = device_draw_ops().extract(old_impl);
        if (!node.empty()) {
            node.key() = impl_.get();
            device_draw_ops().insert(std::move(node));
        }
    }
    if (tl_current_device == &other)
        tl_current_device = this;
}

gfx_device& gfx_device::operator=(gfx_device&& other) noexcept {
    if (impl_)
        device_draw_ops().erase(impl_.get());
    auto* old_impl = other.impl_.get();
    impl_ = std::move(other.impl_);
    if (impl_) {
        auto node = device_draw_ops().extract(old_impl);
        if (!node.empty()) {
            node.key() = impl_.get();
            device_draw_ops().insert(std::move(node));
        }
    }
    if (tl_current_device == &other)
        tl_current_device = this;
    return *this;
}

gfx_device gfx_device::create(gfx_backend pref) {
    init_gfx_backends();

    if (pref != gfx_backend::auto_) {
        for (const auto& e : backend_registry()) {
            if (e.id != pref)
                continue;
            if (auto dev = e.create_device()) {
                gfx_device d(std::move(dev));
                device_draw_ops()[d.impl()] = &e.draw_ops;
                make_current(d);
                return d;
            }
        }
    }

    for (const auto& e : backend_registry()) {
        if (auto dev = e.create_device()) {
            gfx_device d(std::move(dev));
            device_draw_ops()[d.impl()] = &e.draw_ops;
            make_current(d);
            return d;
        }
    }

    throw std::runtime_error("gfx_device: no graphics backend available");
}

swapchain gfx_device::create_swapchain(const swapchain_config& config) {
    if (!valid())
        throw std::runtime_error("gfx_device::create_swapchain: device is not valid");

    if (auto sc = impl_->create_swapchain(config.target.native_handle(), config.target.size())) {
        swapchain s(std::move(sc));
        make_current(s);
        make_current(config.target);
        return s;
    }

    throw std::runtime_error("gfx_device::create_swapchain: failed to create swapchain");
}

swapchain::~swapchain() {
    if (tl_current_swapchain == this)
        tl_current_swapchain = nullptr;
}

swapchain::swapchain(swapchain&& other) noexcept : impl_(std::move(other.impl_)) {
    if (tl_current_swapchain == &other)
        tl_current_swapchain = this;
}

swapchain& swapchain::operator=(swapchain&& other) noexcept {
    impl_ = std::move(other.impl_);
    if (tl_current_swapchain == &other)
        tl_current_swapchain = this;
    return *this;
}

void clear(color c) {
    current_swapchain().impl()->clear(c);
}

void present() {
    current_swapchain().impl()->present();
}

void on_resize(vec2i new_size) {
    current_swapchain().impl()->on_resize(new_size);
}

void draw_prim(prim_type type,
               const void* vertices, int count, int stride,
               std::type_index vtx_type,
               std::span<const vertex_element> elements) {
    current_draw_ops().draw_prim(type, vertices, count, stride, vtx_type, elements);
}

void draw_indexed_prim(prim_type type,
                       const void* vertices, int count, int stride,
                       std::span<const uint32_t> indices,
                       std::type_index vtx_type,
                       std::span<const vertex_element> elements) {
    current_draw_ops().draw_indexed_prim(type, vertices, count, stride, indices, vtx_type, elements);
}

void draw_textured_prim(prim_type type,
                        const void* vertices, int count, int stride,
                        std::type_index vtx_type,
                        std::span<const vertex_element> elements,
                        texture& tex) {
    current_draw_ops().draw_textured_prim(type, vertices, count, stride, vtx_type, elements, tex.impl());
}

void draw_textured_indexed_prim(prim_type type,
                                const void* vertices, int count, int stride,
                                std::span<const uint32_t> indices,
                                std::type_index vtx_type,
                                std::span<const vertex_element> elements,
                                texture& tex) {
    current_draw_ops().draw_textured_indexed_prim(
        type, vertices, count, stride, indices, vtx_type, elements, tex.impl());
}

} // namespace alia
