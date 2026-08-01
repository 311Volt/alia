#include "render_pass.hpp"

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
    }
    frame::~frame() { finish_without_present(); }
    frame::frame(frame &&other) noexcept
        : swapchain_(std::exchange(other.swapchain_, nullptr)), active_pass_(std::exchange(other.active_pass_, nullptr)),
          active_(std::exchange(other.active_, false)) {
        if (active_pass_)
            active_pass_->frame_ = this;
    }
    frame &frame::operator=(frame &&other) noexcept {
        if (this != &other) {
            finish_without_present();
            swapchain_ = std::exchange(other.swapchain_, nullptr);
            active_pass_ = std::exchange(other.active_pass_, nullptr);
            active_ = std::exchange(other.active_, false);
            if (active_pass_)
                active_pass_->frame_ = this;
        }
        return *this;
    }
    void frame::finish_without_present() noexcept {
        if (!active_)
            return;
        try {
            if (active_pass_)
                active_pass_->end();
            swapchain_->backend_->swapchain_end_frame.get_or_throw()(swapchain_->handle_);
        } catch (...) {
            // Destructors must not throw. The swapchain is still made reusable.
        }
        swapchain_->frame_active_ = false;
        active_pass_ = nullptr;
        active_ = false;
        swapchain_ = nullptr;
    }
    void frame::present() {
        if (!active_)
            throw std::logic_error("frame::present: frame is not active");
        if (active_pass_)
            throw std::logic_error("frame::present: render pass is still active");
        swapchain_->backend_->swapchain_end_frame.get_or_throw()(swapchain_->handle_);
        swapchain_->backend_->swapchain_present.get_or_throw()(swapchain_->handle_);
        swapchain_->frame_active_ = false;
        active_ = false;
        swapchain_ = nullptr;
    }
    frame swapchain::begin_frame() { return frame(*this); }

    render_pass::render_pass(frame &frame, pipeline &pipeline, const render_pass_begin_info &info, bool has_depth)
        : frame_(&frame), backend_(frame.swapchain_->backend()), device_(pipeline.device()),
          pipeline_(&pipeline), target_size_(info.target_size), has_depth_(has_depth) {
        validate_pipeline(pipeline);
        if (!backend_->begin_render_pass.get_or_throw()(device_, info))
            throw std::runtime_error("render_pass: render-to-texture may be unsupported on this device");
        try {
            backend_->bind_pipeline.get_or_throw()(device_, pipeline.impl());
            active_ = true;
        } catch (...) {
            backend_->end_render_pass.get_or_throw()(device_);
            throw;
        }
    }
    render_pass::~render_pass() { end(); }
    render_pass::render_pass(render_pass &&other) noexcept
        : frame_(std::exchange(other.frame_, nullptr)), backend_(std::exchange(other.backend_, nullptr)),
          device_(std::exchange(other.device_, nullptr)), pipeline_(std::exchange(other.pipeline_, nullptr)),
          target_size_(other.target_size_), has_depth_(other.has_depth_), active_(std::exchange(other.active_, false)) {
        if (frame_ && frame_->active_pass_ == &other)
            frame_->active_pass_ = this;
    }
    render_pass &render_pass::operator=(render_pass &&other) noexcept {
        if (this != &other) {
            end();
            frame_ = std::exchange(other.frame_, nullptr);
            backend_ = std::exchange(other.backend_, nullptr);
            device_ = std::exchange(other.device_, nullptr);
            pipeline_ = std::exchange(other.pipeline_, nullptr);
            target_size_ = other.target_size_;
            has_depth_ = other.has_depth_;
            active_ = std::exchange(other.active_, false);
            if (frame_ && frame_->active_pass_ == &other)
                frame_->active_pass_ = this;
        }
        return *this;
    }
    void render_pass::ensure_active() const {
        if (!active_ || !frame_ || !frame_->active_)
            throw std::logic_error("render_pass: pass is not active");
    }
    void render_pass::validate_pipeline(pipeline &pipeline) const {
        if (!pipeline)
            throw std::invalid_argument("render_pass: pipeline is not valid");
        if (pipeline.backend() != backend_ || pipeline.device() != device_)
            throw std::invalid_argument("render_pass: pipeline belongs to another gfx_device");
        if (!has_depth_ && pipeline.depth_enabled())
            throw std::invalid_argument("render_pass: depth state requires a depth attachment");
    }
    void render_pass::set_pipeline(pipeline &pipeline) {
        ensure_active();
        validate_pipeline(pipeline);
        pipeline_ = &pipeline;
        backend_->bind_pipeline.get_or_throw()(device_, pipeline.impl());
    }
    void render_pass::set_texture(int slot, texture &texture) { set_texture(slot, texture, texture.sampler()); }
    void render_pass::set_texture(int slot, texture &texture, const sampler_state &sampler) {
        ensure_active();
        if (slot < 0)
            throw std::invalid_argument("render_pass::set_texture: slot must be non-negative");
        if (texture.backend() != backend_ || texture.device() != device_)
            throw std::invalid_argument("render_pass::set_texture: texture belongs to another gfx_device");
        backend_->bind_resources.get_or_throw()(device_, {slot, texture.impl(), sampler});
    }
    void render_pass::set_viewport(const render_viewport &viewport) {
        ensure_active();
        backend_->set_viewport.get_or_throw()(device_, viewport);
    }
    void render_pass::prepare_draw(const vertex_definition_view &definition) {
        ensure_active();
        if (definition.stride <= 0)
            throw std::invalid_argument("render_pass: vertex definition has no layout");
        if (pipeline_->is_dynamic()) {
            auto &dynamic = static_cast<dynamic_pipeline &>(*pipeline_);
            if (dynamic.vertex_definition_index() != definition.index || dynamic.desc_.vertex_layout.stride == 0)
                dynamic.set_vertex_definition(definition);
            validate_pipeline(dynamic);
            backend_->bind_pipeline.get_or_throw()(device_, dynamic.impl());
        } else if (pipeline_->vertex_definition_index() != definition.index) {
            throw std::invalid_argument(
                "render_pass: vertex type does not match immutable pipeline (pipeline slot " +
                std::to_string(pipeline_->vertex_definition_index()) + ", draw slot " + std::to_string(definition.index) + ")");
        }
    }
    void render_pass::copy_to_texture(texture &dst, rect_i src_rect, vec2i dst_pos, int dst_level) {
        ensure_active();
        if (dst.backend() != backend_ || dst.device() != device_)
            throw std::invalid_argument("render_pass::copy_to_texture: texture belongs to another gfx_device");
        if (dst_level < 0 || dst_level >= dst.mip_levels())
            throw std::out_of_range("render_pass::copy_to_texture: destination mip level out of range");
        if (src_rect.width() <= 0 || src_rect.height() <= 0 || dst_pos.x < 0 || dst_pos.y < 0)
            throw std::invalid_argument("render_pass::copy_to_texture: invalid source rectangle or destination position");
        if (!rect_i{{}, target_size_}.contains(src_rect))
            throw std::invalid_argument("render_pass::copy_to_texture: source is outside the pass target");
        if (!rect_i{{}, mip_size(dst.size(), dst_level)}.contains(rect_i::pos_size(dst_pos, src_rect.size())))
            throw std::invalid_argument("render_pass::copy_to_texture: destination is outside texture level");
        if (!backend_->copy_render_target_to_texture.get_or_throw()(device_, dst.impl(), src_rect, target_size_, dst_pos, dst_level))
            throw std::runtime_error("render_pass::copy_to_texture: backend copy failed");
    }
    void render_pass::end() {
        if (!active_)
            return;
        backend_->end_render_pass.get_or_throw()(device_);
        active_ = false;
        if (frame_ && frame_->active_pass_ == this)
            frame_->active_pass_ = nullptr;
        frame_ = nullptr;
    }

    render_pass frame::begin_render_pass(pipeline &pipeline, const render_pass_desc &desc) {
        if (!active_)
            throw std::logic_error("frame::begin_render_pass: frame is not active");
        if (active_pass_)
            throw std::logic_error("frame::begin_render_pass: another pass is active");
        if (desc.level != 0)
            throw std::invalid_argument("frame::begin_render_pass: backbuffer has only level zero");
        render_pass_begin_info info{swapchain_->handle_, nullptr, 0, swapchain_->size_, desc.clear_color, desc.clear_depth};
        render_pass pass(*this, pipeline, info, true);
        active_pass_ = &pass;
        return pass;
    }
    render_pass frame::begin_render_pass(pipeline &pipeline, texture &target, const render_pass_desc &desc) {
        if (!active_)
            throw std::logic_error("frame::begin_render_pass: frame is not active");
        if (active_pass_)
            throw std::logic_error("frame::begin_render_pass: another pass is active");
        if (target.backend() != swapchain_->backend_ || target.device() != pipeline.device())
            throw std::invalid_argument("frame::begin_render_pass: texture belongs to another gfx_device");
        if (target.usage() != texture_usage::render_target)
            throw std::invalid_argument("frame::begin_render_pass: texture is not a render target");
        if (desc.level < 0 || desc.level >= target.mip_levels())
            throw std::out_of_range("frame::begin_render_pass: texture mip level out of range");
        if (desc.clear_depth)
            throw std::invalid_argument("frame::begin_render_pass: texture targets have no depth attachment");
        render_pass_begin_info info{nullptr, target.impl(), desc.level, mip_size(target.size(), desc.level), desc.clear_color, {}};
        render_pass pass(*this, pipeline, info, false);
        active_pass_ = &pass;
        return pass;
    }

    namespace detail {
        void pass_draw_transient(render_pass &pass, const void *vertices, int vertex_count, const vertex_definition_view &definition, primitive_topology topology) {
            if (vertex_count <= 0)
                return;
            if (!vertices)
                throw std::invalid_argument("render_pass::draw: vertex data is null");
            pass.prepare_draw(definition);
            pass.backend_->upload_transient_vertex_data.get_or_throw()(pass.device_, vertices, vertex_count * definition.stride);
            pass.backend_->draw.get_or_throw()(pass.device_, topology, vertex_count, 0);
        }
        void pass_draw_buffered(render_pass &pass, vertex_buffer_handle *vertices, int vertex_count, int first_vertex, const vertex_definition_view &definition, primitive_topology topology) {
            if (vertex_count <= 0)
                return;
            if (!vertices)
                throw std::invalid_argument("render_pass::draw: vertex buffer is not valid");
            const int total = pass.backend_->vertex_buffer_count.get_or_throw()(vertices);
            validate_range("render_pass::draw", first_vertex, vertex_count, total);
            pass.prepare_draw(definition);
            pass.backend_->bind_vertex_buffer.get_or_throw()(pass.device_, vertices);
            pass.backend_->draw.get_or_throw()(pass.device_, topology, vertex_count, first_vertex);
        }
        void pass_draw_indexed_transient(render_pass &pass, const void *vertices, int vertex_count, std::span<const uint32_t> indices, const vertex_definition_view &definition, primitive_topology topology) {
            if (vertex_count <= 0 || indices.empty())
                return;
            if (!vertices)
                throw std::invalid_argument("render_pass::draw_indexed: vertex data is null");
            pass.prepare_draw(definition);
            pass.backend_->upload_transient_vertex_data.get_or_throw()(pass.device_, vertices, vertex_count * definition.stride);
            pass.backend_->upload_transient_index_data.get_or_throw()(pass.device_, indices);
            pass.backend_->draw_indexed.get_or_throw()(pass.device_, topology, static_cast<int>(indices.size()), 0, 0);
        }
        void pass_draw_indexed_buffered(render_pass &pass, vertex_buffer_handle *vertices, int vertex_count, index_buffer_handle *indices, int index_total, int first_index, int index_count, int base_vertex, const vertex_definition_view &definition, primitive_topology topology) {
            if (index_count < 0)
                index_count = index_total - first_index;
            if (vertex_count <= 0 || index_count <= 0)
                return;
            if (!vertices || !indices)
                throw std::invalid_argument("render_pass::draw_indexed: buffer is not valid");
            validate_range("render_pass::draw_indexed", first_index, index_count, index_total);
            if (base_vertex < 0 || base_vertex >= vertex_count)
                throw std::out_of_range("render_pass::draw_indexed: base vertex is outside the vertex buffer");
            pass.prepare_draw(definition);
            pass.backend_->bind_vertex_buffer.get_or_throw()(pass.device_, vertices);
            pass.backend_->bind_index_buffer.get_or_throw()(pass.device_, indices);
            pass.backend_->draw_indexed.get_or_throw()(pass.device_, topology, index_count, first_index, base_vertex);
        }
    } // namespace detail
} // namespace alia
