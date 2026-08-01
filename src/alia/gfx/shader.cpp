#include "shader.hpp"

#include "texture.hpp"

#include <stdexcept>
#include <utility>

namespace alia {

    shader_program::shader_program(gfx_device &device, const shader_program_desc &desc) {
        if (!device)
            throw shader_error("shader_program: device is not valid");

        backend_ = device.backend();
        handle_ = backend_->create_shader_program.get_or_throw()(device.device(), desc);
        if (!handle_) {
            backend_ = nullptr;
            throw shader_error("shader_program: backend failed to create program");
        }
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

    shader_sampler shader_program::allocate_sampler(std::string_view identifier, shader_type stage) {
        if (!handle_)
            throw shader_error("shader_program::allocate_sampler: program is not valid");
        shader_sampler_slot slot =
            backend_->shader_lookup_sampler.get_or_throw()(handle_, identifier, stage);
        if (!slot.valid)
            throw shader_error("shader_program::allocate_sampler: shader sampler not found");
        return shader_sampler(backend_, handle_, slot);
    }

    void shader_sampler::set_texture(texture &tex) const {
        if (!*this)
            throw shader_error("shader_sampler::set_texture: invalid shader sampler");
        backend_->shader_set_sampler.get_or_throw()(program_, slot_, tex.impl());
    }

} // namespace alia
