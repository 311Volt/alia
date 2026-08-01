#ifndef GRAPHICS_BACKEND_INTERFACE_B17C3B83_2C66_4D0F_908A_037719970A33
#define GRAPHICS_BACKEND_INTERFACE_B17C3B83_2C66_4D0F_908A_037719970A33

#include "../core/color.hpp"
#include "../core/rect.hpp"
#include "../core/vec.hpp"
#include "bitmap/pixel.hpp"
#include "transform.hpp"
#include "vertex.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace alia {

    struct unsupported_operation_exception : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct shader_error : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    template <class R, class... Args>
    struct gfx_backend_op;

    template <class R, class... Args>
    struct gfx_backend_op<R(Args...)> {
        R (*operation)(Args...) = nullptr;
        std::optional<std::string> reason_unsupported;

        [[nodiscard]] bool is_supported() const noexcept { return operation != nullptr; }

        auto get_or_throw() const -> R (*)(Args...) {
            if (operation)
                return operation;
            throw unsupported_operation_exception(
                reason_unsupported.value_or("operation not supported by this backend"));
        }
    };

    // Opaque bases. Concrete backend handles inherit these types and are only
    // downcast inside their own backend implementation files.
    struct device_handle {};
    struct texture_handle {};
    struct vertex_buffer_handle {};
    struct index_buffer_handle {};
    struct swapchain_handle {};
    struct shader_program_handle {};
    struct pipeline_handle {};

    enum class gfx_backend { auto_, d3d9, opengl };
    enum class texture_role { color, alpha_mask };
    enum class texture_usage { sampling_only, render_target };
    enum class texture_filter { nearest, linear };
    enum class texture_wrap { clamp, repeat, mirror };
    enum class buffer_usage { static_mesh, dynamic_mesh };
    enum class buffer_lock_mode { read_write, read_only, write_only };
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
        float_1, float_2, float_3, float_4,
        int_1, int_2, int_3, int_4, matrix_4x4,
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
    inline constexpr sampler_state linear_clamp{};
    inline constexpr sampler_state linear_wrap{
        texture_filter::linear, texture_filter::linear, texture_filter::linear,
        texture_wrap::repeat, texture_wrap::repeat};
    inline constexpr sampler_state nearest_clamp{
        texture_filter::nearest, texture_filter::nearest, texture_filter::nearest,
        texture_wrap::clamp, texture_wrap::clamp};
    inline constexpr sampler_state nearest_wrap{
        texture_filter::nearest, texture_filter::nearest, texture_filter::nearest,
        texture_wrap::repeat, texture_wrap::repeat};

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
    enum class texture_lock_mode { read_write, read_only, write_only };

    enum class primitive_topology { triangle_list, triangle_strip, triangle_fan };
    enum class cull_mode { none, clockwise, counter_clockwise };
    enum class compare_func { never, less, equal, less_equal, greater, not_equal, greater_equal, always };
    enum class blend_factor { zero, one, src_alpha, inv_src_alpha };
    enum class blend_op { add };
    enum class texture_operation { vertex_color, replace, modulate, alpha_mask };
    enum class lighting_mode { unlit };

    struct render_viewport {
        vec2i origin = {};
        vec2i size = {};
        float min_depth = 0.0f;
        float max_depth = 1.0f;
    };
    struct blend_state {
        bool enabled = false;
        blend_factor src = blend_factor::src_alpha;
        blend_factor dst = blend_factor::inv_src_alpha;
        blend_op op = blend_op::add;
    };
    struct depth_state {
        bool test_enabled = false;
        bool write_enabled = false;
        compare_func compare = compare_func::less_equal;
    };
    struct raster_state { cull_mode cull = cull_mode::none; };
    struct basic_effect {
        texture_operation texture_op = texture_operation::vertex_color;
        lighting_mode lighting = lighting_mode::unlit;
        transform world = transform::identity();
        transform projection = transform::identity();
    };

    struct texture_sampler_binding {
        int slot = 0;
        texture_handle *texture = nullptr;
        sampler_state sampler = {};
    };
    struct vertex_definition_view {
        std::size_t index = 0;
        int stride = 0;
        std::span<const vertex_element> elements;
    };
    struct pipeline_desc {
        shader_program_handle *shader = nullptr;
        const basic_effect *effect = nullptr;
        vertex_definition_view vertex_layout;
        blend_state blend = {};
        depth_state depth = {};
        raster_state raster = {};
    };
    struct render_pass_begin_info {
        swapchain_handle *swapchain = nullptr;
        texture_handle *target_texture = nullptr;
        int target_level = 0;
        vec2i target_size = {};
        std::optional<color> clear_color;
        std::optional<float> clear_depth;
    };

    // Bind and upload operations are record-only. Backends consume their
    // recorded sources at draw time. Transient data remains valid through the
    // next same-kind bind/upload or end_render_pass.
    struct graphics_backend_interface {
        gfx_backend id = gfx_backend::auto_;
        vec2f pixel_center_offset = {};

        gfx_backend_op<void(device_handle *)> destroy_device;

        gfx_backend_op<texture_handle *(device_handle *, pixel_format, vec2i, int, texture_role, texture_usage)> create_texture;
        gfx_backend_op<void(texture_handle *)> destroy_texture;
        gfx_backend_op<pixel_format(const texture_handle *)> texture_format;
        gfx_backend_op<int(const texture_handle *)> texture_width;
        gfx_backend_op<int(const texture_handle *)> texture_height;
        gfx_backend_op<int(const texture_handle *)> texture_mip_levels;
        gfx_backend_op<sampler_state(const texture_handle *)> texture_sampler;
        gfx_backend_op<void(texture_handle *, const sampler_state &)> texture_set_sampler;
        gfx_backend_op<bool(texture_handle *, rect_i, int, texture_lock_mode, texture_lock_info &)> texture_lock;
        gfx_backend_op<void(texture_handle *, const texture_lock_info &, bool)> texture_unlock;
        gfx_backend_op<void(texture_handle *)> texture_generate_mipmaps;
        gfx_backend_op<texture_handle *(const texture_handle *)> texture_clone;
        gfx_backend_op<bool(device_handle *, texture_handle *, rect_i, vec2i, vec2i, int)> copy_render_target_to_texture;

        gfx_backend_op<vertex_buffer_handle *(device_handle *, int, int, buffer_usage, const void *)> create_vertex_buffer;
        gfx_backend_op<void(vertex_buffer_handle *)> destroy_vertex_buffer;
        gfx_backend_op<int(const vertex_buffer_handle *)> vertex_buffer_count;
        gfx_backend_op<int(const vertex_buffer_handle *)> vertex_buffer_stride;
        gfx_backend_op<buffer_usage(const vertex_buffer_handle *)> vertex_buffer_usage;
        gfx_backend_op<bool(vertex_buffer_handle *, int, int, buffer_lock_mode, buffer_lock_info &)> vertex_buffer_lock;
        gfx_backend_op<void(vertex_buffer_handle *, const buffer_lock_info &, bool)> vertex_buffer_unlock;
        gfx_backend_op<index_buffer_handle *(device_handle *, int, buffer_usage, const uint32_t *)> create_index_buffer;
        gfx_backend_op<void(index_buffer_handle *)> destroy_index_buffer;
        gfx_backend_op<int(const index_buffer_handle *)> index_buffer_count;
        gfx_backend_op<buffer_usage(const index_buffer_handle *)> index_buffer_usage;
        gfx_backend_op<bool(index_buffer_handle *, int, int, buffer_lock_mode, buffer_lock_info &)> index_buffer_lock;
        gfx_backend_op<void(index_buffer_handle *, const buffer_lock_info &, bool)> index_buffer_unlock;

        gfx_backend_op<shader_program_handle *(device_handle *, const shader_program_desc &)> create_shader_program;
        gfx_backend_op<void(shader_program_handle *)> destroy_shader_program;
        gfx_backend_op<shader_constant_slot(shader_program_handle *, std::string_view, shader_type)> shader_lookup_constant;
        gfx_backend_op<void(shader_program_handle *, const shader_constant_slot &, const shader_constant_payload &)> shader_set_constant;
        gfx_backend_op<shader_sampler_slot(shader_program_handle *, std::string_view, shader_type)> shader_lookup_sampler;
        gfx_backend_op<void(shader_program_handle *, const shader_sampler_slot &, texture_handle *)> shader_set_sampler;

        gfx_backend_op<swapchain_handle *(device_handle *, void *, vec2i)> create_swapchain;
        gfx_backend_op<void(swapchain_handle *)> destroy_swapchain;
        gfx_backend_op<void(swapchain_handle *)> swapchain_begin_frame;
        gfx_backend_op<void(swapchain_handle *)> swapchain_end_frame;
        gfx_backend_op<void(swapchain_handle *)> swapchain_present;
        gfx_backend_op<void(swapchain_handle *, vec2i)> swapchain_on_resize;

        gfx_backend_op<pipeline_handle *(device_handle *, const pipeline_desc &)> create_pipeline;
        gfx_backend_op<void(pipeline_handle *)> destroy_pipeline;
        gfx_backend_op<void(pipeline_handle *, const pipeline_desc &)> update_pipeline;
        gfx_backend_op<void(device_handle *, pipeline_handle *)> bind_pipeline;
        gfx_backend_op<bool(device_handle *, const render_pass_begin_info &)> begin_render_pass;
        gfx_backend_op<void(device_handle *)> end_render_pass;
        gfx_backend_op<void(device_handle *, const render_viewport &)> set_viewport;
        gfx_backend_op<void(device_handle *, vertex_buffer_handle *)> bind_vertex_buffer;
        gfx_backend_op<void(device_handle *, index_buffer_handle *)> bind_index_buffer;
        gfx_backend_op<void(device_handle *, const void *, int)> upload_transient_vertex_data;
        gfx_backend_op<void(device_handle *, std::span<const uint32_t>)> upload_transient_index_data;
        gfx_backend_op<void(device_handle *, const texture_sampler_binding &)> bind_resources;
        gfx_backend_op<void(device_handle *, primitive_topology, int, int)> draw;
        gfx_backend_op<void(device_handle *, primitive_topology, int, int, int)> draw_indexed;
    };

    struct created_device { device_handle *handle = nullptr; graphics_backend_interface iface; };
    struct gfx_backend_factory { gfx_backend id; created_device (*create)(); };
    void register_gfx_backend(gfx_backend_factory factory);

} // namespace alia

#endif
