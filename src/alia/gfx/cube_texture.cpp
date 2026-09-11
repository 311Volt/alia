#include "cube_texture.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace alia {
    namespace {
        [[nodiscard]] bool valid_face(cube_face face) noexcept {
            const int value = static_cast<int>(face);
            return value >= 0 && value < cube_face_count;
        }

        void validate_face(cube_face face, const char *operation) {
            if (!valid_face(face))
                throw std::out_of_range(std::string(operation) + ": cube face out of range");
        }

        void validate_faces(
            std::span<const any_bitmap_view, cube_face_count> faces,
            int mip_levels
        ) {
            const auto format = faces[0].format();
            const auto size = faces[0].size();
            detail::validate_texture_desc(format, size, mip_levels, "cube_texture");
            if (size.x != size.y)
                throw std::invalid_argument("cube_texture: faces must be square");

            for (std::size_t i = 1; i < faces.size(); ++i) {
                if (faces[i].size() != size)
                    throw std::invalid_argument("cube_texture: all faces must have the same size");
                if (faces[i].format() != format)
                    throw std::invalid_argument("cube_texture: all faces must have the same pixel format");
            }
        }
    } // namespace

    cube_texture::~cube_texture() {
        if (handle_)
            backend_->destroy_texture.get_or_throw()(handle_);
    }

    cube_texture::cube_texture(cube_texture &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr))
        , backend_(std::exchange(other.backend_, nullptr))
        , device_(std::exchange(other.device_, nullptr))
        , usage_(std::exchange(other.usage_, texture_usage::sampling_only)) {}

    cube_texture &cube_texture::operator=(cube_texture &&other) noexcept {
        if (this != &other) {
            if (handle_)
                backend_->destroy_texture.get_or_throw()(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
            backend_ = std::exchange(other.backend_, nullptr);
            device_ = std::exchange(other.device_, nullptr);
            usage_ = std::exchange(other.usage_, texture_usage::sampling_only);
        }
        return *this;
    }

    cube_texture::cube_texture(
        gfx_device &device,
        pixel_format fmt,
        int edge,
        int mip_levels,
        texture_usage usage
    ) {
        if (!device.valid())
            throw std::runtime_error("cube_texture: device is not valid");
        detail::validate_texture_desc(fmt, {edge, edge}, mip_levels, "cube_texture");

        const auto *backend = device.backend();
        handle_ = backend->create_cube_texture.get_or_throw()(
            device.device(), fmt, edge, mip_levels, usage);
        if (!handle_)
            throw std::runtime_error("cube_texture: backend failed to create texture");

        backend_ = backend;
        device_ = device.device();
        usage_ = usage;
    }

    cube_texture::cube_texture(
        gfx_device &device,
        std::span<const any_bitmap_view, cube_face_count> faces,
        int mip_levels,
        texture_usage usage
    ) {
        if (!device.valid())
            throw std::runtime_error("cube_texture: device is not valid");
        validate_faces(faces, mip_levels);

        const auto *backend = device.backend();
        handle_ = backend->create_cube_texture.get_or_throw()(
            device.device(), faces[0].format(), faces[0].width(), mip_levels, usage);
        if (!handle_)
            throw std::runtime_error("cube_texture: backend failed to create texture");

        backend_ = backend;
        device_ = device.device();
        usage_ = usage;

        try {
            for (int i = 0; i < cube_face_count; ++i) {
                if (!detail::upload_bitmap_view(
                        handle_, backend_, faces[static_cast<std::size_t>(i)],
                        static_cast<cube_face>(i)))
                    throw std::runtime_error(
                        "cube_texture: failed to upload initial face pixels");
            }
        } catch (...) {
            backend_->destroy_texture.get_or_throw()(handle_);
            handle_ = nullptr;
            backend_ = nullptr;
            device_ = nullptr;
            throw;
        }
    }

    cube_texture::cube_texture(
        gfx_device &device,
        std::span<const bitmap, cube_face_count> faces,
        int mip_levels,
        texture_usage usage
    )
        : cube_texture(
            device,
            std::array<any_bitmap_view, cube_face_count>{
                faces[0].view(), faces[1].view(), faces[2].view(),
                faces[3].view(), faces[4].view(), faces[5].view()
            },
            mip_levels,
            usage
        ) {}

    pixel_format cube_texture::format() const noexcept {
        return backend_->texture_format.get_or_throw()(handle_);
    }

    int cube_texture::edge() const noexcept {
        return backend_->texture_width.get_or_throw()(handle_);
    }

    int cube_texture::mip_levels() const noexcept {
        return backend_->texture_mip_levels.get_or_throw()(handle_);
    }

    void cube_texture::set_sampler(const sampler_state &s) {
        backend_->texture_set_sampler.get_or_throw()(handle_, s);
    }

    sampler_state cube_texture::sampler() const noexcept {
        return backend_->texture_sampler.get_or_throw()(handle_);
    }

    std::unique_ptr<detail::texture_lock_state> cube_texture::lock_impl(
        cube_face face,
        const std::optional<rect_i> &region,
        int level,
        pixel_format expected_fmt,
        texture_lock_mode mode
    ) {
        if (!handle_ || !valid_face(face) || level < 0 || level >= mip_levels() ||
            format() != expected_fmt)
            return nullptr;

        const int size = std::max(1, edge() >> level);
        const rect_i requested = region.value_or(rect_i{{0, 0}, {size, size}});

        texture_lock_info info{};
        if (!backend_->cube_texture_lock.get_or_throw()(
                handle_, face, requested, level, mode, info))
            return nullptr;

        auto state = std::make_unique<detail::texture_lock_state>();
        state->handle = handle_;
        state->backend = backend_;
        state->info = info;
        return state;
    }

    void cube_texture::generate_mipmaps() {
        backend_->texture_generate_mipmaps.get_or_throw()(handle_);
    }

    bitmap cube_texture::download(cube_face face, int level) const {
        validate_face(face, "cube_texture::download");
        if (level < 0 || level >= mip_levels())
            throw std::out_of_range("cube_texture::download: mip level out of range");

        const int size = std::max(1, edge() >> level);
        texture_lock_info info{};
        if (!backend_->cube_texture_lock.get_or_throw()(
                handle_, face, rect_i{{0, 0}, {size, size}}, level,
                texture_lock_mode::read_only, info))
            throw std::runtime_error("cube_texture::download: backend lock failed");

        const std::size_t total = static_cast<std::size_t>(info.stride_bytes) * size;
        bitmap result(
            format(), {size, size}, std::span<const std::byte>(info.data, total),
            info.stride_bytes);
        backend_->texture_unlock.get_or_throw()(handle_, info, false);
        return result;
    }

    cube_texture cube_texture::clone() const {
        texture_handle *cloned = backend_->texture_clone.get_or_throw()(handle_);
        if (!cloned)
            throw std::runtime_error("cube_texture::clone: backend failed to clone texture");
        return cube_texture(cloned, backend_, device_, usage_);
    }

} // namespace alia
