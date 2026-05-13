#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"

#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <string>

namespace alia {

    namespace {

        template <typename T>
        class com_ptr {
        public:
            com_ptr() = default;
            ~com_ptr() {
                reset();
            }
            com_ptr(com_ptr &&other) noexcept
                : ptr_(other.ptr_) {
                other.ptr_ = nullptr;
            }
            com_ptr &operator=(com_ptr &&other) noexcept {
                if (this != &other) {
                    reset();
                    ptr_ = other.ptr_;
                    other.ptr_ = nullptr;
                }
                return *this;
            }
            com_ptr(const com_ptr &) = delete;
            com_ptr &operator=(const com_ptr &) = delete;

            T **put() noexcept {
                reset();
                return &ptr_;
            }
            T *get() const noexcept {
                return ptr_;
            }
            T *detach() noexcept {
                T *p = ptr_;
                ptr_ = nullptr;
                return p;
            }
            void reset(T *p = nullptr) noexcept {
                if (ptr_)
                    ptr_->Release();
                ptr_ = p;
            }

        private:
            T *ptr_ = nullptr;
        };

        const shader_source *
        select_source(const shader_program_desc &desc, shader_type type) {
            for (const auto &src : desc.sources) {
                if (src.backend == gfx_backend::d3d9 && src.type == type)
                    return &src;
            }
            for (const auto &src : desc.sources) {
                if (src.backend == gfx_backend::auto_ && src.type == type)
                    return &src;
            }
            return nullptr;
        }

        std::string blob_text(ID3DBlob *blob) {
            if (!blob || !blob->GetBufferPointer() || blob->GetBufferSize() == 0)
                return {};
            return std::string(
                static_cast<const char *>(blob->GetBufferPointer()),
                static_cast<std::size_t>(blob->GetBufferSize())
            );
        }

        const char *default_profile(const d3d9_device &dev, shader_type type) {
            const DWORD version =
                type == shader_type::vertex ? dev.caps.VertexShaderVersion : dev.caps.PixelShaderVersion;
            const int major = D3DSHADER_VERSION_MAJOR(version);
            if (type == shader_type::vertex)
                return major >= 3 ? "vs_3_0" : "vs_2_0";
            return major >= 3 ? "ps_3_0" : "ps_2_0";
        }

        com_ptr<ID3DBlob> compile_shader_source(const d3d9_device &dev, const shader_source &source) {
            const std::string source_text(source.source);
            const std::string entry = source.entry_point.empty() ? "main" : std::string(source.entry_point);
            const std::string profile =
                source.profile.empty() ? default_profile(dev, source.type) : std::string(source.profile);
            const std::string debug_name =
                source.debug_name.empty() ? "alia shader" : std::string(source.debug_name);

            UINT flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#ifndef NDEBUG
            flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

            com_ptr<ID3DBlob> code;
            com_ptr<ID3DBlob> errors;
            const HRESULT hr = D3DCompile(
                source_text.data(), source_text.size(),
                debug_name.c_str(),
                nullptr, nullptr,
                entry.c_str(), profile.c_str(),
                flags, 0,
                code.put(), errors.put()
            );
            if (FAILED(hr)) {
                std::string message = "D3D9 shader compile failed for " + debug_name +
                    " (" + profile + ")";
                const std::string log = blob_text(errors.get());
                if (!log.empty())
                    message += ":\n" + log;
                throw shader_error(message);
            }
            return code;
        }

        void apply_d3d9_sampler_state(IDirect3DDevice9 *device, DWORD stage, const sampler_state &s) {
            auto filt = [](texture_filter f) -> DWORD {
                return f == texture_filter::nearest ? D3DTEXF_POINT : D3DTEXF_LINEAR;
            };
            auto addr = [](texture_wrap w) -> DWORD {
                switch (w) {
                case texture_wrap::clamp:  return D3DTADDRESS_CLAMP;
                case texture_wrap::repeat: return D3DTADDRESS_WRAP;
                case texture_wrap::mirror: return D3DTADDRESS_MIRROR;
                }
                return D3DTADDRESS_CLAMP;
            };
            device->SetSamplerState(stage, D3DSAMP_MINFILTER, filt(s.min_filter));
            device->SetSamplerState(stage, D3DSAMP_MAGFILTER, filt(s.mag_filter));
            device->SetSamplerState(stage, D3DSAMP_MIPFILTER, filt(s.mip_filter));
            device->SetSamplerState(stage, D3DSAMP_ADDRESSU, addr(s.wrap_u));
            device->SetSamplerState(stage, D3DSAMP_ADDRESSV, addr(s.wrap_v));
        }

        int register_count_for(shader_constant_value_type type) {
            return type == shader_constant_value_type::matrix_4x4 ? 4 : 1;
        }

        void apply_constant(IDirect3DDevice9 *device, const d3d9_stored_shader_constant &c) {
            const UINT start = static_cast<UINT>(c.slot.location);
            const UINT count = static_cast<UINT>(register_count_for(c.type));

            if (!c.floats.empty()) {
                std::array<float, 16> data = {};
                std::copy(c.floats.begin(), c.floats.end(), data.begin());
                if (c.slot.stage == shader_type::vertex)
                    device->SetVertexShaderConstantF(start, data.data(), count);
                else
                    device->SetPixelShaderConstantF(start, data.data(), count);
            } else if (!c.ints.empty()) {
                std::array<int, 4> data = {};
                std::copy(c.ints.begin(), c.ints.end(), data.begin());
                if (c.slot.stage == shader_type::vertex)
                    device->SetVertexShaderConstantI(start, data.data(), 1);
                else
                    device->SetPixelShaderConstantI(start, data.data(), 1);
            }
        }

