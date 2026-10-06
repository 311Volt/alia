#ifndef GRAPHICS_BACKEND_INTERFACE_C6537708_AD74_4B03_AB5F_5848902518DF
#define GRAPHICS_BACKEND_INTERFACE_C6537708_AD74_4B03_AB5F_5848902518DF

#include "../core/color.hpp"
#include "../core/rect.hpp"
#include "../core/vec.hpp"
#include "bitmap/pixel.hpp"
#include "framebuffer_config.hpp"
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

    // One backend operation slot. A null operation means the hardware lacks
    // the feature (probed at device creation), never that the backend has not
    // implemented it yet; reason_unsupported then says why.
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

    // Opaque bases. Concrete backend handles inherit these types and are only
    // downcast inside their own backend implementation files. 2D and cube
    // textures share texture_handle; concrete texture records carry their kind
    // and shared texture operations dispatch on it.
    struct device_handle {};
    struct texture_handle {};
    struct vertex_buffer_handle {};
    struct index_buffer_handle {};
    struct swapchain_handle {};
    struct shader_program_handle {};
    struct pipeline_handle {};

    enum class gfx_backend {
        auto_,
        d3d9,
        opengl
    };
    enum class clip_depth_range {
        negative_one_to_one,
        zero_to_one
    };
    enum class vsync_mode {
        disable, // Request immediate presentation; fail creation if rejected.
        suggest, // Request synchronization, but allow creation without control.
        require  // Request synchronization; fail creation if rejected.
    };
    struct swapchain_desc {
        vsync_mode vsync = vsync_mode::disable;
        framebuffer_config framebuffer;
    };
    enum class texture_role {
        color,
        alpha_mask
    };
    enum class texture_usage {
        sampling_only,
        render_target
    };
    enum class cube_face {
        positive_x,
        negative_x,
        positive_y,
        negative_y,
        positive_z,
        negative_z
    };
    inline constexpr int cube_face_count = 6;
    enum class texture_filter {
        nearest,
        linear
    };
    enum class texture_wrap {
        clamp,
        repeat,
        mirror
    };
    enum class buffer_usage {
        static_mesh,
        dynamic_mesh
    };
    enum class buffer_lock_mode {
        read_write,
        read_only,
        write_only
    };
    enum class shader_type {
        vertex,
        pixel
    };

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
    inline constexpr sampler_state linear_clamp{};
    inline constexpr sampler_state linear_wrap{
        texture_filter::linear, texture_filter::linear, texture_filter::linear, texture_wrap::repeat, texture_wrap::repeat
    };
    inline constexpr sampler_state nearest_clamp{
        texture_filter::nearest, texture_filter::nearest, texture_filter::nearest, texture_wrap::clamp, texture_wrap::clamp
    };
    inline constexpr sampler_state nearest_wrap{
        texture_filter::nearest, texture_filter::nearest, texture_filter::nearest, texture_wrap::repeat, texture_wrap::repeat
    };

    struct texture_lock_info {
        vec2i origin;
        vec2i extent;
        int stride_bytes = 0;
        int level = 0;
        int face = 0; // Locked cube face; zero for 2D textures.
        std::byte *data = nullptr;
    };
    struct buffer_lock_info {
        int offset_bytes = 0;
        int size_bytes = 0;
        std::byte *data = nullptr;
    };
    enum class texture_lock_mode {
        read_write,
        read_only,
        write_only
    };

    enum class primitive_topology {
        triangle_list,
        triangle_strip,
        triangle_fan
    };
    enum class cull_mode {
        none,
        clockwise,
        counter_clockwise
    };
    enum class compare_func {
        never,
        less,
        equal,
        less_equal,
        greater,
        not_equal,
        greater_equal,
        always
    };
    enum class blend_factor {
        zero,
        one,
        src_alpha,
        inv_src_alpha
    };
    enum class blend_op {
        add
    };
    enum class texture_operation {
        vertex_color,
        replace,
        modulate,
        alpha_mask
    };
    enum class lighting_mode {
        unlit
    };

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
    struct raster_state {
        cull_mode cull = cull_mode::none;
    };
    // Fixed-function effect. Pipelines reference it without owning it and
    // backends read the matrices at draw time, so changes apply to later draws.
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
    struct render_target_info {
        swapchain_handle *swapchain = nullptr;
        texture_handle *target_texture = nullptr;
        int target_level = 0;
        int target_face = 0; // Selected cube face; zero otherwise.
        vec2i target_size = {};
    };

    // One instance per gfx_device. Device-owned objects keep a raw pointer to
    // it, so the device must outlive them.
    //
    // Bind and upload operations are record-only. Backends consume their
    // recorded sources at draw time. Transient data remains valid through the
    // next same-kind bind/upload or swapchain_end_frame; L3 enforces this, and
    // every backend clears the recorded sources at frame end.
    // Unless an operation says otherwise, object handles must be live handles
    // created by this interface and its device; the device outlives all of them.
    struct graphics_backend_interface {
        gfx_backend id = gfx_backend::auto_;
        vec2f pixel_center_offset = {};
        clip_depth_range clip_depth = clip_depth_range::negative_one_to_one;
        gfx_device_caps caps;

        /// @brief Release a device after all objects created from it have been destroyed.
        /// @param device Device handle returned with this interface.
        /// @return Nothing.
        gfx_backend_op<void(device_handle *device)> destroy_device;

        /// @brief Create an uninitialized 2D texture with the requested mip chain and role.
        /// @param device Device that will own the texture.
        /// @param format Pixel storage format for every mip level.
        /// @param size Width and height in pixels of mip level zero.
        /// @param mip_levels Number of levels; one means no mipmaps, zero requests a full chain.
        /// @param role Whether the texture stores color or an alpha mask.
        /// @param usage Whether the texture is for sampling only or can be a render target.
        /// @return New texture handle, or null if creation fails.
        gfx_backend_op<texture_handle *(device_handle *device, pixel_format format, vec2i size, int mip_levels, texture_role role, texture_usage usage)> create_texture;

        /// @brief Create an uninitialized cube texture with six square faces.
        /// @param device Device that will own the texture.
        /// @param format Pixel storage format for every face and mip level.
        /// @param edge Width and height in pixels of each base-level face.
        /// @param mip_levels Number of levels; one means no mipmaps, zero requests a full chain.
        /// @param usage Whether the texture is for sampling only or can be a render target.
        /// @return New cube texture handle, or null if creation fails.
        gfx_backend_op<texture_handle *(device_handle *device, pixel_format format, int edge, int mip_levels, texture_usage usage)> create_cube_texture;

        /// @brief Release a 2D or cube texture handle and its backend resource.
        /// @param texture Texture handle to release.
        /// @return Nothing.
        gfx_backend_op<void(texture_handle *texture)> destroy_texture;

        /// @brief Return the pixel format stored by a texture.
        /// @param texture Texture to query.
        /// @return The texture's pixel format.
        gfx_backend_op<pixel_format(const texture_handle *texture)> texture_format;

        /// @brief Return the base-level width of a texture in pixels.
        /// @param texture Texture to query.
        /// @return Base-level width; for cube textures this is the face edge length.
        gfx_backend_op<int(const texture_handle *texture)> texture_width;

        /// @brief Return the base-level height of a texture in pixels.
        /// @param texture Texture to query.
        /// @return Base-level height; for cube textures this is the face edge length.
        gfx_backend_op<int(const texture_handle *texture)> texture_height;

        /// @brief Return the number of allocated mip levels in a texture.
        /// @param texture Texture to query.
        /// @return Number of levels, including level zero.
        gfx_backend_op<int(const texture_handle *texture)> texture_mip_levels;

        /// @brief Read the sampler state last assigned to a texture.
        /// @param texture Texture to query.
        /// @return The texture's current sampler state.
        gfx_backend_op<sampler_state(const texture_handle *texture)> texture_sampler;

        /// @brief Store the sampler state used when this texture is sampled.
        /// @param texture Texture whose sampler state is changed.
        /// @param sampler Minification, magnification, mip filtering, and U/V wrap modes.
        /// @return Nothing.
        gfx_backend_op<void(texture_handle *texture, const sampler_state &sampler)> texture_set_sampler;

        /// @brief Lock a rectangular region of one 2D texture mip level for CPU access.
        /// @param texture 2D texture to lock.
        /// @param region Requested rectangle in mip-level pixel coordinates.
        /// @param level Mip level to lock, where zero is the base level.
        /// @param mode Whether the caller reads, writes, or both; write-only locks need not preserve old contents.
        /// @param info Output lock record; on success describes the clipped accessible origin, extent, byte stride, level, face zero, and data pointer. It is usable only when the call returns true.
        /// @return True if the region was locked and `info` was populated; false if it could not be locked or the rectangle does not overlap the mip level.
        gfx_backend_op<bool(texture_handle *texture, rect_i region, int level, texture_lock_mode mode, texture_lock_info &info)> texture_lock;

        /// @brief Lock a rectangular region of one cube-texture face and mip level for CPU access.
        /// @param texture Cube texture to lock.
        /// @param face Face to lock.
        /// @param region Requested rectangle in face mip-level pixel coordinates.
        /// @param level Mip level to lock, where zero is the base level.
        /// @param mode Whether the caller reads, writes, or both; write-only locks need not preserve old contents.
        /// @param info Output lock record; on success describes the clipped accessible origin, extent, byte stride, level, face, and data pointer. It is usable only when the call returns true.
        /// @return True if the face region was locked and `info` was populated; false if it could not be locked or the rectangle does not overlap the face mip level.
        gfx_backend_op<bool(texture_handle *texture, cube_face face, rect_i region, int level, texture_lock_mode mode, texture_lock_info &info)> cube_texture_lock;

        /// @brief Release a previously successful texture lock and commit writes when requested.
        /// @param texture Texture whose level or face is locked.
        /// @param info Lock record returned by `texture_lock` or `cube_texture_lock` for this lock.
        /// @param wrote True if the caller may have changed pixels and the region must be committed; false for read-only access.
        /// @return Nothing.
        gfx_backend_op<void(texture_handle *texture, const texture_lock_info &info, bool wrote)> texture_unlock;

        /// @brief Regenerate the texture's mip levels from level zero.
        /// @param texture 2D or cube texture to update.
        /// @return Nothing.
        gfx_backend_op<void(texture_handle *texture)> texture_generate_mipmaps;

        /// @brief Make a deep GPU-side copy of a texture, including its levels and sampler state.
        /// @param texture Source 2D or cube texture.
        /// @return Independent cloned texture handle, or null if cloning fails.
        gfx_backend_op<texture_handle *(const texture_handle *texture)> texture_clone;

        /// @brief Copy pixels from the currently selected render target into a texture region.
        /// @param device Device whose current render target supplies the pixels.
        /// @param destination Destination texture; may be 2D or cube.
        /// @param source_region Rectangle in current-target pixel coordinates.
        /// @param source_size Size of the current render target in pixels, used to translate top-left source coordinates for the backend.
        /// @param destination_origin Top-left destination pixel in the selected mip level.
        /// @param destination_level Destination mip level, where zero is the base level.
        /// @param destination_face Destination cube face index; zero for a 2D texture.
        /// @return True if the region was copied; false if the source, format, or destination cannot be copied.
        gfx_backend_op<bool(device_handle *device, texture_handle *destination, rect_i source_region, vec2i source_size, vec2i destination_origin, int destination_level, int destination_face)> copy_render_target_to_texture;

        /// @brief Allocate a vertex buffer and optionally initialize all its vertices.
        /// @param device Device that will own the buffer.
        /// @param vertex_stride Size in bytes of each vertex record.
        /// @param vertex_count Number of vertex records to allocate; must be positive.
        /// @param usage Static or dynamic buffer usage hint.
        /// @param initial_data Optional pointer to `vertex_count * vertex_stride` initial bytes; may be null.
        /// @return New vertex-buffer handle, or null if allocation fails.
        gfx_backend_op<vertex_buffer_handle *(device_handle *device, int vertex_stride, int vertex_count, buffer_usage usage, const void *initial_data)> create_vertex_buffer;

        /// @brief Release a vertex buffer and its backend resource.
        /// @param buffer Vertex buffer to release.
        /// @return Nothing.
        gfx_backend_op<void(vertex_buffer_handle *buffer)> destroy_vertex_buffer;

        /// @brief Return the number of vertices allocated in a vertex buffer.
        /// @param buffer Vertex buffer to query.
        /// @return Vertex count.
        gfx_backend_op<int(const vertex_buffer_handle *buffer)> vertex_buffer_count;

        /// @brief Return the byte size of one vertex record in a vertex buffer.
        /// @param buffer Vertex buffer to query.
        /// @return Vertex stride in bytes.
        gfx_backend_op<int(const vertex_buffer_handle *buffer)> vertex_buffer_stride;

        /// @brief Return the usage hint supplied when a vertex buffer was created.
        /// @param buffer Vertex buffer to query.
        /// @return The buffer's static or dynamic usage.
        gfx_backend_op<buffer_usage(const vertex_buffer_handle *buffer)> vertex_buffer_usage;

        /// @brief Lock a contiguous range of vertices for CPU access.
        /// @param buffer Vertex buffer to lock.
        /// @param first_vertex First vertex index in the range.
        /// @param vertex_count Number of vertices to lock.
        /// @param mode Whether the caller reads, writes, or both; write-only locks need not preserve old contents.
        /// @param info Output lock record; on success gives byte offset, byte size, and the first accessible data byte.
        /// @return True if the range was locked and `info` was populated; false if it could not be locked.
        gfx_backend_op<bool(vertex_buffer_handle *buffer, int first_vertex, int vertex_count, buffer_lock_mode mode, buffer_lock_info &info)> vertex_buffer_lock;

        /// @brief Release a previously successful vertex-buffer lock and commit writes when requested.
        /// @param buffer Locked vertex buffer.
        /// @param info Lock record returned by `vertex_buffer_lock` for this lock.
        /// @param wrote True if the caller may have changed vertices; false for read-only access.
        /// @return Nothing.
        gfx_backend_op<void(vertex_buffer_handle *buffer, const buffer_lock_info &info, bool wrote)> vertex_buffer_unlock;

        /// @brief Allocate an index buffer and optionally initialize all its 32-bit indices.
        /// @param device Device that will own the buffer.
        /// @param index_count Number of indices to allocate; must be positive.
        /// @param usage Static or dynamic buffer usage hint.
        /// @param initial_data Optional pointer to `index_count` indices; may be null.
        /// @return New index-buffer handle, or null if allocation fails.
        gfx_backend_op<index_buffer_handle *(device_handle *device, int index_count, buffer_usage usage, const uint32_t *initial_data)> create_index_buffer;

        /// @brief Release an index buffer and its backend resource.
        /// @param buffer Index buffer to release.
        /// @return Nothing.
        gfx_backend_op<void(index_buffer_handle *buffer)> destroy_index_buffer;

        /// @brief Return the number of 32-bit indices allocated in an index buffer.
        /// @param buffer Index buffer to query.
        /// @return Index count.
        gfx_backend_op<int(const index_buffer_handle *buffer)> index_buffer_count;

        /// @brief Return the usage hint supplied when an index buffer was created.
        /// @param buffer Index buffer to query.
        /// @return The buffer's static or dynamic usage.
        gfx_backend_op<buffer_usage(const index_buffer_handle *buffer)> index_buffer_usage;

        /// @brief Lock a contiguous range of indices for CPU access.
        /// @param buffer Index buffer to lock.
        /// @param first_index First index position in the range.
        /// @param index_count Number of indices to lock.
        /// @param mode Whether the caller reads, writes, or both; write-only locks need not preserve old contents.
        /// @param info Output lock record; on success gives byte offset, byte size, and the first accessible data byte.
        /// @return True if the range was locked and `info` was populated; false if it could not be locked.
        gfx_backend_op<bool(index_buffer_handle *buffer, int first_index, int index_count, buffer_lock_mode mode, buffer_lock_info &info)> index_buffer_lock;

        /// @brief Release a previously successful index-buffer lock and commit writes when requested.
        /// @param buffer Locked index buffer.
        /// @param info Lock record returned by `index_buffer_lock` for this lock.
        /// @param wrote True if the caller may have changed indices; false for read-only access.
        /// @return Nothing.
        gfx_backend_op<void(index_buffer_handle *buffer, const buffer_lock_info &info, bool wrote)> index_buffer_unlock;

        /// @brief Compile and link the vertex and pixel shader sources in a program description.
        /// @param device Device that will own the program.
        /// @param description Shader sources and optional constant and sampler bindings.
        /// @return New shader-program handle. Compilation, linking, or invalid source errors are reported by throwing `shader_error`.
        gfx_backend_op<shader_program_handle *(device_handle *device, const shader_program_desc &description)> create_shader_program;

        /// @brief Release a compiled shader program.
        /// @param program Shader program to release.
        /// @return Nothing.
        gfx_backend_op<void(shader_program_handle *program)> destroy_shader_program;

        /// @brief Find a named shader constant and describe how to address it in later updates.
        /// @param program Program containing the constant.
        /// @param name Constant name as used by the shader source or its declared binding.
        /// @param stage Shader stage that owns the constant.
        /// @return A valid slot when found, or a slot with `valid == false` when absent.
        gfx_backend_op<shader_constant_slot(shader_program_handle *program, std::string_view name, shader_type stage)> shader_lookup_constant;

        /// @brief Record a shader constant value for application when the program is used.
        /// @param program Program whose constant state is updated.
        /// @param slot Valid constant slot returned by `shader_lookup_constant`.
        /// @param payload Typed scalar, vector, or matrix data to store; its spans must remain valid for this call.
        /// @return Nothing.
        gfx_backend_op<void(shader_program_handle *program, const shader_constant_slot &slot, const shader_constant_payload &payload)> shader_set_constant;

        /// @brief Find a named shader sampler and describe its location and texture unit.
        /// @param program Program containing the sampler.
        /// @param name Sampler name as used by the shader source or its declared binding.
        /// @param stage Shader stage that owns the sampler.
        /// @return A valid slot when found, or a slot with `valid == false` when absent.
        gfx_backend_op<shader_sampler_slot(shader_program_handle *program, std::string_view name, shader_type stage)> shader_lookup_sampler;

        /// @brief Record the texture assigned to a shader sampler for application when the program is used.
        /// @param program Program whose sampler state is updated.
        /// @param slot Valid sampler slot returned by `shader_lookup_sampler`.
        /// @param texture Texture to assign, or null to clear the sampler binding.
        /// @return Nothing.
        gfx_backend_op<void(shader_program_handle *program, const shader_sampler_slot &slot, texture_handle *texture)> shader_set_sampler;

        /// @brief Create a swapchain for a native window surface.
        /// @param device Device that will own the swapchain.
        /// @param native_surface Backend-native window handle; passed through from the window abstraction.
        /// @param size Initial surface size in pixels.
        /// @param description Requested vsync and framebuffer properties.
        /// @return New swapchain handle, or null if it cannot be created.
        gfx_backend_op<swapchain_handle *(device_handle *device, void *native_surface, vec2i size, const swapchain_desc &description)> create_swapchain;

        /// @brief Release a swapchain and its surface-dependent resources.
        /// @param swapchain Swapchain to release.
        /// @return Nothing.
        gfx_backend_op<void(swapchain_handle *swapchain)> destroy_swapchain;

        /// @brief Report the actual framebuffer properties provided by a swapchain.
        /// @param swapchain Swapchain to query.
        /// @return Properties selected by the backend, including color, depth, stencil, multisampling, and regional-update support.
        gfx_backend_op<framebuffer_properties(const swapchain_handle *swapchain)> swapchain_properties;

        /// @brief Prepare the swapchain backend for rendering a new frame.
        /// @param swapchain Swapchain whose frame is beginning.
        /// @return Nothing.
        gfx_backend_op<void(swapchain_handle *swapchain)> swapchain_begin_frame;

        /// @brief Finish rendering on a swapchain without presenting its backbuffer and clear recorded transient draw sources.
        /// @param swapchain Swapchain whose frame is ending.
        /// @return Nothing.
        gfx_backend_op<void(swapchain_handle *swapchain)> swapchain_end_frame;

        /// @brief Present the completed swapchain backbuffer to its native surface.
        /// @param swapchain Swapchain to present.
        /// @return Nothing.
        gfx_backend_op<void(swapchain_handle *swapchain)> swapchain_present;

        /// @brief Present only a rectangular portion of the completed swapchain backbuffer when supported.
        /// @param swapchain Swapchain to present.
        /// @param region Rectangle in swapchain pixel coordinates to update.
        /// @return True if the requested region was presented; false if the backend could not perform the regional update.
        gfx_backend_op<bool(swapchain_handle *swapchain, rect_i region)> swapchain_present_region;

        /// @brief Resize swapchain-dependent buffers and state after the native surface changes size.
        /// @param swapchain Swapchain to resize.
        /// @param new_size New positive surface dimensions in pixels.
        /// @return Nothing.
        gfx_backend_op<void(swapchain_handle *swapchain, vec2i new_size)> swapchain_on_resize;

        /// @brief Create a pipeline from shader or fixed-function effect and render-state descriptions.
        /// @param device Device that will own the pipeline.
        /// @param description Shader/effect, vertex layout, blend, depth, and raster state to retain. Referenced shader/effect objects are non-owning and must outlive the pipeline.
        /// @return New pipeline handle, or null if the backend cannot create it.
        gfx_backend_op<pipeline_handle *(device_handle *device, const pipeline_desc &description)> create_pipeline;

        /// @brief Release a pipeline.
        /// @param pipeline Pipeline to release.
        /// @return Nothing.
        gfx_backend_op<void(pipeline_handle *pipeline)> destroy_pipeline;

        /// @brief Replace the retained description of a mutable pipeline.
        /// @param pipeline Pipeline to update.
        /// @param description New shader/effect, vertex layout, blend, depth, and raster state. Referenced shader/effect objects are non-owning and must outlive the pipeline.
        /// @return Nothing.
        gfx_backend_op<void(pipeline_handle *pipeline, const pipeline_desc &description)> update_pipeline;

        /// @brief Bind a pipeline as the device's current draw pipeline.
        /// @param device Device whose pipeline state is changed.
        /// @param pipeline Pipeline to bind.
        /// @return Nothing.
        gfx_backend_op<void(device_handle *device, pipeline_handle *pipeline)> bind_pipeline;

        /// @brief Select the swapchain backbuffer or a texture mip/face as the current render target and reset the viewport to its full extent.
        /// @param device Device whose render target is changed.
        /// @param target Target description; exactly one of `swapchain` and `target_texture` identifies the target, with level, face, and size describing texture targets.
        /// @return True if the target was selected; false if it is incomplete or unsupported.
        gfx_backend_op<bool(device_handle *device, const render_target_info &target)> set_render_target;

        /// @brief Clear the current render target's selected color and/or depth attachments.
        /// @param device Device whose current target is cleared.
        /// @param clear_color Optional color value; absent leaves color unchanged.
        /// @param clear_depth Optional depth value; absent leaves depth unchanged.
        /// @return True if the requested clear succeeded or no attachments were requested; false on backend failure.
        gfx_backend_op<bool(device_handle *device, const std::optional<color> &clear_color, const std::optional<float> &clear_depth)> clear;

        /// @brief Set the current render viewport and depth range.
        /// @param device Device whose viewport is changed.
        /// @param viewport Pixel origin and size plus normalized minimum and maximum depth values.
        /// @return Nothing.
        gfx_backend_op<void(device_handle *device, const render_viewport &viewport)> set_viewport;

        /// @brief Record a vertex buffer as the vertex source for the next draw.
        /// @param device Device whose draw source is changed.
        /// @param buffer Vertex buffer to use, or null to clear the bound source.
        /// @return Nothing.
        gfx_backend_op<void(device_handle *device, vertex_buffer_handle *buffer)> bind_vertex_buffer;

        /// @brief Record an index buffer as the index source for the next indexed draw.
        /// @param device Device whose draw source is changed.
        /// @param buffer Index buffer to use, or null to clear the bound source.
        /// @return Nothing.
        gfx_backend_op<void(device_handle *device, index_buffer_handle *buffer)> bind_index_buffer;

        /// @brief Record caller-owned vertex bytes as the source for subsequent draw calls in this frame; the backend does not copy the bytes.
        /// @param device Device receiving the transient source.
        /// @param data Address of the first byte of vertex data; it must remain valid through the next vertex bind/upload or frame end.
        /// @param byte_count Number of valid bytes beginning at `data`.
        /// @return Nothing.
        gfx_backend_op<void(device_handle *device, const void *data, int byte_count)> upload_transient_vertex_data;

        /// @brief Record caller-owned 32-bit indices as the source for subsequent indexed draws in this frame; the backend does not copy the indices.
        /// @param device Device receiving the transient source.
        /// @param indices Index values; the referenced storage must remain valid through the next index bind/upload or frame end.
        /// @return Nothing.
        gfx_backend_op<void(device_handle *device, std::span<const uint32_t> indices)> upload_transient_index_data;

        /// @brief Record a texture and sampler state for a slot; a null texture clears that slot, and the recorded binding is consumed at draw time.
        /// @param device Device whose resource binding is changed.
        /// @param binding Slot, optional texture, and sampler configuration; a null texture clears that slot.
        /// @return Nothing.
        gfx_backend_op<void(device_handle *device, const texture_sampler_binding &binding)> bind_resources;

        /// @brief Submit a non-indexed draw using the current pipeline and recorded vertex source.
        /// @param device Device on which to draw.
        /// @param topology Triangle list, strip, or fan interpretation of the vertices.
        /// @param vertex_count Number of vertices to consume from the selected source.
        /// @param first_vertex First vertex in the bound buffer or transient data source.
        /// @return Nothing.
        gfx_backend_op<void(device_handle *device, primitive_topology topology, int vertex_count, int first_vertex)> draw;

        /// @brief Submit an indexed draw using the current pipeline and recorded vertex and index sources.
        /// @param device Device on which to draw.
        /// @param topology Triangle list, strip, or fan interpretation of the indices.
        /// @param index_count Number of indices consumed by this draw.
        /// @param first_index First index in the bound index buffer or transient index span.
        /// @param base_vertex Vertex offset added to each index before fetching from the vertex source.
        /// @return Nothing.
        gfx_backend_op<void(device_handle *device, primitive_topology topology, int index_count, int first_index, int base_vertex)> draw_indexed;
    };

    struct created_device {
        device_handle *handle = nullptr;
        // Not "interface": that is a macro in MinGW's COM headers.
        graphics_backend_interface iface;
    };
    struct gfx_backend_factory {
        gfx_backend id;
        /// @brief Select an adapter and render method, create a device, probe capabilities, and build the full interface.
        /// @param config Adapter and rendering requirements for device creation.
        /// @return The new device handle and populated operation table; the handle is null if this backend cannot create a usable device.
        created_device (*create)(const gfx_device_config &config);
    };
    void register_gfx_backend(gfx_backend_factory factory);

} // namespace alia

#endif /* GRAPHICS_BACKEND_INTERFACE_C6537708_AD74_4B03_AB5F_5848902518DF */
