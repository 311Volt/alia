#include "shader.hpp"

#include "cube_texture.hpp"
#include "texture.hpp"

#include <stdexcept>
#include <utility>

namespace alia {

    shader_program::shader_program(gfx_device &device, const shader_program_desc &desc) {
        if (!device)
            throw shader_error("shader_program: device is not valid");
        for (std::size_t i = 0; i < desc.sources.size(); ++i) {
            const auto &program_source = desc.sources[i];
            for (const auto *stage : {&program_source.vertex, &program_source.pixel}) {
                if (stage->source.empty() == stage->bytecode.empty())
                    throw shader_error("shader_program: each stage must provide exactly one of source text and bytecode");
            }
            for (std::size_t j = 0; j < i; ++j) {
                if (desc.sources[j].backend == program_source.backend)
                    throw shader_error("shader_program: multiple sources for the same backend");
            }
        }

        backend_ = device.backend();
        handle_ = backend_->create_shader_program.get_or_throw()(device.device(), desc);
        if (!handle_) {
            backend_ = nullptr;
            throw shader_error("shader_program: backend failed to create program");
        }
    }

    shader_program::shader_program(
        gfx_device &device,
        std::string_view debug_name,
        const std::vector<shader_program_source> &sources
    )
        : shader_program(device, {.sources = sources, .debug_name = debug_name}) {
    }

    shader_program::~shader_program() {
        if (handle_)
            backend_->destroy_shader_program.get_or_throw()(handle_);
    }

    shader_program::shader_program(shader_program &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr))
        , backend_(std::exchange(other.backend_, nullptr)) {
    }

    shader_program &shader_program::operator=(shader_program &&other) noexcept {
        if (this != &other) {
            if (handle_)
                backend_->destroy_shader_program.get_or_throw()(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
            backend_ = std::exchange(other.backend_, nullptr);
        }
        return *this;
    }

    shader_sampler shader_program::allocate_sampler(std::string_view identifier, int unit, shader_type stage) {
        if (!handle_)
            throw shader_error("shader_program::allocate_sampler: program is not valid");
        if (unit < 0)
            throw shader_error("shader_program::allocate_sampler: negative texture unit for '" +
                               std::string(identifier) + "'");
        shader_sampler_slot slot =
            backend_->shader_lookup_sampler.get_or_throw()(handle_, identifier, stage, unit);
        if (!slot.valid)
            throw shader_error("shader_program::allocate_sampler: shader sampler '" +
                               std::string(identifier) + "' not found");
        if (slot.slot != unit)
            throw shader_error("shader_program::allocate_sampler: register mismatch for '" +
                               std::string(identifier) + "': shader declares s" +
                               std::to_string(slot.slot) + ", requested unit " + std::to_string(unit));
        return shader_sampler(backend_, handle_, slot);
    }

    void shader_sampler::set_texture(texture &tex) const {
        if (!*this)
            throw shader_error("shader_sampler::set_texture: invalid shader sampler");
        backend_->shader_set_sampler.get_or_throw()(program_, slot_, tex.impl());
    }

    void shader_sampler::set_texture(cube_texture &tex) const {
        if (!*this)
            throw shader_error("shader_sampler::set_texture: invalid shader sampler");
        backend_->shader_set_sampler.get_or_throw()(program_, slot_, tex.impl());
    }

} // namespace alia