        void bind_texture_slot(IDirect3DDevice9 *device, int slot, texture_handle *texture) {
            auto *d3d_texture = texture ? as_d3d9_texture(texture) : nullptr;
            device->SetTexture(
                static_cast<DWORD>(slot),
                d3d_texture ? d3d_texture->texture : nullptr
            );
            if (d3d_texture)
                apply_d3d9_sampler_state(device, static_cast<DWORD>(slot), d3d_texture->sampler);
        }

    } // namespace

    shader_program_handle *d3d9_create_shader_program(device_handle *dev_h, const shader_program_desc &desc) {
        auto *dev = as_d3d9_device(dev_h);
        const shader_source *vertex_source = select_source(desc, shader_type::vertex);
        const shader_source *pixel_source = select_source(desc, shader_type::pixel);
        if (!vertex_source || !pixel_source)
            throw shader_error("D3D9 shader program requires matching vertex and pixel sources");

        com_ptr<ID3DBlob> vertex_code = compile_shader_source(*dev, *vertex_source);
        com_ptr<ID3DBlob> pixel_code = compile_shader_source(*dev, *pixel_source);

        com_ptr<IDirect3DVertexShader9> vertex_shader;
        if (FAILED(dev->device->CreateVertexShader(
                static_cast<const DWORD *>(vertex_code.get()->GetBufferPointer()),
                vertex_shader.put()
            )))
            throw shader_error("D3D9 shader program failed to create vertex shader");

        com_ptr<IDirect3DPixelShader9> pixel_shader;
        if (FAILED(dev->device->CreatePixelShader(
                static_cast<const DWORD *>(pixel_code.get()->GetBufferPointer()),
                pixel_shader.put()
            )))
            throw shader_error("D3D9 shader program failed to create pixel shader");

        auto *program = new d3d9_shader_program;
        program->device = dev->device;
        program->vertex_shader = vertex_shader.detach();
        program->pixel_shader = pixel_shader.detach();

        for (const auto &binding : desc.constant_bindings) {
            if (binding.name.empty() || binding.index < 0)
                continue;
            program->constants.emplace(
                std::string(binding.name),
                shader_constant_slot{true, binding.stage, binding.index, binding.count}
            );
        }
        for (const auto &binding : desc.sampler_bindings) {
            if (binding.name.empty() || binding.slot < 0)
                continue;
            program->samplers.emplace(
                std::string(binding.name),
                shader_sampler_slot{true, binding.stage, binding.slot, binding.slot}
            );
        }

        return program;
    }

    void d3d9_destroy_shader_program(shader_program_handle *h) {
        auto *program = as_d3d9_shader_program(h);
        if (program->vertex_shader)
            program->vertex_shader->Release();
        if (program->pixel_shader)
            program->pixel_shader->Release();
        delete program;
    }

    shader_constant_slot
    d3d9_shader_lookup_constant(shader_program_handle *h, std::string_view name, shader_type stage) {
        auto *program = as_d3d9_shader_program(h);
        auto it = program->constants.find(std::string(name));
        if (it == program->constants.end() || it->second.stage != stage)
            return {};
        return it->second;
    }

    void d3d9_shader_set_constant(
        shader_program_handle *h,
        const shader_constant_slot &slot,
        const shader_constant_payload &payload
    ) {
        auto *program = as_d3d9_shader_program(h);

        auto it = std::find_if(
            program->stored_constants.begin(),
            program->stored_constants.end(),
            [&](const d3d9_stored_shader_constant &c) {
                return c.slot.stage == slot.stage && c.slot.location == slot.location;
            }
        );
        if (it == program->stored_constants.end()) {
            program->stored_constants.push_back({});
            it = std::prev(program->stored_constants.end());
        }

        it->slot = slot;
        it->type = payload.type;
        it->floats.assign(payload.floats.begin(), payload.floats.end());
        it->ints.assign(payload.ints.begin(), payload.ints.end());
    }

    shader_sampler_slot
    d3d9_shader_lookup_sampler(shader_program_handle *h, std::string_view name, shader_type stage) {
        auto *program = as_d3d9_shader_program(h);
        auto it = program->samplers.find(std::string(name));
        if (it == program->samplers.end() || it->second.stage != stage)
            return {};
        return it->second;
    }

    void d3d9_shader_set_sampler(shader_program_handle *h, const shader_sampler_slot &slot, texture_handle *tex) {
        as_d3d9_shader_program(h)->sampler_textures[slot.slot] = tex;
    }

    void d3d9_apply_shader_program(shader_program_handle *h, texture_handle *primary_texture) {
        if (!h)
            return;

        auto *program = as_d3d9_shader_program(h);
        auto *device = program->device;

        device->SetVertexShader(program->vertex_shader);
        device->SetPixelShader(program->pixel_shader);

        for (const auto &constant : program->stored_constants)
            apply_constant(device, constant);

        for (const auto &[slot, texture] : program->sampler_textures)
            bind_texture_slot(device, slot, texture);
        if (primary_texture)
            bind_texture_slot(device, 0, primary_texture);
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
