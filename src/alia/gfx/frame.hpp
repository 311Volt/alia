#ifndef ALIA_GFX_FRAME_HPP
#define ALIA_GFX_FRAME_HPP

#include "cube_texture.hpp"
#include "lighting.hpp"
#include "prim_buffers.hpp"
#include "shader.hpp"

#include <array>
#include <initializer_list>
#include <optional>
#include <span>
#include <unordered_map>

namespace alia {
    class frame;
    namespace detail {
        void frame_draw_transient(frame &, const void *, int, const vertex_definition_view &, primitive_topology);
        void frame_draw_buffered(frame &, vertex_buffer_handle *, int, int, const vertex_definition_view &, primitive_topology);
        void frame_draw_indexed_transient(frame &, const void *, int, std::span<const uint32_t>, const vertex_definition_view &, primitive_topology);
        void frame_draw_indexed_buffered(frame &, vertex_buffer_handle *, int, index_buffer_handle *, int, int, int, int, const vertex_definition_view &, primitive_topology);
    }

    // A frame owns all drawing state for one swapchain update. It starts on
    // the backbuffer with no implicit clear; targets and clears are commands.
    // Defaults are alpha blending, no depth or culling, fixed function, identity
    // world/view and a pixel-aligned UI projection. Lighting and fog start off,
    // ambient is black and material has its default values. Texture bindings persist.
    class frame {
        struct stored_lighting {
            color ambient = black;
            std::array<light, max_lights> lights = {};
            int count = 0;
            bool operator==(const stored_lighting &) const noexcept;
        };

    public:
        // Restores blend, depth, culling, shader, transforms, lights, ambient,
        // material and fog on destruction.
        // Targets, viewports and texture bindings are not part of the snapshot.
        // Scopes must end in reverse order, before moving or destroying the frame.
        class state_scope {
        public:
            ~state_scope();
            state_scope(state_scope &&) noexcept;
            state_scope &operator=(state_scope &&) = delete;
            state_scope(const state_scope &) = delete;
            state_scope &operator=(const state_scope &) = delete;

        private:
            friend class frame;
            explicit state_scope(frame &) noexcept;
            frame *frame_;
            shader_program *shader_;
            blend_state blend_;
            depth_state depth_;
            cull_mode cull_;
            transform_state transforms_;
            stored_lighting lighting_;
            material material_;
            fog_state fog_;
        };

        frame() = delete;
        ~frame();
        frame(frame &&) noexcept;
        frame &operator=(frame &&) noexcept;
        frame(const frame &) = delete;
        frame &operator=(const frame &) = delete;

        // Select the swapchain backbuffer (which may have a depth attachment).
        // Every set_target resets the viewport and projection to the target's
        // full extent; the remaining render state is retained.
        void set_target();
        // Select a render-target texture mip level (which has no depth attachment).
        // Frame bindings of the texture are removed first; shader-owned samplers
        // must avoid sampling it themselves.
        void set_target(texture &, int level = 0);
        // Select one face and mip level of a render-target cube texture, with
        // the same depth and binding rules as the texture overload.
        void set_target(cube_texture &, cube_face, int level = 0);
        // Clear either attachment. A depth clear is valid only on the backbuffer.
        void clear(std::optional<color> color = {}, std::optional<float> depth = {});

        void set_blend(const blend_state &);
        // Validated at draw time: testing or writing needs a depth attachment.
        void set_depth(const depth_state &);
        void set_cull(cull_mode);
        // Non-owning: the shader must stay valid while selected or saved in a scope.
        // Frame transforms never update shader constants automatically.
        void set_shader(shader_program &);
        // Return to fixed-function drawing.
        void set_shader(std::nullptr_t);
        // Row-vector order: world, then view, then projection.
        void set_world(const transform &);
        void set_view(const transform &);
        void set_projection(const transform &);
        [[nodiscard]] const transform &world() const noexcept { return transforms_.world; }
        [[nodiscard]] const transform &view() const noexcept { return transforms_.view; }
        [[nodiscard]] const transform &projection() const noexcept { return transforms_.projection; }
        [[nodiscard]] transform ui_projection() const;
        [[nodiscard]] state_scope save_state();
        // Saves the full render state, then sets world to local * world().
        [[nodiscard]] state_scope push_world(const transform &local);

        // Copies lights, up to caps().max_lights. Empty selects unlit drawing.
        // Only fixed-function vertex layouts with normals use lighting; other
        // layouts and shader draws stay unlit. Both spot parameter sets are
        // always validated. All components must be finite, directions nonzero,
        // and attenuation nonnegative with at least one positive term.
        // Invalid values/counts throw std::invalid_argument;
        // non-empty input on unsupported hardware throws unsupported_operation_exception.
        void set_lights(std::span<const light>);
        void set_lights(std::initializer_list<light>);
        // Ambient defaults to black. All color components must be finite.
        void set_ambient(color);
        // Finite colors and shininess in [0, 128] are required. Accepted and
        // retained even when the device does not support fixed-function lighting.
        void set_material(const material &);

