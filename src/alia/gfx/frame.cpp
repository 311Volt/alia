#include "frame.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace alia {
    namespace {
        [[nodiscard]] vec2i mip_size(vec2i size, int level) {
            return {(std::max)(1, size.x >> level), (std::max)(1, size.y >> level)};
        }
        void validate_range(const char *name, int first, int count, int total) {
            if (first < 0 || count < 0 || first > total || count > total - first)
                throw std::out_of_range(std::string(name) + ": range is outside the buffer");
        }
    }

    frame::frame(swapchain &swapchain) : swapchain_(&swapchain) {
        if (!swapchain.handle_)
            throw std::logic_error("swapchain::begin_frame: swapchain is not valid");
        if (swapchain.frame_active_)
            throw std::logic_error("swapchain::begin_frame: a frame is already active");
        swapchain.backend_->swapchain_begin_frame.get_or_throw()(swapchain.handle_);
        swapchain.frame_active_ = true;
        active_ = true;
        try {
            set_target();
        } catch (...) {
            finish_without_present();
            throw;
        }
    }
    frame::~frame() { finish_without_present(); }
    frame::frame(frame &&other) noexcept
        : swapchain_(std::exchange(other.swapchain_, nullptr)), pipeline_(std::exchange(other.pipeline_, nullptr)),
          target_size_(other.target_size_), target_has_depth_(other.target_has_depth_),
          texture_bindings_(std::move(other.texture_bindings_)), active_(std::exchange(other.active_, false)) {}
    frame &frame::operator=(frame &&other) noexcept {
        if (this != &other) {
            finish_without_present();
            swapchain_ = std::exchange(other.swapchain_, nullptr);
            pipeline_ = std::exchange(other.pipeline_, nullptr);
            target_size_ = other.target_size_;
            target_has_depth_ = other.target_has_depth_;
            texture_bindings_ = std::move(other.texture_bindings_);
            active_ = std::exchange(other.active_, false);
        }
        return *this;
    }
    void frame::finish_without_present() noexcept {
        if (!active_)
            return;
        try {
            swapchain_->backend_->swapchain_end_frame.get_or_throw()(swapchain_->handle_);
        } catch (...) {
            // Destructors must not throw. The swapchain is still made reusable.
        }
        swapchain_->frame_active_ = false;
        pipeline_ = nullptr;
        texture_bindings_.clear();
        active_ = false;
        swapchain_ = nullptr;
    }
    void frame::present() {
        ensure_active();
        swapchain_->backend_->swapchain_end_frame.get_or_throw()(swapchain_->handle_);
        swapchain_->backend_->swapchain_present.get_or_throw()(swapchain_->handle_);
        swapchain_->frame_active_ = false;
        pipeline_ = nullptr;
        texture_bindings_.clear();
        active_ = false;
        swapchain_ = nullptr;
    }
    frame swapchain::begin_frame() { return frame(*this); }

    void frame::ensure_active() const {
        if (!active_ || !swapchain_)
            throw std::logic_error("frame: frame is not active");
    }
    void frame::validate_pipeline(pipeline &pipeline) const {
        if (!pipeline)
            throw std::invalid_argument("frame: pipeline is not valid");
        if (pipeline.backend() != swapchain_->backend_ || pipeline.device() != swapchain_->device_)
            throw std::invalid_argument("frame: pipeline belongs to another gfx_device");
        if (!target_has_depth_ && pipeline.depth_enabled())
            throw std::invalid_argument("frame: depth state requires a depth attachment");
    }
    void frame::unbind_render_target_source(texture_handle *target) {
        if (!target)
            return;
        for (auto it = texture_bindings_.begin(); it != texture_bindings_.end();) {
            if (it->second != target) {
                ++it;
                continue;
            }
            swapchain_->backend_->bind_resources.get_or_throw()(swapchain_->device_, {it->first, nullptr, {}});
            it = texture_bindings_.erase(it);
        }
    }
    void frame::set_target() {
        ensure_active();
        const render_target_info info{swapchain_->handle_, nullptr, 0, swapchain_->size_};
        if (!swapchain_->backend_->set_render_target.get_or_throw()(swapchain_->device_, info))
            throw std::runtime_error("frame::set_target: backend failed to select the swapchain backbuffer");
        target_size_ = info.target_size;
        target_has_depth_ = true;
    }
    void frame::set_target(texture &target, int level) {
        ensure_active();
        if (target.backend() != swapchain_->backend_ || target.device() != swapchain_->device_)
            throw std::invalid_argument("frame::set_target: texture belongs to another gfx_device");
        if (target.usage() != texture_usage::render_target)
            throw std::invalid_argument("frame::set_target: texture is not a render target");
        if (level < 0 || level >= target.mip_levels())
            throw std::out_of_range("frame::set_target: texture mip level out of range");
        if (pipeline_ && pipeline_->depth_enabled())
            throw std::invalid_argument("frame::set_target: depth state requires a depth attachment");
        unbind_render_target_source(target.impl());
        const render_target_info info{nullptr, target.impl(), level, mip_size(target.size(), level)};
        if (!swapchain_->backend_->set_render_target.get_or_throw()(swapchain_->device_, info))
            throw std::runtime_error("frame::set_target: render-to-texture may be unsupported on this device");
        target_size_ = info.target_size;
        target_has_depth_ = false;
    }
    void frame::clear(std::optional<color> color, std::optional<float> depth) {
        ensure_active();
        if (depth && !target_has_depth_)
            throw std::invalid_argument("frame::clear: current render target has no depth attachment");
        if (!color && !depth)
            return;
        if (!swapchain_->backend_->clear.get_or_throw()(swapchain_->device_, color, depth))
            throw std::runtime_error("frame::clear: backend clear failed");
    }
    void frame::set_pipeline(pipeline &pipeline) {
        ensure_active();
        validate_pipeline(pipeline);
        pipeline_ = &pipeline;
        swapchain_->backend_->bind_pipeline.get_or_throw()(swapchain_->device_, pipeline.impl());
    }
    void frame::set_texture(int slot, texture &texture) { set_texture(slot, texture, texture.sampler()); }
    void frame::set_texture(int slot, texture &texture, const sampler_state &sampler) {
        ensure_active();
        if (slot < 0)
            throw std::invalid_argument("frame::set_texture: slot must be non-negative");
        if (texture.backend() != swapchain_->backend_ || texture.device() != swapchain_->device_)
            throw std::invalid_argument("frame::set_texture: texture belongs to another gfx_device");
        swapchain_->backend_->bind_resources.get_or_throw()(swapchain_->device_, {slot, texture.impl(), sampler});
        texture_bindings_[slot] = texture.impl();
    }
    void frame::set_viewport(const render_viewport &viewport) {
        ensure_active();
        swapchain_->backend_->set_viewport.get_or_throw()(swapchain_->device_, viewport);
    }
    void frame::prepare_draw(const vertex_definition_view &definition) {
        ensure_active();
        if (!pipeline_)
            throw std::logic_error("frame::draw: a pipeline must be set before drawing");
        if (definition.stride <= 0)
            throw std::invalid_argument("frame: vertex definition has no layout");
        if (pipeline_->is_dynamic()) {
            auto &dynamic = static_cast<dynamic_pipeline &>(*pipeline_);
            if (dynamic.vertex_definition_index() != definition.index || dynamic.desc_.vertex_layout.stride == 0)
                dynamic.set_vertex_definition(definition);
            validate_pipeline(dynamic);
            swapchain_->backend_->bind_pipeline.get_or_throw()(swapchain_->device_, dynamic.impl());
        } else if (pipeline_->vertex_definition_index() != definition.index) {
            throw std::invalid_argument(
                "frame: vertex type does not match immutable pipeline (pipeline slot " +
                std::to_string(pipeline_->vertex_definition_index()) + ", draw slot " + std::to_string(definition.index) + ")");
        }
    }
    void frame::copy_to_texture(texture &dst, rect_i src_rect, vec2i dst_pos, int dst_level) {
        ensure_active();
        if (dst.backend() != swapchain_->backend_ || dst.device() != swapchain_->device_)
            throw std::invalid_argument("frame::copy_to_texture: texture belongs to another gfx_device");
        if (dst_level < 0 || dst_level >= dst.mip_levels())
            throw std::out_of_range("frame::copy_to_texture: destination mip level out of range");
        if (src_rect.width() <= 0 || src_rect.height() <= 0 || dst_pos.x < 0 || dst_pos.y < 0)
            throw std::invalid_argument("frame::copy_to_texture: invalid source rectangle or destination position");
        if (!rect_i{{}, target_size_}.contains(src_rect))
            throw std::invalid_argument("frame::copy_to_texture: source is outside the current target");
        if (!rect_i{{}, mip_size(dst.size(), dst_level)}.contains(rect_i::pos_size(dst_pos, src_rect.size())))
            throw std::invalid_argument("frame::copy_to_texture: destination is outside texture level");
        if (!swapchain_->backend_->copy_render_target_to_texture.get_or_throw()(swapchain_->device_, dst.impl(), src_rect, target_size_, dst_pos, dst_level))
            throw std::runtime_error("frame::copy_to_texture: backend copy failed");
    }

    namespace detail {
        void frame_draw_transient(frame &frame, const void *vertices, int vertex_count, const vertex_definition_view &definition, primitive_topology topology) {
            if (vertex_count <= 0)
                return;
            if (!vertices)
                throw std::invalid_argument("frame::draw: vertex data is null");
            frame.prepare_draw(definition);
            frame.swapchain_->backend_->upload_transient_vertex_data.get_or_throw()(frame.swapchain_->device_, vertices, vertex_count * definition.stride);
            frame.swapchain_->backend_->draw.get_or_throw()(frame.swapchain_->device_, topology, vertex_count, 0);
        }
        void frame_draw_buffered(frame &frame, vertex_buffer_handle *vertices, int vertex_count, int first_vertex, const vertex_definition_view &definition, primitive_topology topology) {
            if (vertex_count <= 0)
                return;
            if (!vertices)
                throw std::invalid_argument("frame::draw: vertex buffer is not valid");
            const int total = frame.swapchain_->backend_->vertex_buffer_count.get_or_throw()(vertices);
            validate_range("frame::draw", first_vertex, vertex_count, total);
            frame.prepare_draw(definition);
            frame.swapchain_->backend_->bind_vertex_buffer.get_or_throw()(frame.swapchain_->device_, vertices);
            frame.swapchain_->backend_->draw.get_or_throw()(frame.swapchain_->device_, topology, vertex_count, first_vertex);
        }
        void frame_draw_indexed_transient(frame &frame, const void *vertices, int vertex_count, std::span<const uint32_t> indices, const vertex_definition_view &definition, primitive_topology topology) {
            if (vertex_count <= 0 || indices.empty())
                return;
            if (!vertices)
                throw std::invalid_argument("frame::draw_indexed: vertex data is null");
            frame.prepare_draw(definition);
            frame.swapchain_->backend_->upload_transient_vertex_data.get_or_throw()(frame.swapchain_->device_, vertices, vertex_count * definition.stride);
            frame.swapchain_->backend_->upload_transient_index_data.get_or_throw()(frame.swapchain_->device_, indices);
            frame.swapchain_->backend_->draw_indexed.get_or_throw()(frame.swapchain_->device_, topology, static_cast<int>(indices.size()), 0, 0);
        }
        void frame_draw_indexed_buffered(frame &frame, vertex_buffer_handle *vertices, int vertex_count, index_buffer_handle *indices, int index_total, int first_index, int index_count, int base_vertex, const vertex_definition_view &definition, primitive_topology topology) {
            if (index_count < 0)
                index_count = index_total - first_index;
            if (vertex_count <= 0 || index_count <= 0)
                return;
            if (!vertices || !indices)
                throw std::invalid_argument("frame::draw_indexed: buffer is not valid");
            validate_range("frame::draw_indexed", first_index, index_count, index_total);
            if (base_vertex < 0 || base_vertex >= vertex_count)
                throw std::out_of_range("frame::draw_indexed: base vertex is outside the vertex buffer");
            frame.prepare_draw(definition);
            frame.swapchain_->backend_->bind_vertex_buffer.get_or_throw()(frame.swapchain_->device_, vertices);
            frame.swapchain_->backend_->bind_index_buffer.get_or_throw()(frame.swapchain_->device_, indices);
            frame.swapchain_->backend_->draw_indexed.get_or_throw()(frame.swapchain_->device_, topology, index_count, first_index, base_vertex);
        }
    } // namespace detail
} // namespace alia
