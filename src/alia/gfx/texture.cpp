#include "texture.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace alia {

    namespace {

        struct rgba_u8 {
            std::byte r, g, b, a;
        };

        bool read_rgba_u8(rgba_u8 &out, pixel_format fmt, const std::byte *src) {
            switch (fmt) {
            case pixel_format::rgb888:
                out = {src[0], src[1], src[2], std::byte{0xff}};
                return true;
            case pixel_format::rgba8888:
                out = {src[0], src[1], src[2], src[3]};
                return true;
            case pixel_format::bgr888:
                out = {src[2], src[1], src[0], std::byte{0xff}};
                return true;
            case pixel_format::bgra8888:
                out = {src[2], src[1], src[0], src[3]};
                return true;
            case pixel_format::gray_u8:
                out = {src[0], src[0], src[0], std::byte{0xff}};
                return true;
            case pixel_format::rgb565: {
                const auto raw = std::to_integer<unsigned>(src[0]) | (std::to_integer<unsigned>(src[1]) << 8);
                const auto r = (raw >> 11) & 0x1f;
                const auto g = (raw >> 5) & 0x3f;
                const auto b = raw & 0x1f;
                out = {std::byte((r * 255 + 15) / 31), std::byte((g * 255 + 31) / 63), std::byte((b * 255 + 15) / 31), std::byte{0xff}};
                return true;
            }
            default:
                return false;
            }
        }

        bool write_rgba_u8(pixel_format fmt, std::byte *dst, rgba_u8 in) {
            switch (fmt) {
            case pixel_format::rgb888:
                dst[0] = in.r; dst[1] = in.g; dst[2] = in.b;
                return true;
            case pixel_format::rgba8888:
                dst[0] = in.r; dst[1] = in.g; dst[2] = in.b; dst[3] = in.a;
                return true;
            case pixel_format::bgr888:
                dst[0] = in.b; dst[1] = in.g; dst[2] = in.r;
                return true;
            case pixel_format::bgra8888:
                dst[0] = in.b; dst[1] = in.g; dst[2] = in.r; dst[3] = in.a;
                return true;
            default:
                return false;
            }
        }

        bool upload_bitmap_view(
            texture_handle *dst_handle, const graphics_backend_interface *backend,
            const any_bitmap_view &src
        ) {
            const pixel_format src_fmt = src.format();
            const pixel_format dst_fmt = backend->texture_format.get_or_throw()(dst_handle);
            const int src_bpp = bytes_per_pixel_for_format(src_fmt);
            const int dst_bpp = bytes_per_pixel_for_format(dst_fmt);
            if (src_bpp == 0 || dst_bpp == 0)
                return false;

            texture_lock_info info{};
            const rect_i full{{0, 0}, {src.width(), src.height()}};
            if (!backend->texture_lock.get_or_throw()(dst_handle, full, 0, texture_lock_mode::write_only, info))
                return false;

            bool uploaded = true;
            if (src_fmt == dst_fmt) {
                for (int y = 0; y < src.height(); ++y) {
                    std::memcpy(
                        info.data + y * info.stride_bytes,
                        static_cast<const std::byte *>(src.line(y)),
                        static_cast<std::size_t>(src.width()) * src_bpp
                    );
                }
            } else {
                for (int y = 0; y < src.height() && uploaded; ++y) {
                    const auto *src_row = static_cast<const std::byte *>(src.line(y));
                    auto *dst_row = info.data + y * info.stride_bytes;
                    for (int x = 0; x < src.width(); ++x) {
                        rgba_u8 px{};
                        if (!read_rgba_u8(px, src_fmt, src_row + x * src_bpp) ||
                            !write_rgba_u8(dst_fmt, dst_row + x * dst_bpp, px)) {
                            uploaded = false;
                            break;
                        }
                    }
                }
            }

            backend->texture_unlock.get_or_throw()(dst_handle, info, uploaded);
            return uploaded;
        }

        void validate_texture_desc(pixel_format fmt, vec2i size, int mip_levels, const char *operation) {
            if (bytes_per_pixel_for_format(fmt) == 0)
                throw std::invalid_argument(std::string(operation) + ": unsupported pixel format");
            if (size.x <= 0 || size.y <= 0)
                throw std::invalid_argument(std::string(operation) + ": texture size must be positive");
            if (mip_levels < 0)
                throw std::invalid_argument(std::string(operation) + ": mip level count must be non-negative");
        }

        [[nodiscard]] vec2i mip_size_for(vec2i base, int level) {
            return {
                std::max(1, base.x >> level),
                std::max(1, base.y >> level)
            };
        }

    } // namespace

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
        pixel_format fmt,
        vec2i size,
        int mip_levels,
        texture_role role,
        texture_usage usage
    ) {
        if (!device.valid())
            throw std::runtime_error("texture: device is not valid");
        validate_texture_desc(fmt, size, mip_levels, "texture");

        const auto *b = device.backend();
        handle_ = b->create_texture.get_or_throw()(device.device(), fmt, size, mip_levels, role, usage);
        if (!handle_)
            throw std::runtime_error("texture: backend failed to create texture");

        backend_ = b;
        device_ = device.device();
        role_ = role;
        usage_ = usage;
    }

    texture::texture(
        gfx_device &device,
        const any_bitmap_view &src,
        int mip_levels,
        texture_role role,
        texture_usage usage
    ) {
        if (!device.valid())
            throw std::runtime_error("texture: device is not valid");
        validate_texture_desc(src.format(), {src.width(), src.height()}, mip_levels, "texture");

        const auto *b = device.backend();
        handle_ = b->create_texture.get_or_throw()(
            device.device(), src.format(), {src.width(), src.height()}, mip_levels, role, usage
        );
        if (!handle_)
            throw std::runtime_error("texture: backend failed to create texture");

        backend_ = b;
        device_ = device.device();
        role_ = role;
        usage_ = usage;
        if (!upload_bitmap_view(handle_, backend_, src)) {
            backend_->destroy_texture.get_or_throw()(handle_);
            handle_ = nullptr;
            backend_ = nullptr;
            device_ = nullptr;
            throw std::runtime_error("texture: failed to upload initial pixels");
        }
    }

    texture::texture(gfx_device &device, const bitmap &src, int mip_levels, texture_role role, texture_usage usage)
        : texture(device, src.view(), mip_levels, role, usage) {}

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
    texture::lock_impl(const std::optional<rect_i> &region, int level, pixel_format expected_fmt, texture_lock_mode mode) {
        if (!handle_ || backend_->texture_format.get_or_throw()(handle_) != expected_fmt)
            return nullptr;

        const int lw = std::max(1, backend_->texture_width.get_or_throw()(handle_) >> level);
        const int lh = std::max(1, backend_->texture_height.get_or_throw()(handle_) >> level);
        const rect_i r = region.value_or(rect_i{{0, 0}, {lw, lh}});

        texture_lock_info info{};
        if (!backend_->texture_lock.get_or_throw()(handle_, r, level, mode, info))
            return nullptr;

        auto state = std::make_unique<detail::texture_lock_state>();
        state->handle = handle_;
        state->backend = backend_;
        state->info = info;
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
