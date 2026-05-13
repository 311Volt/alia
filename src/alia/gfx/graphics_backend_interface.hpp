#ifndef GRAPHICS_BACKEND_OPS_A6ED0D03_4A82_48C9_99BB_A6C8BFA288C4
#define GRAPHICS_BACKEND_OPS_A6ED0D03_4A82_48C9_99BB_A6C8BFA288C4

#include "../core/color.hpp"
#include "../core/rect.hpp"
#include "../core/vec.hpp"
#include "pixel.hpp"
#include "vertex.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <typeindex>

namespace alia {

    // ── Exception ─────────────────────────────────────────────────────────

    struct unsupported_operation_exception : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // ── graphics_backend_operation ─────────────────────────────────────────

    template <class R, class... Args>
    struct graphics_backend_operation;

    template <class R, class... Args>
    struct graphics_backend_operation<R(Args...)> {
        R (*operation)(Args...) = nullptr;
        std::optional<std::string> reason_unsupported;

        [[nodiscard]] bool is_supported() const noexcept {
            return operation != nullptr;
        }

        auto get_or_throw() const -> R (*)(Args...) {
            if (operation)
                return operation;
            throw unsupported_operation_exception(
                reason_unsupported.value_or("operation not supported by this backend")
            );
        }
    };

    // ── Opaque handle base types ───────────────────────────────────────────
    // Each backend defines its own concrete struct that inherits from these.
    // Use static_cast to reach the concrete type inside backend code.

    struct device_handle {};
    struct texture_handle {};
    struct swapchain_handle {};

    // ── Backend-agnostic enums and POD types ───────────────────────────────

    enum class gfx_backend { auto_, d3d9, opengl };

    enum class texture_role { color, alpha_mask };

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

    enum class prim_type {
        triangle_list,
        triangle_strip,
        triangle_fan,
    };

    // ── graphics_backend_interface ─────────────────────────────────────────
    // One instance per gfx_device; ops may be null with reason_unsupported
    // set when the underlying hardware does not support them.

    struct graphics_backend_interface {
        gfx_backend id = gfx_backend::auto_;
        vec2f pixel_center_offset = {};

        // ── Device ──────────────────────────────────────────────────────
        graphics_backend_operation<void(device_handle *)> destroy_device;

        // ── Texture ─────────────────────────────────────────────────────
        graphics_backend_operation<texture_handle *(device_handle *, pixel_format, vec2i, int, texture_role)>
            create_texture;
        graphics_backend_operation<void(texture_handle *)>
            destroy_texture;
        graphics_backend_operation<pixel_format(const texture_handle *)>
            texture_format;
        graphics_backend_operation<int(const texture_handle *)>
            texture_width;
        graphics_backend_operation<int(const texture_handle *)>
            texture_height;
        graphics_backend_operation<int(const texture_handle *)>
            texture_mip_levels;
        graphics_backend_operation<sampler_state(const texture_handle *)>
            texture_sampler;
        graphics_backend_operation<void(texture_handle *, const sampler_state &)>
            texture_set_sampler;
        graphics_backend_operation<bool(texture_handle *, rect_i, int, texture_lock_info &)>
            texture_lock;
        graphics_backend_operation<void(texture_handle *, const texture_lock_info &, bool)>
            texture_unlock;
        graphics_backend_operation<void(texture_handle *)>
            texture_generate_mipmaps;
        graphics_backend_operation<texture_handle *(const texture_handle *)>
            texture_clone;

        // ── Swapchain ────────────────────────────────────────────────────
        graphics_backend_operation<swapchain_handle *(device_handle *, void *, vec2i)>
            create_swapchain;
        graphics_backend_operation<void(swapchain_handle *)>
            destroy_swapchain;
        graphics_backend_operation<void(swapchain_handle *, color)>
            swapchain_clear;
        graphics_backend_operation<void(swapchain_handle *)>
            swapchain_present;
        graphics_backend_operation<void(swapchain_handle *, vec2i)>
            swapchain_on_resize;

        // ── Drawing ───────────────────────────────────────────────────────
        graphics_backend_operation<
            void(prim_type, const void *, int, int, std::type_index, std::span<const vertex_element>)>
            draw_prim;
        graphics_backend_operation<
            void(prim_type, const void *, int, int, std::span<const uint32_t>, std::type_index, std::span<const vertex_element>)>
            draw_indexed_prim;
        graphics_backend_operation<
            void(prim_type, const void *, int, int, std::type_index, std::span<const vertex_element>, texture_handle *)>
            draw_textured_prim;
        graphics_backend_operation<
            void(prim_type, const void *, int, int, std::type_index, std::span<const vertex_element>, texture_handle *)>
            draw_alpha_masked_prim;
        graphics_backend_operation<
            void(prim_type, const void *, int, int, std::span<const uint32_t>, std::type_index, std::span<const vertex_element>, texture_handle *)>
            draw_textured_indexed_prim;
    };

    // ── Registration ─────────────────────────────────────────────────────

    struct created_device {
        device_handle *handle = nullptr;
        graphics_backend_interface iface;
    };

    struct gfx_backend_factory {
        gfx_backend id;
        created_device (*create)();
    };

    void register_gfx_backend(gfx_backend_factory factory);

} // namespace alia

#endif /* GRAPHICS_BACKEND_OPS_A6ED0D03_4A82_48C9_99BB_A6C8BFA288C4 */
