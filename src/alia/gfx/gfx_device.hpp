#ifndef ALIA_GFX_GFX_DEVICE_HPP
#define ALIA_GFX_GFX_DEVICE_HPP

#include "../core/vec.hpp"
#include "../core/color.hpp"
#include <memory>

namespace alia {

class window;

// ── Vertex type ───────────────────────────────────────────────────────

struct colored_vertex {
    vec2f position;  // NDC coordinates: x,y in [-1, 1]
    color col;       // RGB(A) color
};

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
};

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
    ~gfx_device() = default;
    gfx_device(gfx_device&&) noexcept = default;
    gfx_device& operator=(gfx_device&&) noexcept = default;
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
    ~swapchain() = default;
    swapchain(swapchain&&) noexcept = default;
    swapchain& operator=(swapchain&&) noexcept = default;
    swapchain(const swapchain&) = delete;
    swapchain& operator=(const swapchain&) = delete;

    // Bind a graphics device to a window
    static swapchain create(gfx_device& device, window& win);

    [[nodiscard]] bool valid() const noexcept { return impl_ != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

    // Per-frame API: clear → draw → present
    void clear(color c);
    void draw_triangle(colored_vertex v0, colored_vertex v1, colored_vertex v2);
    void present();

    // Call when the window is resized
    void on_resize(vec2i new_size);

private:
    std::unique_ptr<swapchain_impl> impl_;
    explicit swapchain(std::unique_ptr<swapchain_impl> impl) : impl_(std::move(impl)) {}
};

} // namespace alia

#endif // ALIA_GFX_GFX_DEVICE_HPP
