#ifndef GFX_DEVICE_EEFA16EE_8C09_453F_9EBB_D094DD1124EA
#define GFX_DEVICE_EEFA16EE_8C09_453F_9EBB_D094DD1124EA

#include "../core/vec.hpp"
#include "../core/rect.hpp"
#include "../core/color.hpp"
#include "vertex.hpp"
#include "pixel.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <typeindex>

namespace alia {

class window;
class texture;

// ── Backend selection ─────────────────────────────────────────────────

enum class gfx_backend {
    auto_,   // pick the first available
    d3d9,
    opengl,
};

// ── Texture sampler state ─────────────────────────────────────────────

enum class texture_filter { nearest, linear };
enum class texture_wrap   { clamp, repeat, mirror };

struct sampler_state {
    texture_filter min_filter = texture_filter::linear;
    texture_filter mag_filter = texture_filter::linear;
    texture_filter mip_filter = texture_filter::linear;
    texture_wrap   wrap_u     = texture_wrap::clamp;
    texture_wrap   wrap_v     = texture_wrap::clamp;
};

// ── Texture lock protocol (internal) ─────────────────────────────────
// Returned by texture_impl::lock(); passed back to texture_impl::unlock().

struct texture_lock_info {
    vec2i      origin;       // top-left of locked region within the mip level
    vec2i      extent;       // size of locked region in pixels
    int        level;        // mip level that was locked
    int        stride_bytes; // bytes per row in the mapped buffer
    std::byte* data;         // pointer to the locked region (backend-owned)
};

// ── texture_impl (internal) ───────────────────────────────────────────

struct texture_impl {
    virtual ~texture_impl() = default;

    virtual pixel_format  format()     const noexcept = 0;
    virtual int           width()      const noexcept = 0;
    virtual int           height()     const noexcept = 0;
    virtual int           mip_levels() const noexcept = 0;

    virtual sampler_state sampler()    const noexcept = 0;
    virtual void          set_sampler(const sampler_state& s) = 0;

    // Map a subregion of the given mip level into CPU-accessible memory.
    // Returns false if the format is unsupported, the level is out of range,
    // or the backend cannot map it (e.g. autogen level > 0 on D3D9).
    virtual bool lock(rect_i region, int level, texture_lock_info& out) = 0;

    // Release the lock. If wrote == true the backend re-uploads the region
    // (required for backends that stage through a CPU buffer, e.g. OpenGL).
    virtual void unlock(const texture_lock_info& info, bool wrote) = 0;

    // Fill mip levels 1+ from level 0.
    virtual void generate_mipmaps() = 0;

    // GPU-to-GPU deep copy.
    virtual std::unique_ptr<texture_impl> clone() const = 0;
};

// ── Implementation interfaces (internal) ─────────────────────────────

struct gfx_device_impl {
    virtual ~gfx_device_impl() = default;
    virtual const char* backend_name() const noexcept = 0;

    // Create an uninitialised GPU texture.
    // mip_levels == 1  → no mipmaps
    // mip_levels == 0  → full chain down to 1×1 (alia::full_mip_chain)
    // mip_levels >  1  → explicit level count
    // Returns nullptr if the format or size is unsupported by the backend.
    virtual std::unique_ptr<texture_impl> create_texture(
        pixel_format fmt, vec2i size, int mip_levels) = 0;
};

enum class prim_type {
    triangle_list,
    triangle_strip,
    triangle_fan,
};

struct swapchain_impl {
    virtual ~swapchain_impl() = default;
    virtual void clear(color c) = 0;
    virtual void present() = 0;
    virtual void on_resize(vec2i new_size) = 0;

    virtual void set_transform(std::span<const float, 16> m) = 0;
    virtual void get_transform(std::span<float, 16> m) const = 0;
    virtual void set_projection(std::span<const float, 16> m) = 0;
    virtual void get_projection(std::span<float, 16> m) const = 0;

    virtual void draw_prim(prim_type type,
                           const void* vertices, int count, int stride,
                           std::type_index vtx_type,
                           std::span<const vertex_element> elements) = 0;

    virtual void draw_indexed_prim(prim_type type,
                                   const void* vertices, int count, int stride,
                                   std::span<const uint32_t> indices,
                                   std::type_index vtx_type,
                                   std::span<const vertex_element> elements) = 0;

    virtual void draw_textured_prim(prim_type type,
                                    const void* vertices, int count, int stride,
                                    std::type_index vtx_type,
                                    std::span<const vertex_element> elements,
                                    texture_impl* tex) = 0;

    virtual void draw_textured_indexed_prim(prim_type type,
                                            const void* vertices, int count, int stride,
                                            std::span<const uint32_t> indices,
                                            std::type_index vtx_type,
                                            std::span<const vertex_element> elements,
                                            texture_impl* tex) = 0;
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
    void present();

    void draw_prim(prim_type type,
                   const void* vertices, int count, int stride,
                   std::type_index vtx_type,
                   std::span<const vertex_element> elements);

    void draw_indexed_prim(prim_type type,
                           const void* vertices, int count, int stride,
                           std::span<const uint32_t> indices,
                           std::type_index vtx_type,
                           std::span<const vertex_element> elements);

    void draw_textured_prim(prim_type type,
                            const void* vertices, int count, int stride,
                            std::type_index vtx_type,
                            std::span<const vertex_element> elements,
                            texture& tex);

    void draw_textured_indexed_prim(prim_type type,
                                    const void* vertices, int count, int stride,
                                    std::span<const uint32_t> indices,
                                    std::type_index vtx_type,
                                    std::span<const vertex_element> elements,
                                    texture& tex);

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

#endif /* GFX_DEVICE_EEFA16EE_8C09_453F_9EBB_D094DD1124EA */
