#ifndef GFX_DEVICE_DE00092E_67EA_4FA0_99D4_7315A97F7B9B
#define GFX_DEVICE_DE00092E_67EA_4FA0_99D4_7315A97F7B9B

#include "../core/color.hpp"
#include "../core/rect.hpp"
#include "../core/vec.hpp"
#include "pixel.hpp"
#include "vertex.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <typeindex>

namespace alia {

    class window;
    class texture;
    class swapchain;
    struct swapchain_impl;

    enum class gfx_backend {
        auto_,
        d3d9,
        opengl,
    };

    enum class texture_filter { nearest, linear };
    enum class texture_wrap { clamp, repeat, mirror };

    struct sampler_state {
        texture_filter min_filter = texture_filter::linear;
        texture_filter mag_filter = texture_filter::linear;
        texture_filter mip_filter = texture_filter::linear;
        texture_wrap wrap_u = texture_wrap::clamp;
        texture_wrap wrap_v = texture_wrap::clamp;
    };

    struct texture_lock_info {
        vec2i origin;
        vec2i extent;
        int stride_bytes = 0;
        int level = 0;
        std::byte *data = nullptr;
    };

    struct texture_impl {
        virtual ~texture_impl() = default;

        virtual pixel_format format() const noexcept = 0;
        virtual int width() const noexcept = 0;
        virtual int height() const noexcept = 0;
        virtual int mip_levels() const noexcept = 0;

        virtual sampler_state sampler() const noexcept = 0;
        virtual void set_sampler(const sampler_state &s) = 0;

        virtual bool lock(rect_i region, int level, texture_lock_info &out) = 0;
        virtual void unlock(const texture_lock_info &info, bool wrote) = 0;
        virtual void generate_mipmaps() = 0;
        virtual std::unique_ptr<texture_impl> clone() const = 0;
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
    };

    struct gfx_device_impl {
        virtual ~gfx_device_impl() = default;

        // Screen-space position bias needed for pixel-aligned UI rendering.
        [[nodiscard]] virtual vec2f pixel_center_offset() const noexcept = 0;

        virtual std::unique_ptr<texture_impl> create_texture(pixel_format fmt, vec2i size, int mip_levels) = 0;

        virtual std::unique_ptr<swapchain_impl> create_swapchain(void *native_handle, vec2i initial_size) = 0;
    };

    struct ogl_platform_ops {
        void *(*create_context)();
        void (*destroy_context)(void *ctx);
        void *(*create_surface)(void *native_handle, void *ctx);
        void (*destroy_surface)(void *native_handle, void *surface);
        void (*make_current)(void *surface, void *ctx);
        void (*swap_buffers)(void *surface);
        void (*make_none_current)();
    };

    void register_ogl_platform(ogl_platform_ops ops);
    const ogl_platform_ops &get_ogl_platform();

    struct gfx_draw_ops {
        void (*draw_prim)(prim_type type, const void *vertices, int count, int stride, std::type_index vtx_type, std::span<const vertex_element> elements);

        void (*draw_indexed_prim)(
            prim_type type, const void *vertices, int count, int stride, std::span<const uint32_t> indices, std::type_index vtx_type,
            std::span<const vertex_element> elements
        );

        void (*draw_textured_prim)(
            prim_type type, const void *vertices, int count, int stride, std::type_index vtx_type, std::span<const vertex_element> elements, texture_impl *tex
        );

        void (*draw_textured_indexed_prim)(
            prim_type type, const void *vertices, int count, int stride, std::span<const uint32_t> indices, std::type_index vtx_type,
            std::span<const vertex_element> elements, texture_impl *tex
        );
    };

    struct gfx_backend_entry {
        gfx_backend id;
        std::unique_ptr<gfx_device_impl> (*create_device)();
        gfx_draw_ops draw_ops;
    };

    void register_gfx_backend(gfx_backend_entry entry);

    struct swapchain_config {
        window &target;
    };

    class gfx_device {
    public:
        gfx_device() = default;
        ~gfx_device();
        gfx_device(gfx_device &&) noexcept;
        gfx_device &operator=(gfx_device &&) noexcept;
        gfx_device(const gfx_device &) = delete;
        gfx_device &operator=(const gfx_device &) = delete;

        static gfx_device create(gfx_backend pref = gfx_backend::auto_);
        [[nodiscard]] swapchain create_swapchain(const swapchain_config &config);

        [[nodiscard]] bool valid() const noexcept { return impl_ != nullptr; }
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

        gfx_device_impl *impl() const noexcept { return impl_.get(); }
        [[nodiscard]] vec2f pixel_center_offset() const;

    private:
        std::unique_ptr<gfx_device_impl> impl_;
        explicit gfx_device(std::unique_ptr<gfx_device_impl> impl) : impl_(std::move(impl)) {}
    };

    class swapchain {
    public:
        swapchain() = default;
        ~swapchain();
        swapchain(swapchain &&) noexcept;
        swapchain &operator=(swapchain &&) noexcept;
        swapchain(const swapchain &) = delete;
        swapchain &operator=(const swapchain &) = delete;

        [[nodiscard]] bool valid() const noexcept { return impl_ != nullptr; }
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

        swapchain_impl *impl() const noexcept { return impl_.get(); }

    private:
        friend class gfx_device;

        std::unique_ptr<swapchain_impl> impl_;
        explicit swapchain(std::unique_ptr<swapchain_impl> impl) : impl_(std::move(impl)) {}
    };

    inline thread_local gfx_device *tl_current_device = nullptr;
    inline thread_local swapchain *tl_current_swapchain = nullptr;
    inline thread_local window *tl_current_window = nullptr;

    void make_current(gfx_device &d);
    void make_current(swapchain &s);
    void make_current(window &w);
    gfx_device &current_device();
    [[nodiscard]] vec2f current_pixel_center_offset();
    swapchain &current_swapchain();
    window &current_window();

    void clear(color c);
    void present();
    void on_resize(vec2i new_size);

    void draw_prim(prim_type type, const void *vertices, int count, int stride, std::type_index vtx_type, std::span<const vertex_element> elements);

    void draw_indexed_prim(
        prim_type type, const void *vertices, int count, int stride, std::span<const uint32_t> indices, std::type_index vtx_type,
        std::span<const vertex_element> elements
    );

    void draw_textured_prim(
        prim_type type, const void *vertices, int count, int stride, std::type_index vtx_type, std::span<const vertex_element> elements, texture &tex
    );

    void draw_textured_indexed_prim(
        prim_type type, const void *vertices, int count, int stride, std::span<const uint32_t> indices, std::type_index vtx_type,
        std::span<const vertex_element> elements, texture &tex
    );

    void set_current_transform_matrix(std::span<const float, 16> m);
    void get_current_transform_matrix(std::span<float, 16> m);
    void set_current_projection_matrix(std::span<const float, 16> m);
    void get_current_projection_matrix(std::span<float, 16> m);

} // namespace alia

#endif /* GFX_DEVICE_DE00092E_67EA_4FA0_99D4_7315A97F7B9B */
