#ifndef ALIA_GFX_RENDER_PASS_HPP
#define ALIA_GFX_RENDER_PASS_HPP

#include "pipeline.hpp"
#include "prim_buffers.hpp"
#include "texture.hpp"

#include <optional>
#include <span>

namespace alia {
    struct render_pass_desc {
        std::optional<color> clear_color;
        std::optional<float> clear_depth;
        int level = 0;
    };

    class frame;
    class render_pass;
    namespace detail {
        void pass_draw_transient(render_pass &, const void *, int, const vertex_definition_view &, primitive_topology);
        void pass_draw_buffered(render_pass &, vertex_buffer_handle *, int, int, const vertex_definition_view &, primitive_topology);
        void pass_draw_indexed_transient(render_pass &, const void *, int, std::span<const uint32_t>, const vertex_definition_view &, primitive_topology);
        void pass_draw_indexed_buffered(render_pass &, vertex_buffer_handle *, int, index_buffer_handle *, int, int, int, int, const vertex_definition_view &, primitive_topology);
    }

    class render_pass {
    public:
        render_pass() = delete;
        ~render_pass();
        render_pass(render_pass &&) noexcept;
        render_pass &operator=(render_pass &&) noexcept;
        render_pass(const render_pass &) = delete;
        render_pass &operator=(const render_pass &) = delete;

        void set_pipeline(pipeline &);
        void set_texture(int slot, texture &tex);
        void set_texture(int slot, texture &tex, const sampler_state &sampler);
        void set_viewport(const render_viewport &vp);
        [[nodiscard]] vec2i target_size() const noexcept { return target_size_; }

        template <vertex_type TVertex>
        void draw(std::span<const TVertex> vertices, primitive_topology topology = primitive_topology::triangle_list) {
            detail::pass_draw_transient(*this, vertices.data(), static_cast<int>(vertices.size()), detail::vertex_definition_of<TVertex>(), topology);
        }
        template <vertex_type TVertex>
        void draw(vertex_buffer<TVertex> &vertices, int first_vertex = 0, int vertex_count = -1,
                  primitive_topology topology = primitive_topology::triangle_list) {
            const int count = vertex_count < 0 ? vertices.count() - first_vertex : vertex_count;
            detail::pass_draw_buffered(*this, vertices.impl(), count, first_vertex, detail::vertex_definition_of<TVertex>(), topology);
        }
        template <vertex_type TVertex>
        void draw_indexed(std::span<const TVertex> vertices, std::span<const uint32_t> indices,
                          primitive_topology topology = primitive_topology::triangle_list) {
            detail::pass_draw_indexed_transient(*this, vertices.data(), static_cast<int>(vertices.size()), indices, detail::vertex_definition_of<TVertex>(), topology);
        }
        template <vertex_type TVertex>
        void draw_indexed(vertex_buffer<TVertex> &vertices, index_buffer &indices,
                          int first_index = 0, int index_count = -1, int base_vertex = 0,
                          primitive_topology topology = primitive_topology::triangle_list) {
            detail::pass_draw_indexed_buffered(*this, vertices.impl(), vertices.count(), indices.impl(), indices.count(), first_index, index_count, base_vertex, detail::vertex_definition_of<TVertex>(), topology);
        }

        void copy_to_texture(texture &dst, rect_i src_rect, vec2i dst_pos = {}, int dst_level = 0);
        void end();

    private:
        friend class frame;
        friend void detail::pass_draw_transient(render_pass &, const void *, int, const vertex_definition_view &, primitive_topology);
        friend void detail::pass_draw_buffered(render_pass &, vertex_buffer_handle *, int, int, const vertex_definition_view &, primitive_topology);
        friend void detail::pass_draw_indexed_transient(render_pass &, const void *, int, std::span<const uint32_t>, const vertex_definition_view &, primitive_topology);
        friend void detail::pass_draw_indexed_buffered(render_pass &, vertex_buffer_handle *, int, index_buffer_handle *, int, int, int, int, const vertex_definition_view &, primitive_topology);
        render_pass(frame &, pipeline &, const render_pass_begin_info &, bool has_depth);
        void ensure_active() const;
        void validate_pipeline(pipeline &) const;
        void prepare_draw(const vertex_definition_view &);
        frame *frame_ = nullptr;
        const graphics_backend_interface *backend_ = nullptr;
        device_handle *device_ = nullptr;
        pipeline *pipeline_ = nullptr;
        vec2i target_size_ = {};
        bool has_depth_ = false;
        bool active_ = false;
    };

    class frame {
    public:
        frame() = delete;
        ~frame();
        frame(frame &&) noexcept;
        frame &operator=(frame &&) noexcept;
        frame(const frame &) = delete;
        frame &operator=(const frame &) = delete;

        [[nodiscard]] render_pass begin_render_pass(pipeline &, const render_pass_desc &desc = {});
        [[nodiscard]] render_pass begin_render_pass(pipeline &, texture &, const render_pass_desc &desc = {});
        void present();
        [[nodiscard]] bool valid() const noexcept { return active_; }

    private:
        friend class swapchain;
        friend class render_pass;
        explicit frame(swapchain &);
        void finish_without_present() noexcept;
        swapchain *swapchain_ = nullptr;
        render_pass *active_pass_ = nullptr;
        bool active_ = false;
    };
} // namespace alia

#endif
