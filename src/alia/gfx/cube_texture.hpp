#ifndef ALIA_GFX_CUBE_TEXTURE_HPP
#define ALIA_GFX_CUBE_TEXTURE_HPP

#include "texture.hpp"

#include <array>
#include <optional>
#include <span>
#include <utility>

namespace alia {

    /// @brief GPU-resident cube map with six square faces.
    ///
    /// Cube textures are move-only and share texture sampler, locking, mipmap,
    /// download, and clone behaviour with @c texture. They are intended for
    /// shader samplers; fixed-function cube-map texgen is not provided.
    class cube_texture {
    public:
        ~cube_texture();
        cube_texture(cube_texture &&) noexcept;
        cube_texture &operator=(cube_texture &&) noexcept;
        cube_texture(const cube_texture &) = delete;
        cube_texture &operator=(const cube_texture &) = delete;

        /// Create an uninitialised color cube texture.
        cube_texture(
            gfx_device &device,
            pixel_format fmt,
            int edge,
            int mip_levels = 1,
            texture_usage usage = texture_usage::sampling_only
        );

        /// Create and initialise level 0 from six type-erased face views.
        /// Faces are ordered according to @c cube_face.
        cube_texture(
            gfx_device &device,
            std::span<const any_bitmap_view, cube_face_count> faces,
            int mip_levels = 1,
            texture_usage usage = texture_usage::sampling_only
        );

        /// Create and initialise level 0 from six owning bitmaps.
        cube_texture(
            gfx_device &device,
            std::span<const bitmap, cube_face_count> faces,
            int mip_levels = 1,
            texture_usage usage = texture_usage::sampling_only
        );

        /// Create and initialise level 0 from six typed face views.
        template <pixel TPixel>
        cube_texture(
            gfx_device &device,
            std::span<const bitmap_view<TPixel>, cube_face_count> faces,
            int mip_levels = 1,
            texture_usage usage = texture_usage::sampling_only
        )
            : cube_texture(device, copy_faces(faces), mip_levels, usage) {}

        [[nodiscard]] pixel_format format() const noexcept;
        [[nodiscard]] texture_usage usage() const noexcept {
            return usage_;
        }
        [[nodiscard]] int edge() const noexcept;
        [[nodiscard]] vec2i face_size() const noexcept {
            const int value = edge();
            return {value, value};
        }
        [[nodiscard]] int mip_levels() const noexcept;

        void set_sampler(const sampler_state &s);
        [[nodiscard]] sampler_state sampler() const noexcept;

        template <pixel TPixel>
        [[nodiscard]] locked_texture_region<TPixel, texture_lock_mode::read_write>
        lock(cube_face face, std::optional<rect_i> region = {}, int level = 0) {
            return locked_texture_region<TPixel, texture_lock_mode::read_write>(
                lock_impl(face, region, level, TPixel::format_id, texture_lock_mode::read_write));
        }

        template <pixel TPixel>
        [[nodiscard]] locked_texture_region<TPixel, texture_lock_mode::read_only>
        lock_read_only(cube_face face, std::optional<rect_i> region = {}, int level = 0) {
            return locked_texture_region<TPixel, texture_lock_mode::read_only>(
                lock_impl(face, region, level, TPixel::format_id, texture_lock_mode::read_only));
        }

        template <pixel TPixel>
        [[nodiscard]] locked_texture_region<TPixel, texture_lock_mode::write_only>
        lock_write_only(cube_face face, std::optional<rect_i> region = {}, int level = 0) {
            return locked_texture_region<TPixel, texture_lock_mode::write_only>(
                lock_impl(face, region, level, TPixel::format_id, texture_lock_mode::write_only));
        }

        void generate_mipmaps();
        [[nodiscard]] bitmap download(cube_face face, int level = 0) const;
        [[nodiscard]] cube_texture clone() const;

        [[nodiscard]] texture_handle *impl() noexcept {
            return handle_;
        }
        [[nodiscard]] const texture_handle *impl() const noexcept {
            return handle_;
        }
        [[nodiscard]] const graphics_backend_interface *backend() const noexcept {
            return backend_;
        }
        [[nodiscard]] device_handle *device() const noexcept {
            return device_;
        }

    private:
        template <pixel TPixel>
        static std::array<bitmap, cube_face_count> copy_faces(
            std::span<const bitmap_view<TPixel>, cube_face_count> faces
        ) {
            return {
                bitmap(faces[0]), bitmap(faces[1]), bitmap(faces[2]),
                bitmap(faces[3]), bitmap(faces[4]), bitmap(faces[5])
            };
        }

        cube_texture(
            gfx_device &device,
            const std::array<bitmap, cube_face_count> &faces,
            int mip_levels,
            texture_usage usage
        )
            : cube_texture(
                device,
                std::span<const bitmap, cube_face_count>(faces),
                mip_levels,
                usage
            ) {}

        cube_texture(
            gfx_device &device,
            const std::array<any_bitmap_view, cube_face_count> &faces,
            int mip_levels,
            texture_usage usage
        )
            : cube_texture(
                device,
                std::span<const any_bitmap_view, cube_face_count>(faces),
                mip_levels,
                usage
            ) {}

        std::unique_ptr<detail::texture_lock_state> lock_impl(
            cube_face face,
            const std::optional<rect_i> &region,
            int level,
            pixel_format expected_fmt,
            texture_lock_mode mode
        );

        explicit cube_texture(
            texture_handle *handle,
            const graphics_backend_interface *backend,
            device_handle *device,
            texture_usage usage
        ) noexcept
            : handle_(handle), backend_(backend), device_(device), usage_(usage) {}

        texture_handle *handle_ = nullptr;
        const graphics_backend_interface *backend_ = nullptr;
        device_handle *device_ = nullptr;
        texture_usage usage_ = texture_usage::sampling_only;
    };

} // namespace alia

#endif // ALIA_GFX_CUBE_TEXTURE_HPP