        // Fixed-function fog applies to every vertex layout, including 2D.
        // Shader draws disable it; select none or restore a scope for an unfogged overlay.
        // All components must be finite, density nonnegative, and linear fog
        // must have start != end. Invalid input throws std::invalid_argument.
        // An enabled mode on unsupported hardware throws unsupported_operation_exception;
        // none is always accepted. Distances follow caps().fog_distance.
        void set_fog(const fog_state &);

        // Fixed-function draws derive the texture operation from the vertex
        // layout and slot 0: color textures modulate, alpha masks supply alpha,
        // and missing texture coordinates, empty slots or cubes use vertex color.
        void set_texture(int slot, texture &tex);
        void set_texture(int slot, texture &tex, const sampler_state &sampler);
        void set_texture(int slot, cube_texture &tex);
        void set_texture(int slot, cube_texture &tex, const sampler_state &sampler);
        void set_viewport(const render_viewport &vp);
        [[nodiscard]] vec2i target_size() const noexcept { return target_size_; }

        template <vertex_type TVertex>
        void draw(std::span<const TVertex> vertices, primitive_topology topology = primitive_topology::triangle_list) {
            detail::frame_draw_transient(*this, vertices.data(), static_cast<int>(vertices.size()), detail::vertex_definition_of<TVertex>(), topology);
        }
        template <vertex_type TVertex>
        void draw(vertex_buffer<TVertex> &vertices, int first_vertex = 0, int vertex_count = -1,
                  primitive_topology topology = primitive_topology::triangle_list) {
            const int count = vertex_count < 0 ? vertices.count() - first_vertex : vertex_count;
            detail::frame_draw_buffered(*this, vertices.impl(), count, first_vertex, detail::vertex_definition_of<TVertex>(), topology);
        }
        template <vertex_type TVertex>
        void draw_indexed(std::span<const TVertex> vertices, std::span<const uint32_t> indices,
                          primitive_topology topology = primitive_topology::triangle_list) {
            detail::frame_draw_indexed_transient(*this, vertices.data(), static_cast<int>(vertices.size()), indices, detail::vertex_definition_of<TVertex>(), topology);
        }
        template <vertex_type TVertex>
        void draw_indexed(vertex_buffer<TVertex> &vertices, index_buffer &indices,
                          int first_index = 0, int index_count = -1, int base_vertex = 0,
                          primitive_topology topology = primitive_topology::triangle_list) {
            detail::frame_draw_indexed_buffered(*this, vertices.impl(), vertices.count(), indices.impl(), indices.count(), first_index, index_count, base_vertex, detail::vertex_definition_of<TVertex>(), topology);
        }

        void copy_to_texture(texture &dst, rect_i src_rect, vec2i dst_pos = {}, int dst_level = 0);
        void copy_to_texture(
            cube_texture &dst,
            cube_face face,
            rect_i src_rect,
            vec2i dst_pos = {},
            int dst_level = 0
        );
        void present();
        void present(rect_i region);
        [[nodiscard]] bool valid() const noexcept { return active_; }

    private:
        friend class swapchain;
        friend void detail::frame_draw_transient(frame &, const void *, int, const vertex_definition_view &, primitive_topology);
        friend void detail::frame_draw_buffered(frame &, vertex_buffer_handle *, int, int, const vertex_definition_view &, primitive_topology);
        friend void detail::frame_draw_indexed_transient(frame &, const void *, int, std::span<const uint32_t>, const vertex_definition_view &, primitive_topology);
        friend void detail::frame_draw_indexed_buffered(frame &, vertex_buffer_handle *, int, index_buffer_handle *, int, int, int, int, const vertex_definition_view &, primitive_topology);
        explicit frame(swapchain &);
        void finish_without_present() noexcept;
        void ensure_active() const;
        void prepare_draw(const vertex_definition_view &);
        void unbind_render_target_source(texture_handle *);
        [[nodiscard]] const graphics_backend_interface *backend() const noexcept { return swapchain_->backend_; }
        [[nodiscard]] device_handle *device() const noexcept { return swapchain_->device_; }

        swapchain *swapchain_ = nullptr;
        shader_program *shader_ = nullptr;
        blend_state blend_ = alpha_blend;
        depth_state depth_ = {};
        cull_mode cull_ = cull_mode::none;
        render_state state_ = {};
        transform_state transforms_ = {};
        std::optional<transform_state> applied_transforms_;
        stored_lighting lighting_;
        material material_;
        fog_state fog_;
        std::optional<stored_lighting> applied_lighting_;
        std::optional<material> applied_material_;
        std::optional<fog_state> applied_fog_;
        bool render_state_dirty_ = true;
        bool transforms_dirty_ = true;
        bool lighting_dirty_ = true;
        bool material_dirty_ = true;
        bool fog_dirty_ = true;
        enum class slot0_kind { none, color, alpha_mask, cube };
        slot0_kind slot0_kind_ = slot0_kind::none;
        vec2i target_size_ = {};
        bool target_has_depth_ = false;
        std::unordered_map<int, texture_handle *> texture_bindings_;
        bool active_ = false;
    };
} // namespace alia

#endif
