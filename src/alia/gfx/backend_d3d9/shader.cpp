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

        const shader_program_source *select_source(const shader_program_desc &desc) {
            for (const auto &src : desc.sources) {
                if (src.backend == gfx_backend::d3d9)
                    return &src;
            }
            for (const auto &src : desc.sources) {
                if (src.backend == gfx_backend::auto_)
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

        com_ptr<ID3DBlob> compile_shader_source(
            const d3d9_device &dev,
            const shader_stage_source &source,
            shader_type type,
            std::string_view program_name
        ) {
            const std::string source_text(source.source);
            const std::string entry = source.entry_point.empty() ? "main" : std::string(source.entry_point);
            const std::string profile =
                source.profile.empty() ? default_profile(dev, type) : std::string(source.profile);
            const std::string debug_name = detail::shader_stage_debug_name(program_name, type);

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

        std::vector<DWORD> load_shader_code(
            const d3d9_device &dev,
            const shader_stage_source &source,
            shader_type type,
            std::string_view program_name
        ) {
            com_ptr<ID3DBlob> compiled;
            std::span<const std::byte> bytes = source.bytecode;
            if (bytes.empty()) {
                compiled = compile_shader_source(dev, source, type, program_name);
                bytes = {static_cast<const std::byte *>(compiled.get()->GetBufferPointer()),
                         static_cast<std::size_t>(compiled.get()->GetBufferSize())};
            }
            if (bytes.empty() || bytes.size() % sizeof(DWORD) != 0)
                throw shader_error("D3D9 shader bytecode size must be a non-zero multiple of 4");

            // Own an aligned copy even when the caller's byte span is unaligned.
            std::vector<DWORD> code(bytes.size() / sizeof(DWORD));
            std::memcpy(code.data(), bytes.data(), bytes.size());
            const DWORD expected_stage = type == shader_type::vertex ? 0xFFFE : 0xFFFF;
            if ((code.front() >> 16) != expected_stage)
                throw shader_error("D3D9 shader bytecode version token does not match the shader stage");
            return code;
        }

        struct ctab_header {
            std::uint32_t size, creator, version, constants, constant_info, flags, target;
        };
        struct ctab_constant_info {
            std::uint32_t name;
            std::uint16_t register_set, register_index, register_count, reserved;
            std::uint32_t type_info, default_value;
        };
        struct ctab_type_info {
            std::uint16_t class_, type, rows, columns, elements, struct_members;
            std::uint32_t struct_member_info;
        };
        struct ctab_struct_member_info {
            std::uint32_t name, type_info;
        };
        static_assert(sizeof(ctab_header) == 28);
        static_assert(sizeof(ctab_constant_info) == 20);
        static_assert(sizeof(ctab_type_info) == 16);
        static_assert(sizeof(ctab_struct_member_info) == 8);

        enum class ctab_class : std::uint16_t {
            scalar = 0, vector = 1, matrix_rows = 2, matrix_columns = 3, object = 4, struct_ = 5
        };
        enum class ctab_type : std::uint16_t {
            int_ = 2, float_ = 3,
            sampler = 10, sampler_1d = 11, sampler_2d = 12, sampler_3d = 13, sampler_cube = 14
        };

        class ctab_reader {
        public:
            explicit ctab_reader(std::span<const std::byte> data) : data_(data) {}

            void check_range(std::size_t offset, std::size_t count, std::size_t stride = 1) const {
                if (offset > data_.size() || count > (data_.size() - offset) / stride)
                    throw shader_error("D3D9 shader has malformed CTAB data: offset or size is out of bounds");
            }

            template <typename T>
            T read(std::size_t offset) const {
                check_range(offset, sizeof(T));
                T value;
                std::memcpy(&value, data_.data() + offset, sizeof(T));
                return value;
            }

            std::string string(std::size_t offset) const {
                check_range(offset, 1);
                const char *begin = reinterpret_cast<const char *>(data_.data() + offset);
                const auto *end = static_cast<const char *>(std::memchr(begin, '\0', data_.size() - offset));
                if (!end)
                    throw shader_error("D3D9 shader has malformed CTAB data: unterminated string");
                return {begin, end};
            }

            ctab_type_info read_type(std::size_t offset) const {
                std::vector<std::size_t> ancestors;
                return read_type(offset, ancestors);
            }

        private:
            ctab_type_info read_type(std::size_t offset, std::vector<std::size_t> &ancestors) const {
                // Structs are unsupported constants, but their referenced data
                // still needs validation before accepting the bytecode.
                if (ancestors.size() >= 64 ||
                    std::find(ancestors.begin(), ancestors.end(), offset) != ancestors.end())
                    throw shader_error("D3D9 shader has malformed CTAB data: recursive type information");
                const auto type = read<ctab_type_info>(offset);
                if (type.struct_member_info || type.struct_members) {
                    check_range(type.struct_member_info, type.struct_members, sizeof(ctab_struct_member_info));
                    ancestors.push_back(offset);
                    for (std::size_t i = 0; i < type.struct_members; ++i) {
                        const auto member = read<ctab_struct_member_info>(
                            type.struct_member_info + i * sizeof(ctab_struct_member_info));
                        string(member.name);
                        read_type(member.type_info, ancestors);
                    }
                    ancestors.pop_back();
                }
                return type;
            }

            std::span<const std::byte> data_;
        };

        std::optional<shader_constant_value_type> reflected_type(
            const ctab_type_info &type, d3d9_shader_register_set register_set
        ) {
            if (type.elements != 1 || type.struct_members != 0)
                return {};
            const auto class_ = static_cast<ctab_class>(type.class_);
            const auto scalar_type = static_cast<ctab_type>(type.type);
            if ((class_ == ctab_class::scalar || class_ == ctab_class::vector) &&
                type.rows == 1 && type.columns >= 1 && type.columns <= 4 &&
                (class_ != ctab_class::scalar || type.columns == 1)) {
                if (scalar_type == ctab_type::float_ && register_set == d3d9_shader_register_set::float4) {
                    constexpr std::array types{
                        shader_constant_value_type::float_1, shader_constant_value_type::float_2,
                        shader_constant_value_type::float_3, shader_constant_value_type::float_4};
                    return types[type.columns - 1];
                }
                if (scalar_type == ctab_type::int_ &&
                    (register_set == d3d9_shader_register_set::int4 ||
                     register_set == d3d9_shader_register_set::float4)) {
                    constexpr std::array types{
                        shader_constant_value_type::int_1, shader_constant_value_type::int_2,
                        shader_constant_value_type::int_3, shader_constant_value_type::int_4};
                    return types[type.columns - 1];
                }
            }
            if ((class_ == ctab_class::matrix_rows || class_ == ctab_class::matrix_columns) &&
                scalar_type == ctab_type::float_ && type.rows == 4 && type.columns == 4 &&
                register_set == d3d9_shader_register_set::float4)
                return shader_constant_value_type::matrix_4x4;
            return {};
        }

        d3d9_shader_reflection parse_ctab(std::span<const std::byte> data) {
            const ctab_reader reader(data);
            const auto header = reader.read<ctab_header>(0);
            if (header.size != sizeof(ctab_header))
                throw shader_error("D3D9 shader has malformed CTAB data: invalid header size");
            if (header.creator)
                reader.string(header.creator);
            if (header.target)
                reader.string(header.target);
            reader.check_range(header.constant_info, header.constants, sizeof(ctab_constant_info));

            d3d9_shader_reflection reflection;
            reflection.reflected = true;
            reflection.constants.reserve(header.constants);
            for (std::size_t i = 0; i < header.constants; ++i) {
                const auto info = reader.read<ctab_constant_info>(
                    header.constant_info + i * sizeof(ctab_constant_info));
                if (info.register_set > static_cast<std::uint16_t>(d3d9_shader_register_set::sampler))
                    throw shader_error("D3D9 shader has malformed CTAB data: invalid register set");
                const auto register_set = static_cast<d3d9_shader_register_set>(info.register_set);
                const auto type = reader.read_type(info.type_info);
                if (info.default_value) {
                    const std::size_t register_bytes =
                        register_set == d3d9_shader_register_set::bool_ ||
                        register_set == d3d9_shader_register_set::sampler ? 4 : 16;
                    reader.check_range(info.default_value, info.register_count, register_bytes);
                }
                reflection.constants.push_back({
                    reader.string(info.name), register_set,
                    info.register_index, info.register_count, reflected_type(type, register_set)});
            }
            return reflection;
        }

        d3d9_shader_reflection reflect_shader(std::span<const DWORD> code) {
            d3d9_shader_reflection reflection;
            for (std::size_t position = 1; position < code.size();) {
                const DWORD token = code[position++];
                if (token == 0x0000FFFF) // END
                    return reflection;
                if ((token & 0xFFFF) == 0xFFFE) { // COMMENT
                    const std::size_t length = (token >> 16) & 0x7FFF;
                    if (length > code.size() - position)
                        throw shader_error("D3D9 shader has malformed bytecode: truncated comment");
                    if (length != 0 && code[position] == 0x42415443) { // FOURCC 'CTAB'
                        if (reflection.reflected)
                            throw shader_error("D3D9 shader has malformed bytecode: duplicate CTAB comments");
                        reflection = parse_ctab(std::as_bytes(code.subspan(position + 1, length - 1)));
                    }
                    position += length;
                } else if (D3DSHADER_VERSION_MAJOR(code.front()) >= 2) {
                    // Skip operands and literal data instead of interpreting
                    // them as comment tokens. SM2+ encodes their DWORD count.
                    const std::size_t length = (token >> 24) & 0xF;
                    if (length > code.size() - position)
                        throw shader_error("D3D9 shader has malformed bytecode: truncated instruction");
                    position += length;
                }
            }
            throw shader_error("D3D9 shader has malformed bytecode: missing END token");
        }

        d3d9_shader_reflection &stage_reflection(d3d9_shader_program &program, shader_type stage) {
            return stage == shader_type::vertex ? program.vertex_reflection : program.pixel_reflection;
        }

        void apply_d3d9_sampler_state(
            IDirect3DDevice9 *device,
            DWORD stage,
            const d3d9_texture &texture,
            const sampler_state &s
        ) {
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
            if (texture.cube)
                device->SetSamplerState(stage, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
        }

        int register_count_for(shader_constant_value_type type) {
            return type == shader_constant_value_type::matrix_4x4 ? 4 : 1;
        }

        void apply_constant(IDirect3DDevice9 *device, const d3d9_stored_shader_constant &c) {
            const UINT start = static_cast<UINT>(c.register_index);
            const UINT count = static_cast<UINT>(std::min(c.register_count, register_count_for(c.type)));

            if (c.register_set == d3d9_shader_register_set::float4) {
                std::array<float, 16> data = {};
                if (!c.floats.empty())
                    std::copy(c.floats.begin(), c.floats.end(), data.begin());
                else
                    std::transform(c.ints.begin(), c.ints.end(), data.begin(),
                                   [](int value) { return static_cast<float>(value); });
                if (c.slot.stage == shader_type::vertex)
                    device->SetVertexShaderConstantF(start, data.data(), count);
                else
                    device->SetPixelShaderConstantF(start, data.data(), count);
            } else if (c.register_set == d3d9_shader_register_set::int4) {
                std::array<int, 4> data = {};
                std::copy(c.ints.begin(), c.ints.end(), data.begin());
                if (c.slot.stage == shader_type::vertex)
                    device->SetVertexShaderConstantI(start, data.data(), count);
                else
                    device->SetPixelShaderConstantI(start, data.data(), count);
            }
        }

        void bind_texture_slot(IDirect3DDevice9 *device, int slot, texture_handle *texture) {
            auto *d3d_texture = texture ? as_d3d9_texture(texture) : nullptr;
            device->SetTexture(
                static_cast<DWORD>(slot),
                d3d_texture ? d3d9_base_texture(*d3d_texture) : nullptr
            );
            if (d3d_texture)
                apply_d3d9_sampler_state(
                    device, static_cast<DWORD>(slot), *d3d_texture,
                    d3d_texture->sampler);
        }

    } // namespace

    shader_program_handle *d3d9_create_shader_program(device_handle *dev_h, const shader_program_desc &desc) {
        auto *dev = as_d3d9_device(dev_h);
        const shader_program_source *source = select_source(desc);
        if (!source)
            throw shader_error("D3D9 shader program has no source for the d3d9 backend");

        const auto vertex_code = load_shader_code(*dev, source->vertex, shader_type::vertex, desc.debug_name);
        const auto pixel_code = load_shader_code(*dev, source->pixel, shader_type::pixel, desc.debug_name);
        auto vertex_reflection = reflect_shader(vertex_code);
        auto pixel_reflection = reflect_shader(pixel_code);

        com_ptr<IDirect3DVertexShader9> vertex_shader;
        if (FAILED(dev->device->CreateVertexShader(
                vertex_code.data(),
                vertex_shader.put()
            )))
            throw shader_error("D3D9 shader program failed to create vertex shader");

        com_ptr<IDirect3DPixelShader9> pixel_shader;
        if (FAILED(dev->device->CreatePixelShader(
                pixel_code.data(),
                pixel_shader.put()
            )))
            throw shader_error("D3D9 shader program failed to create pixel shader");

        auto program = std::make_unique<d3d9_shader_program>();
        program->device = dev->device;
        program->vertex_reflection = std::move(vertex_reflection);
        program->pixel_reflection = std::move(pixel_reflection);
        program->vertex_shader = vertex_shader.detach();
        program->pixel_shader = pixel_shader.detach();

        return program.release();
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
    d3d9_shader_lookup_constant(
        shader_program_handle *h,
        std::string_view name,
        shader_type stage,
        std::optional<shader_register> explicit_register
    ) {
        auto *program = as_d3d9_shader_program(h);
        auto &reflection = stage_reflection(*program, stage);
        if (reflection.reflected) {
            auto it = std::find_if(
                reflection.constants.begin(), reflection.constants.end(),
                [&](const d3d9_reflected_constant &constant) {
                    return constant.name == name && constant.register_set != d3d9_shader_register_set::sampler;
                });
            if (it == reflection.constants.end() || it->register_count == 0)
                return {};
            if (!it->type || it->register_set == d3d9_shader_register_set::bool_)
                throw shader_error("D3D9 shader constant '" + std::string(name) +
                                   "' has an unsupported declared type");
            if (explicit_register) {
                const auto register_set = explicit_register->kind == shader_register::kind::float4
                    ? d3d9_shader_register_set::float4 : d3d9_shader_register_set::int4;
                if (register_set != it->register_set || explicit_register->index != it->register_index)
                    throw shader_error("D3D9 shader constant '" + std::string(name) +
                                       "': explicit register disagrees with reflection: shader declares " +
                                       (it->register_set == d3d9_shader_register_set::float4 ? "c" : "i") +
                                       std::to_string(it->register_index) + ", requested " +
                                       (explicit_register->kind == shader_register::kind::float4 ? "c" : "i") +
                                       std::to_string(explicit_register->index));
            }
            return {true, stage, static_cast<int>(it - reflection.constants.begin()), it->type};
        }

        if (!explicit_register)
            throw shader_error("D3D9 shader constant '" + std::string(name) +
                               "': no reflection data; pass an explicit shader_register");
        const auto register_set = explicit_register->kind == shader_register::kind::float4
            ? d3d9_shader_register_set::float4 : d3d9_shader_register_set::int4;
        auto it = std::find_if(
            reflection.constants.begin(), reflection.constants.end(),
            [&](const d3d9_reflected_constant &constant) {
                return constant.register_set == register_set &&
                       constant.register_index == explicit_register->index;
            });
        if (it == reflection.constants.end()) {
            reflection.constants.push_back({std::string(name), register_set, explicit_register->index, 0, {}});
            it = std::prev(reflection.constants.end());
        }
        return {true, stage, static_cast<int>(it - reflection.constants.begin()), {}};
    }

    void d3d9_shader_set_constant(
        shader_program_handle *h,
        const shader_constant_slot &slot,
        const shader_constant_payload &payload
    ) {
        auto *program = as_d3d9_shader_program(h);
        auto &reflection = stage_reflection(*program, slot.stage);
        auto &constant = reflection.constants.at(static_cast<std::size_t>(slot.location));
        if (!reflection.reflected)
            constant.register_count = register_count_for(payload.type);

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
        it->register_set = constant.register_set;
        it->register_index = constant.register_index;
        it->register_count = constant.register_count;
        it->type = payload.type;
        it->floats.assign(payload.floats.begin(), payload.floats.end());
        it->ints.assign(payload.ints.begin(), payload.ints.end());
    }

    shader_sampler_slot
    d3d9_shader_lookup_sampler(shader_program_handle *h, std::string_view name, shader_type stage, int unit) {
        auto *program = as_d3d9_shader_program(h);
        const auto &reflection = stage_reflection(*program, stage);
        if (!reflection.reflected)
            return {true, stage, unit, unit};
        auto it = std::find_if(
            reflection.constants.begin(), reflection.constants.end(),
            [&](const d3d9_reflected_constant &constant) {
                return constant.name == name && constant.register_set == d3d9_shader_register_set::sampler;
            });
        if (it == reflection.constants.end() || it->register_count == 0)
            return {};
        return {true, stage, it->register_index, it->register_index};
    }

    void d3d9_shader_set_sampler(shader_program_handle *h, const shader_sampler_slot &slot, texture_handle *tex) {
        as_d3d9_shader_program(h)->sampler_textures[slot.slot] = tex;
    }

    void d3d9_apply_program_state(IDirect3DDevice9 *device, d3d9_shader_program *program) {
        if (!program)
            return;
        device->SetVertexShader(program->vertex_shader);
        device->SetPixelShader(program->pixel_shader);
        for (const auto &constant : program->stored_constants)
            apply_constant(device, constant);
        for (const auto &[slot, texture] : program->sampler_textures)
            bind_texture_slot(device, slot, texture);
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
