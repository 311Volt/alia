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
    struct render_target_scope_handle {};
    struct vertex_buffer_handle {};
    struct index_buffer_handle {};
    struct swapchain_handle {};
    struct shader_program_handle {};

    // ── Backend-agnostic enums and POD types ───────────────────────────────

    enum class gfx_backend { auto_, d3d9, opengl };

    enum class texture_role { color, alpha_mask };
    enum class texture_usage { sampling_only, render_target };

    enum class texture_filter { nearest, linear };
    enum class texture_wrap { clamp, repeat, mirror };

    enum class buffer_usage {
        static_mesh,
        dynamic_mesh,
    };

    enum class buffer_lock_mode {
        read_write,
        read_only,
        write_only,
    };

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

    struct buffer_lock_info {
        int offset_bytes = 0;
        int size_bytes = 0;
        std::byte *data = nullptr;
    };

    /// Access intent for @c texture_lock. Lets backends skip work the caller
    /// has promised it will not need.
    enum class texture_lock_mode {
        /// Read and write. Backend returns current pixel data; @c texture_unlock
        /// with @c wrote=true commits modifications.
        read_write,
        /// Read only. Backend returns current pixel data; the caller will not
        /// modify it and @c texture_unlock receives @c wrote=false.
        read_only,
        /// Write only. Backend may return uninitialised memory; the caller must
        /// overwrite every pixel in the locked region before unlock.
        write_only,
    };

    enum class prim_type {
        triangle_list,
        triangle_strip,
        triangle_fan,
    };

    enum class vertex_input_mode {
        fixed_function,
        shader_attributes,
    };

    enum class fixed_function_texture_mode {
        vertex_color,
        texture_replace,
        texture_modulate_vertex_color,
        alpha_mask,
    };

    enum class cull_mode {
        none,
        clockwise,
        counter_clockwise,
    };

    enum class blend_factor {
        zero,
        one,
        src_alpha,
        inv_src_alpha,
    };

    enum class blend_op {
        add,
    };

    struct render_viewport {
        vec2i origin = {};
        vec2i size = {};
        float min_depth = 0.0f;
        float max_depth = 1.0f;
    };

    struct render_state {
        bool depth_test_enabled = false;
        bool depth_write_enabled = false;
        bool fixed_function_lighting_enabled = false;
        cull_mode cull = cull_mode::none;
    };

    struct blend_state {
        bool enabled = false;
        blend_factor src = blend_factor::src_alpha;
        blend_factor dst = blend_factor::inv_src_alpha;
        blend_op op = blend_op::add;
    };

    struct fixed_function_matrices {
        std::span<const float, 16> world;
        std::span<const float, 16> projection;
    };

    struct texture_binding {
        int slot = 0;
        texture_handle *texture = nullptr;
    };

    struct texture_sampler_binding {
        int slot = 0;
        texture_handle *texture = nullptr;
        sampler_state sampler = {};
    };

    struct vertex_definition_view {
        /// Process-wide vertex-type slot used to index each device's table of
        /// backend-compiled vertex definitions.
        std::size_t index = 0;
        int stride = 0;
        std::span<const vertex_element> elements;
    };

    struct vertex_input_binding {
        vertex_input_mode mode = vertex_input_mode::fixed_function;
        vertex_buffer_handle *buffer = nullptr;
        const void *vertices = nullptr;
        int vertex_offset_bytes = 0;
        vertex_definition_view definition;
    };

    struct draw_arrays_immediate {
        prim_type type = prim_type::triangle_list;
        const void *vertices = nullptr;
        int vertex_count = 0;
        int vertex_stride = 0;
        int primitive_count = 0;
    };

    struct draw_elements_immediate {
        prim_type type = prim_type::triangle_list;
        const void *vertices = nullptr;
        int vertex_count = 0;
        int vertex_stride = 0;
        std::span<const uint32_t> indices;
        int primitive_count = 0;
    };

    struct draw_arrays_buffered {
        prim_type type = prim_type::triangle_list;
        vertex_buffer_handle *vertices = nullptr;
        int first_vertex = 0;
        int vertex_count = 0;
        int vertex_stride = 0;
        int primitive_count = 0;
    };

    struct draw_elements_buffered {
        prim_type type = prim_type::triangle_list;
        vertex_buffer_handle *vertices = nullptr;
        int vertex_count = 0;
        int vertex_stride = 0;
        index_buffer_handle *indices = nullptr;
        int first_index = 0;
        int index_count = 0;
        int primitive_count = 0;
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
        gfx_backend_op<texture_handle *(
            device_handle *device,
            pixel_format format,
            vec2i size,
            int mip_levels,
            texture_role role,
            texture_usage usage
        )> create_texture;
        gfx_backend_op<void(texture_handle *texture)> destroy_texture;
        gfx_backend_op<pixel_format(const texture_handle *texture)> texture_format;
        gfx_backend_op<int(const texture_handle *texture)> texture_width;
        gfx_backend_op<int(const texture_handle *texture)> texture_height;
        gfx_backend_op<int(const texture_handle *texture)> texture_mip_levels;
        gfx_backend_op<sampler_state(const texture_handle *texture)> texture_sampler;
        gfx_backend_op<void(texture_handle *texture, const sampler_state &sampler)> texture_set_sampler;
        gfx_backend_op<bool(texture_handle *texture, rect_i region, int level, texture_lock_mode mode, texture_lock_info &out)> texture_lock;
        gfx_backend_op<void(texture_handle *texture, const texture_lock_info &info, bool wrote)> texture_unlock;
        gfx_backend_op<void(texture_handle *texture)> texture_generate_mipmaps;
        gfx_backend_op<texture_handle *(const texture_handle *texture)> texture_clone;
        gfx_backend_op<render_target_scope_handle *(device_handle *device, texture_handle *texture, int level)> texture_begin_render_target;
        gfx_backend_op<void(device_handle *device, render_target_scope_handle *scope)> texture_end_render_target;
        gfx_backend_op<bool(device_handle *device, texture_handle *dst, rect_i src_rect, vec2i src_target_size, vec2i dst_pos, int dst_level)>
            copy_render_target_to_texture;

        // -- Primitive buffers ------------------------------------------------
        gfx_backend_op<vertex_buffer_handle *(
            device_handle *device,
            int vertex_stride,
            int vertex_count,
            buffer_usage usage,
            const void *initial_data
        )> create_vertex_buffer;
        gfx_backend_op<void(vertex_buffer_handle *buffer)> destroy_vertex_buffer;
        gfx_backend_op<int(const vertex_buffer_handle *buffer)> vertex_buffer_count;
        gfx_backend_op<int(const vertex_buffer_handle *buffer)> vertex_buffer_stride;
        gfx_backend_op<buffer_usage(const vertex_buffer_handle *buffer)> vertex_buffer_usage;
        gfx_backend_op<bool(vertex_buffer_handle *buffer, int first_vertex, int vertex_count, buffer_lock_mode mode, buffer_lock_info &out)>
            vertex_buffer_lock;
        gfx_backend_op<void(vertex_buffer_handle *buffer, const buffer_lock_info &info, bool wrote)> vertex_buffer_unlock;

        gfx_backend_op<index_buffer_handle *(
            device_handle *device,
            int index_count,
            buffer_usage usage,
            const uint32_t *initial_data
        )> create_index_buffer;
        gfx_backend_op<void(index_buffer_handle *buffer)> destroy_index_buffer;
        gfx_backend_op<int(const index_buffer_handle *buffer)> index_buffer_count;
        gfx_backend_op<buffer_usage(const index_buffer_handle *buffer)> index_buffer_usage;
        gfx_backend_op<bool(index_buffer_handle *buffer, int first_index, int index_count, buffer_lock_mode mode, buffer_lock_info &out)>
            index_buffer_lock;
        gfx_backend_op<void(index_buffer_handle *buffer, const buffer_lock_info &info, bool wrote)> index_buffer_unlock;

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
        gfx_backend_op<void(swapchain_handle *swapchain)> swapchain_begin_frame;
        gfx_backend_op<void(swapchain_handle *swapchain)> swapchain_end_frame;
        gfx_backend_op<void(swapchain_handle *swapchain)> swapchain_present;
        gfx_backend_op<void(swapchain_handle *swapchain, vec2i size)> swapchain_on_resize;

        // ── Drawing ───────────────────────────────────────────────────────
        gfx_backend_op<void(device_handle *device, const render_viewport &viewport)> set_viewport;
        gfx_backend_op<void(device_handle *device, color clear_color)> clear_render_target;
        gfx_backend_op<void(device_handle *device, const render_state &state)> set_render_state;
        gfx_backend_op<void(device_handle *device, const blend_state &state)> set_blend_state;

        gfx_backend_op<void(device_handle *device, shader_program_handle *program)> bind_shader_program;
        gfx_backend_op<void(device_handle *device, shader_program_handle *program)> apply_shader_constants;
        gfx_backend_op<void(device_handle *device, shader_program_handle *program)> apply_shader_samplers;

        gfx_backend_op<void(device_handle *device, const fixed_function_matrices &matrices)> set_fixed_function_matrices;
        gfx_backend_op<void(device_handle *device, fixed_function_texture_mode mode)> set_fixed_function_texture_mode;

        gfx_backend_op<void(device_handle *device, const texture_binding &binding)> bind_texture;
        gfx_backend_op<void(device_handle *device, const texture_sampler_binding &binding)> set_texture_sampler;

        gfx_backend_op<void(device_handle *device, const vertex_input_binding &binding)> bind_vertex_input;
        gfx_backend_op<void(device_handle *device, const vertex_input_binding &binding)> unbind_vertex_input;
        gfx_backend_op<void(device_handle *device, const draw_arrays_immediate &draw)> draw_arrays_immediate;
        gfx_backend_op<void(device_handle *device, const draw_elements_immediate &draw)> draw_elements_immediate;
        gfx_backend_op<void(device_handle *device, const draw_arrays_buffered &draw)> draw_arrays_buffered;
        gfx_backend_op<void(device_handle *device, const draw_elements_buffered &draw)> draw_elements_buffered;
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
