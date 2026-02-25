#ifndef ALIA_GFX_GFX_DEVICE_HPP
#define ALIA_GFX_GFX_DEVICE_HPP

#include "../core/vec.hpp"
#include "../core/color.hpp"
#include "vertex.hpp"
#include <cstdint>
#include <memory>
#include <span>

namespace alia {

class window;

// ── Backend selection ─────────────────────────────────────────────────

enum class gfx_backend {
    auto_,   // pick the first available
    d3d9,
    opengl,
};

// ── Implementation interfaces (internal) ─────────────────────────────

struct gfx_device_impl {
    virtual ~gfx_device_impl() = default;
    virtual const char* backend_name() const noexcept = 0;
};

struct swapchain_impl {
    virtual ~swapchain_impl() = default;
    virtual void clear(color c) = 0;
    virtual void draw_triangle(colored_vertex v0, colored_vertex v1, colored_vertex v2) = 0;
    virtual void present() = 0;
    virtual void on_resize(vec2i new_size) = 0;

    virtual void set_transform(std::span<const float, 16> m) = 0;
    virtual void get_transform(std::span<float, 16> m) const = 0;
    virtual void set_projection(std::span<const float, 16> m) = 0;
    virtual void get_projection(std::span<float, 16> m) const = 0;

    virtual void draw_triangles(std::span<const colored_vertex> vertices) = 0;
    virtual void draw_triangle_strip(std::span<const colored_vertex> vertices) = 0;
    virtual void draw_triangle_fan(std::span<const colored_vertex> vertices) = 0;
    virtual void draw_triangles(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices) = 0;
    virtual void draw_triangle_strip(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices) = 0;
    virtual void draw_triangle_fan(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices) = 0;
};

// ── OpenGL platform ops (internal) ───────────────────────────────────
// Platform backends provide this to decouple backend_ogl.cpp from OS headers.

struct ogl_platform_ops {
    void* (*create_context)();                                      // → opaque ctx (HGLRC on Win32)
    void  (*destroy_context)(void* ctx);
    void* (*create_surface)(void* native_handle, void* ctx);        // → opaque surface (HDC on Win32)
    void  (*destroy_surface)(void* native_handle, void* surface);
    void  (*make_current)(void* surface, void* ctx);
    void  (*swap_buffers)(void* surface);
    void  (*make_none_current)();
};

void                    register_ogl_platform(ogl_platform_ops ops);
const ogl_platform_ops& get_ogl_platform();

// ── Backend registry ──────────────────────────────────────────────────

struct gfx_backend_entry {
    const char* name;
    gfx_backend id;
    // Returns nullptr on failure (backend unavailable)
    std::unique_ptr<gfx_device_impl> (*create_device)();
    std::unique_ptr<swapchain_impl>  (*create_swapchain)(
        gfx_device_impl& dev, void* native_handle, vec2i initial_size);
};

void register_gfx_backend(gfx_backend_entry entry);

// ── gfx_device ────────────────────────────────────────────────────────

class gfx_device {
public:
    gfx_device() = default;
    ~gfx_device();
    gfx_device(gfx_device&&) noexcept;
    gfx_device& operator=(gfx_device&&) noexcept;
    gfx_device(const gfx_device&) = delete;
    gfx_device& operator=(const gfx_device&) = delete;

    // Create a graphics device using the preferred backend (auto picks first available)
    static gfx_device create(gfx_backend pref = gfx_backend::auto_);

    [[nodiscard]] bool valid() const noexcept { return impl_ != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
    [[nodiscard]] const char* backend_name() const noexcept;

    gfx_device_impl* impl() const noexcept { return impl_.get(); }

private:
    std::unique_ptr<gfx_device_impl> impl_;
    explicit gfx_device(std::unique_ptr<gfx_device_impl> impl) : impl_(std::move(impl)) {}
};

// ── swapchain ─────────────────────────────────────────────────────────

class swapchain {
public:
    swapchain() = default;
    ~swapchain();
    swapchain(swapchain&&) noexcept;
    swapchain& operator=(swapchain&&) noexcept;
    swapchain(const swapchain&) = delete;
    swapchain& operator=(const swapchain&) = delete;

    // Bind a graphics device to a window
    static swapchain create(gfx_device& device, window& win);

    [[nodiscard]] bool valid() const noexcept { return impl_ != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

    // Per-frame API: clear → draw → present
    void clear(color c);
    void draw_triangle(colored_vertex v0, colored_vertex v1, colored_vertex v2);
    void draw_triangles(std::span<const colored_vertex> vertices);
    void draw_triangle_strip(std::span<const colored_vertex> vertices);
    void draw_triangle_fan(std::span<const colored_vertex> vertices);
    void draw_triangles(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices);
    void draw_triangle_strip(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices);
    void draw_triangle_fan(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices);
    void present();

    // Call when the window is resized
    void on_resize(vec2i new_size);

    void set_transform(std::span<const float, 16> m);
    void get_transform(std::span<float, 16> m) const;
    void set_projection(std::span<const float, 16> m);
    void get_projection(std::span<float, 16> m) const;

private:
    std::unique_ptr<swapchain_impl> impl_;
    explicit swapchain(std::unique_ptr<swapchain_impl> impl) : impl_(std::move(impl)) {}
};

// ── Context API ───────────────────────────────────────────────────────

inline thread_local gfx_device* tl_current_device = nullptr;
inline thread_local swapchain*  tl_current_swapchain = nullptr;
inline thread_local window*     tl_current_window = nullptr;

void        make_current(gfx_device& d);
void        make_current(swapchain& s);
void        make_current(window& w);
gfx_device& current_device();
swapchain&  current_swapchain();
window&     current_window();

void        on_resize(vec2i new_size);

} // namespace alia

#endif // ALIA_GFX_GFX_DEVICE_HPP
