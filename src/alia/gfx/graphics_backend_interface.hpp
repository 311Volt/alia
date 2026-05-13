#ifndef GRAPHICS_BACKEND_INTERFACE_B17C3B83_2C66_4D0F_908A_037719970A33
#define GRAPHICS_BACKEND_INTERFACE_B17C3B83_2C66_4D0F_908A_037719970A33

#include "../core/color.hpp"
#include "../core/rect.hpp"
#include "../core/vec.hpp"
#include "bitmap/pixel.hpp"
#include "vertex.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>

namespace alia {

    // ── Exception ─────────────────────────────────────────────────────────

    struct unsupported_operation_exception : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct shader_error : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // ── graphics_backend_operation ─────────────────────────────────────────

    template <class R, class... Args>
    struct gfx_backend_op;

    template <class R, class... Args>
    struct gfx_backend_op<R(Args...)> {
        R (*operation)(Args...) = nullptr;
        std::optional<std::string> reason_unsupported;

        [[nodiscard]] bool is_supported() const noexcept {
            return operation != nullptr;
        }

        auto get_or_throw() const -> R (*)(Args...) {
            if (operation)
                return operation;
            throw unsupported_operation_exception(reason_unsupported.value_or("operation not supported by this backend"));
        }
    };

    // ── Opaque handle base types ───────────────────────────────────────────
    // Each backend defines its own concrete struct that inherits from these.
    // Use static_cast to reach the concrete type inside backend code.

    struct device_handle {};
    struct texture_handle {};
    struct swapchain_handle {};
    struct shader_program_handle {};

    // ── Backend-agnostic enums and POD types ───────────────────────────────

    enum class gfx_backend { auto_, d3d9, opengl };

    enum class texture_role { color, alpha_mask };

    enum class texture_filter { nearest, linear };
    enum class texture_wrap { clamp, repeat, mirror };

    enum class shader_type { vertex, pixel };

    struct shader_source {
        gfx_backend backend = gfx_backend::auto_;
        shader_type type = shader_type::vertex;
        std::string_view source;
        std::string_view entry_point = "main";
        std::string_view profile;
        std::string_view debug_name;
    };

    struct shader_constant_binding {
        std::string_view name;
        shader_type stage = shader_type::vertex;
        int index = -1;
        int count = 1;
    };

    struct shader_sampler_binding {
        std::string_view name;
        shader_type stage = shader_type::pixel;
        int slot = -1;
    };

    struct shader_program_desc {
        std::span<const shader_source> sources;
        std::span<const shader_constant_binding> constant_bindings = {};
        std::span<const shader_sampler_binding> sampler_bindings = {};
    };

    enum class shader_constant_value_type {
        float_1,
        float_2,
        float_3,
        float_4,
        int_1,
        int_2,
        int_3,
        int_4,
        matrix_4x4,
    };

    struct shader_constant_payload {
        shader_constant_value_type type = shader_constant_value_type::float_1;
        std::span<const float> floats;
        std::span<const int> ints;
    };

    struct shader_constant_slot {
        bool valid = false;
        shader_type stage = shader_type::vertex;
        int location = -1;
        int count = 1;
    };

    struct shader_sampler_slot {
        bool valid = false;
        shader_type stage = shader_type::pixel;
        int location = -1;
        int slot = 0;
    };

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
        gfx_backend_op<void(device_handle *device)> destroy_device;

        // ── Texture ─────────────────────────────────────────────────────
        gfx_backend_op<texture_handle *(device_handle *device, pixel_format format, vec2i size, int mip_levels, texture_role role)>
            create_texture;
        gfx_backend_op<void(texture_handle *texture)> destroy_texture;
        gfx_backend_op<pixel_format(const texture_handle *texture)> texture_format;
        gfx_backend_op<int(const texture_handle *texture)> texture_width;
        gfx_backend_op<int(const texture_handle *texture)> texture_height;
        gfx_backend_op<int(const texture_handle *texture)> texture_mip_levels;
        gfx_backend_op<sampler_state(const texture_handle *texture)> texture_sampler;
        gfx_backend_op<void(texture_handle *texture, const sampler_state &sampler)> texture_set_sampler;
        gfx_backend_op<bool(texture_handle *texture, rect_i region, int level, texture_lock_info &out)> texture_lock;
        gfx_backend_op<void(texture_handle *texture, const texture_lock_info &info, bool wrote)> texture_unlock;
        gfx_backend_op<void(texture_handle *texture)> texture_generate_mipmaps;
        gfx_backend_op<texture_handle *(const texture_handle *texture)> texture_clone;

        gfx_backend_op<shader_program_handle *(device_handle *device, const shader_program_desc &desc)> create_shader_program;
        gfx_backend_op<void(shader_program_handle *program)> destroy_shader_program;
        gfx_backend_op<shader_constant_slot(shader_program_handle *program, std::string_view name, shader_type stage)>
            shader_lookup_constant;

        gfx_backend_op<void(shader_program_handle *program, const shader_constant_slot &slot, const shader_constant_payload &payload)>
            shader_set_constant;
			
        gfx_backend_op<shader_sampler_slot(shader_program_handle *program, std::string_view name, shader_type stage)> shader_lookup_sampler;
        gfx_backend_op<void(shader_program_handle *program, const shader_sampler_slot &slot, texture_handle *texture)> shader_set_sampler;

        // ── Swapchain ────────────────────────────────────────────────────
        gfx_backend_op<swapchain_handle *(device_handle *device, void *window, vec2i size)> create_swapchain;
        gfx_backend_op<void(swapchain_handle *swapchain)> destroy_swapchain;
        gfx_backend_op<void(swapchain_handle *swapchain, color clear_color)> swapchain_clear;
        gfx_backend_op<void(swapchain_handle *swapchain)> swapchain_present;
        gfx_backend_op<void(swapchain_handle *swapchain, vec2i size)> swapchain_on_resize;

        // ── Drawing ───────────────────────────────────────────────────────
        gfx_backend_op<void(
            prim_type type, const void *vertices, int count, int stride, std::type_index vtx_type, std::span<const vertex_element> elements,
            shader_program_handle *shader
        )>
            draw_prim;

        gfx_backend_op<void(
            prim_type type, const void *vertices, int count, int stride, std::span<const uint32_t> indices, std::type_index vtx_type,
            std::span<const vertex_element> elements, shader_program_handle *shader
        )>
            draw_indexed_prim;

        gfx_backend_op<void(
            prim_type type, const void *vertices, int count, int stride, std::type_index vtx_type, std::span<const vertex_element> elements,
            texture_handle *texture, shader_program_handle *shader
        )>
            draw_textured_prim;

        gfx_backend_op<void(
            prim_type type, const void *vertices, int count, int stride, std::type_index vtx_type, std::span<const vertex_element> elements,
            texture_handle *texture, shader_program_handle *shader
        )>
            draw_alpha_masked_prim;

        gfx_backend_op<void(
            prim_type type, const void *vertices, int count, int stride, std::span<const uint32_t> indices, std::type_index vtx_type,
            std::span<const vertex_element> elements, texture_handle *texture, shader_program_handle *shader
        )>
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

#endif /* GRAPHICS_BACKEND_INTERFACE_B17C3B83_2C66_4D0F_908A_037719970A33 */
