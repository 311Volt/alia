#ifndef ALIA_GFX_FRAME_HPP
#define ALIA_GFX_FRAME_HPP

#include "pipeline.hpp"
#include "prim_buffers.hpp"
#include "texture.hpp"

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
    class frame {
    public:
        frame() = delete;
        ~frame();
        frame(frame &&) noexcept;
        frame &operator=(frame &&) noexcept;
        frame(const frame &) = delete;
        frame &operator=(const frame &) = delete;

        // Select the swapchain backbuffer (which has a depth attachment).
        void set_target();
        // Select a render-target texture mip level (which has no depth attachment).
        void set_target(texture &, int level = 0);
        // Clear either attachment. A depth clear is valid only on the backbuffer.
        void clear(std::optional<color> color = {}, std::optional<float> depth = {});

        void set_pipeline(pipeline &);
        void set_texture(int slot, texture &tex);
        void set_texture(int slot, texture &tex, const sampler_state &sampler);
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
        void present();
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
        void validate_pipeline(pipeline &) const;
        void prepare_draw(const vertex_definition_view &);
        void unbind_render_target_source(texture_handle *);
        [[nodiscard]] const graphics_backend_interface *backend() const noexcept { return swapchain_->backend_; }
        [[nodiscard]] device_handle *device() const noexcept { return swapchain_->device_; }

        swapchain *swapchain_ = nullptr;
        pipeline *pipeline_ = nullptr;
        vec2i target_size_ = {};
        bool target_has_depth_ = false;
        std::unordered_map<int, texture_handle *> texture_bindings_;
        bool active_ = false;
    };
} // namespace alia

#endif
