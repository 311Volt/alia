#include "pipeline.hpp"

#include <stdexcept>
#include <utility>

namespace alia {
    namespace {
        const basic_effect &default_basic_effect() {
            static const basic_effect effect{};
            return effect;
        }
    }

    pipeline::pipeline(
        gfx_device &device, const pipeline_config &config,
        const vertex_definition_view &definition, bool dynamic)
        : backend_(device.backend()), device_(device.device()), dynamic_(dynamic) {
        if (!device)
            throw std::runtime_error("pipeline: device is not valid");
        desc_.vertex_layout = definition;
        desc_.blend = config.blend;
        desc_.depth = config.depth;
        desc_.raster = config.raster;
        resolve_effect(config.effect);
        handle_ = backend_->create_pipeline.get_or_throw()(device_, desc_);
        if (!handle_)
            throw std::runtime_error("pipeline: backend failed to create pipeline");
    }

    pipeline::~pipeline() {
        if (handle_)
            backend_->destroy_pipeline.get_or_throw()(handle_);
    }
    pipeline::pipeline(pipeline &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)), backend_(std::exchange(other.backend_, nullptr)),
          device_(std::exchange(other.device_, nullptr)), desc_(other.desc_), dynamic_(other.dynamic_) {}
    pipeline &pipeline::operator=(pipeline &&other) noexcept {
        if (this != &other) {
            if (handle_)
                backend_->destroy_pipeline.get_or_throw()(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
            backend_ = std::exchange(other.backend_, nullptr);
            device_ = std::exchange(other.device_, nullptr);
            desc_ = other.desc_;
            dynamic_ = other.dynamic_;
        }
        return *this;
    }

    void pipeline::resolve_effect(std::variant<const basic_effect *, shader_program *> effect) {
        if (const auto *basic = std::get_if<const basic_effect *>(&effect)) {
            desc_.shader = nullptr;
            desc_.effect = *basic ? *basic : &default_basic_effect();
            return;
        }
        shader_program *program = std::get<shader_program *>(effect);
        if (!program || !*program)
            throw std::invalid_argument("pipeline: shader program is not valid");
        if (program->backend() != backend_)
            throw std::invalid_argument("pipeline: shader program belongs to another gfx_device");
        desc_.shader = program->impl();
        desc_.effect = nullptr;
    }
    void pipeline::update_desc() {
        if (!handle_)
            throw std::logic_error("pipeline: pipeline is not valid");
        backend_->update_pipeline.get_or_throw()(handle_, desc_);
    }

    dynamic_pipeline::dynamic_pipeline(gfx_device &device, const pipeline_config &config)
        : pipeline(device, config, {}, true) {}
    void dynamic_pipeline::set_vertex_definition(const vertex_definition_view &definition) {
        desc_.vertex_layout = definition;
        update_desc();
    }
    void dynamic_pipeline::set_effect(std::variant<const basic_effect *, shader_program *> effect) {
        resolve_effect(effect);
        update_desc();
    }
    void dynamic_pipeline::set_blend(const blend_state &state) { desc_.blend = state; update_desc(); }
    void dynamic_pipeline::set_depth(const depth_state &state) { desc_.depth = state; update_desc(); }
    void dynamic_pipeline::set_raster(const raster_state &state) { desc_.raster = state; update_desc(); }
} // namespace alia
