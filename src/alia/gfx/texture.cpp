#include "texture.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace alia {

    namespace detail {

        std::span<const pixel_format> texture_format_fallbacks(pixel_format source, texture_role role) {
            static constexpr pixel_format color_u8[] = {pixel_format::bgra8888, pixel_format::rgba8888};
            static constexpr pixel_format bgra[] = {pixel_format::bgra8888};
            static constexpr pixel_format rgba[] = {pixel_format::rgba8888};
            static constexpr pixel_format color_float[] = {pixel_format::rgba_f32};
            switch (source) {
            case pixel_format::rgb888:
            case pixel_format::bgr888:
            case pixel_format::rgb565:
            case pixel_format::xy_u8:
                return color_u8;
            case pixel_format::gray_u8:
                return role == texture_role::color ? std::span<const pixel_format>(color_u8)
                                                   : std::span<const pixel_format>{};
            case pixel_format::rgba8888:
                return bgra;
            case pixel_format::bgra8888:
                return rgba;
            case pixel_format::gray_f32:
            case pixel_format::rgb_f32:
            case pixel_format::xy_f32:
                return color_float;
            default:
                return {};
            }
        }

        bool upload_bitmap_view(
            texture_handle *dst_handle,
            const graphics_backend_interface *backend,
            const any_bitmap_view &src,
            std::optional<cube_face> face
        ) {
            const pixel_format src_fmt = src.format();
            const pixel_format dst_fmt = backend->texture_format.get_or_throw()(dst_handle);
            if (!can_convert_pixel_lossy(src_fmt, dst_fmt))
                return false;

            texture_lock_info info{};
            const rect_i full{{0, 0}, {src.width(), src.height()}};
            const bool locked = face
                ? backend->cube_texture_lock.get_or_throw()(
                    dst_handle, *face, full, 0, texture_lock_mode::write_only, info)
                : backend->texture_lock.get_or_throw()(
                    dst_handle, full, 0, texture_lock_mode::write_only, info);
            if (!locked)
                return false;

            for (int y = 0; y < src.height(); ++y)
                convert_pixel_row_lossy(dst_fmt, info.data + y * info.stride_bytes, src_fmt, src.line(y), src.width());

            backend->texture_unlock.get_or_throw()(dst_handle, info, true);
            return true;
        }

        void validate_texture_desc(pixel_format fmt, vec2i size, int mip_levels, const char *operation) {
            if (bytes_per_pixel_for_format(fmt) == 0)
                throw std::invalid_argument(std::string(operation) + ": unsupported pixel format");
            if (size.x <= 0 || size.y <= 0)
                throw std::invalid_argument(std::string(operation) + ": texture size must be positive");
            if (mip_levels < 0)
                throw std::invalid_argument(std::string(operation) + ": mip level count must be non-negative");
        }

    } // namespace detail

    // ── Destructor / move ─────────────────────────────────────────────────

    texture::~texture() {
        if (handle_)
            backend_->destroy_texture.get_or_throw()(handle_);
    }

    texture::texture(texture &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr))
        , backend_(std::exchange(other.backend_, nullptr))
        , device_(std::exchange(other.device_, nullptr))
        , role_(std::exchange(other.role_, texture_role::color))
        , usage_(std::exchange(other.usage_, texture_usage::sampling_only)) {}

    texture &texture::operator=(texture &&other) noexcept {
        if (this != &other) {
            if (handle_)
                backend_->destroy_texture.get_or_throw()(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
            backend_ = std::exchange(other.backend_, nullptr);
            device_ = std::exchange(other.device_, nullptr);
            role_ = std::exchange(other.role_, texture_role::color);
            usage_ = std::exchange(other.usage_, texture_usage::sampling_only);
        }
        return *this;
    }

    // ── Constructors ──────────────────────────────────────────────────────

    texture::texture(
        gfx_device &device,
        vec2i size,
        const texture_config &config
    ) {
        if (!device.valid())
            throw std::runtime_error("texture: device is not valid");
        const pixel_format fmt = config.format.value_or(
            config.role == texture_role::alpha_mask ? pixel_format::gray_u8 : pixel_format::bgra8888);
        detail::validate_texture_desc(fmt, size, config.mip_levels, "texture");
        if (!can_create(device, config))
            throw std::runtime_error("texture: requested pixel format is not supported by this device");

        const auto *b = device.backend();
        handle_ = b->create_texture.get_or_throw()(
            device.device(), fmt, size, config.mip_levels, config.role, config.usage);
        if (!handle_)
            throw std::runtime_error("texture: backend failed to create texture");

        backend_ = b;
        device_ = device.device();
        role_ = config.role;
        usage_ = config.usage;
    }

    texture::texture(
        gfx_device &device,
        const any_bitmap_view &src,
        const texture_config &config
    ) {
        if (!device.valid())
            throw std::runtime_error("texture: device is not valid");
        detail::validate_texture_desc(src.format(), src.size(), config.mip_levels, "texture");
        if (config.format)
            detail::validate_texture_desc(*config.format, src.size(), config.mip_levels, "texture");

        const auto *b = device.backend();
        const auto supported = [&](pixel_format fmt) {
            return b->texture_format_supported.get_or_throw()(
                device.device(), fmt, config.mip_levels, config.role, config.usage);
        };
        const auto fmt = config.format
            ? (supported(*config.format) ? config.format : std::nullopt)
            : detail::closest_texture_format(src.format(), config.role, supported);
        if (!fmt)
            throw std::runtime_error("texture: requested pixel format is not supported by this device");
        if (!can_convert_pixel_lossy(src.format(), *fmt))
            throw std::invalid_argument("texture: unsupported source pixel format conversion");

        handle_ = b->create_texture.get_or_throw()(
            device.device(), *fmt, src.size(), config.mip_levels, config.role, config.usage
        );
        if (!handle_)
            throw std::runtime_error("texture: backend failed to create texture");

        backend_ = b;
        device_ = device.device();
        role_ = config.role;
        usage_ = config.usage;
        if (!detail::upload_bitmap_view(handle_, backend_, src)) {
            backend_->destroy_texture.get_or_throw()(handle_);
            handle_ = nullptr;
            backend_ = nullptr;
            device_ = nullptr;
            throw std::runtime_error("texture: failed to upload initial pixels");
        }
    }

    texture::texture(gfx_device &device, const bitmap &src, const texture_config &config)
        : texture(device, src.view(), config) {}

    bool texture::can_create(const gfx_device &device, const texture_config &config) {
        if (!device.valid() || config.mip_levels < 0 ||
            !device.backend()->texture_format_supported.is_supported())
            return false;
        const auto fmt = config.format.value_or(
            config.role == texture_role::alpha_mask ? pixel_format::gray_u8 : pixel_format::bgra8888);
        if (bytes_per_pixel_for_format(fmt) == 0)
            return false;
        return device.backend()->texture_format_supported.get_or_throw()(
            device.device(), fmt, config.mip_levels, config.role, config.usage);
    }

    // ── Accessors ────────────────────────────────────────────────────────

    pixel_format texture::format() const noexcept {
        return backend_->texture_format.get_or_throw()(handle_);
    }
    int texture::width() const noexcept {
        return backend_->texture_width.get_or_throw()(handle_);
    }
    int texture::height() const noexcept {
        return backend_->texture_height.get_or_throw()(handle_);
    }
    int texture::mip_levels() const noexcept {
        return backend_->texture_mip_levels.get_or_throw()(handle_);
    }

    void texture::set_sampler(const sampler_state &s) {
        backend_->texture_set_sampler.get_or_throw()(handle_, s);
    }
    sampler_state texture::sampler() const noexcept {
        return backend_->texture_sampler.get_or_throw()(handle_);
    }

    // ── lock_impl ────────────────────────────────────────────────────────

    std::unique_ptr<detail::texture_lock_state>
    texture::lock_impl(
        const std::optional<rect_i> &region, int level,
        std::optional<pixel_format> expected_fmt, texture_lock_mode mode
    ) {
        if (!handle_ || level < 0 || level >= mip_levels())
            return nullptr;
        const auto actual_fmt = format();
        if (expected_fmt && actual_fmt != *expected_fmt)
            return nullptr;

        const int lw = std::max(1, backend_->texture_width.get_or_throw()(handle_) >> level);
        const int lh = std::max(1, backend_->texture_height.get_or_throw()(handle_) >> level);
        const rect_i r = region.value_or(rect_i{{0, 0}, {lw, lh}});

        auto state = std::make_unique<detail::texture_lock_state>();
        if (!backend_->texture_lock.get_or_throw()(handle_, r, level, mode, state->info))
            return nullptr;

        state->handle = handle_;
        state->backend = backend_;
        state->format = actual_fmt;
        return state;
    }

    // ── Operations ───────────────────────────────────────────────────────

    void texture::generate_mipmaps() {
        backend_->texture_generate_mipmaps.get_or_throw()(handle_);
    }

    bitmap texture::download(int level) const {
        const int lw = std::max(1, backend_->texture_width.get_or_throw()(handle_) >> level);
        const int lh = std::max(1, backend_->texture_height.get_or_throw()(handle_) >> level);
        const rect_i full{{0, 0}, {lw, lh}};

        texture_lock_info info{};
        if (!backend_->texture_lock.get_or_throw()(handle_, full, level, texture_lock_mode::read_only, info))
            throw std::runtime_error("texture::download: backend lock failed");

        const pixel_format fmt = backend_->texture_format.get_or_throw()(handle_);
        const std::size_t total = static_cast<std::size_t>(info.stride_bytes) * lh;

        bitmap result(fmt, {lw, lh}, std::span<const std::byte>(info.data, total), info.stride_bytes);

        backend_->texture_unlock.get_or_throw()(handle_, info, false);
        return result;
    }

    texture texture::clone() const {
        texture_handle *cloned = backend_->texture_clone.get_or_throw()(handle_);
        if (!cloned)
            throw std::runtime_error("texture::clone: backend failed to clone texture");
        return texture(cloned, backend_, device_, role_, usage_);
    }

} // namespace alia
