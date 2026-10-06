#ifndef ALIA_GFX_CUBE_TEXTURE_HPP
#define ALIA_GFX_CUBE_TEXTURE_HPP

#include "texture.hpp"

#include <array>
#include <optional>
#include <span>
#include <utility>

namespace alia {

    struct cube_texture_config {
        /// Set: stored exactly; face pixels are converted (lossy allowed).
        /// Throws if the device cannot store the requested format.
        /// Empty: uploads use the source format or the closest supported one;
        /// uninitialised cube textures use bgra8888.
        std::optional<pixel_format> format;
        int mip_levels = 1;
        texture_usage usage = texture_usage::sampling_only;
    };

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
            int edge,
            const cube_texture_config &config = {}
        );

        /// Create and initialise level 0 from six type-erased face views.
        /// Faces are ordered according to @c cube_face.
        cube_texture(
            gfx_device &device,
            std::span<const any_bitmap_view, cube_face_count> faces,
            const cube_texture_config &config = {}
        );

        /// Create and initialise level 0 from six owning bitmaps.
        cube_texture(
            gfx_device &device,
            std::span<const bitmap, cube_face_count> faces,
            const cube_texture_config &config = {}
        );

        /// Create and initialise level 0 from six typed face views.
        template <pixel TPixel>
        cube_texture(
            gfx_device &device,
            std::span<const bitmap_view<TPixel>, cube_face_count> faces,
            const cube_texture_config &config = {}
        )
            : cube_texture(device, copy_faces(faces), config) {}

        /// Query the exact configured format, or bgra8888 if omitted, without
        /// allocating. Returns false if cube textures are unsupported.
        /// Dimensions and allocation success are not covered by this query.
        [[nodiscard]] static bool can_create(const gfx_device &, const cube_texture_config &);

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

        /// Acquire a typed lock for reading and writing pixels on one face.
        /// @tparam TPixel Must match the stored format; otherwise returns a falsy handle.
        /// @param face Cube face to lock; invalid faces return a falsy handle.
        /// @param region Sub-rectangle clamped to level bounds; omit for the entire face level.
        /// @param level Mip level (0 = base). Invalid levels/empty regions return a falsy handle.
        template <pixel TPixel>
        [[nodiscard]] locked_texture_region<TPixel, texture_lock_mode::read_write>
        lock(cube_face face, std::optional<rect_i> region = {}, int level = 0) {
            return locked_texture_region<TPixel, texture_lock_mode::read_write>(
                lock_impl(face, region, level, TPixel::format_id, texture_lock_mode::read_write));
        }

        /// Acquire a typed lock to inspect pixels without modifying them.
        /// The returned view is const and release skips the upload step.
        /// @tparam TPixel Must match the stored format; otherwise returns a falsy handle.
        /// @param face Cube face to lock; invalid faces return a falsy handle.
        /// @param region Sub-rectangle clamped to level bounds; omit for the entire face level.
        /// @param level Mip level (0 = base). Invalid levels/empty regions return a falsy handle.
        template <pixel TPixel>
        [[nodiscard]] locked_texture_region<TPixel, texture_lock_mode::read_only>
        lock_read_only(cube_face face, std::optional<rect_i> region = {}, int level = 0) {
            return locked_texture_region<TPixel, texture_lock_mode::read_only>(
                lock_impl(face, region, level, TPixel::format_id, texture_lock_mode::read_only));
        }

        /// Acquire a typed lock without preserving current contents.
        /// The backend may skip downloading pixels. Write every locked pixel;
        /// reading unwritten pixels or leaving them unwritten gives undefined contents.
        /// @tparam TPixel Must match the stored format; otherwise returns a falsy handle.
        /// @param face Cube face to lock; invalid faces return a falsy handle.
        /// @param region Sub-rectangle clamped to level bounds; omit for the entire face level.
        /// @param level Mip level (0 = base). Invalid levels/empty regions return a falsy handle.
        template <pixel TPixel>
        [[nodiscard]] locked_texture_region<TPixel, texture_lock_mode::write_only>
        lock_write_only(cube_face face, std::optional<rect_i> region = {}, int level = 0) {
            return locked_texture_region<TPixel, texture_lock_mode::write_only>(
                lock_impl(face, region, level, TPixel::format_id, texture_lock_mode::write_only));
        }

        /// Acquire a runtime-format lock for reading and writing pixels on one face.
        /// @param face Cube face to lock; invalid faces return a falsy handle.
        /// @param region Sub-rectangle clamped to level bounds; omit for the entire face level.
        /// @param level Mip level (0 = base). Invalid levels/empty regions return a falsy handle.
        [[nodiscard]] any_locked_texture_region<texture_lock_mode::read_write>
        lock_any(cube_face face, std::optional<rect_i> region = {}, int level = 0) {
            return any_locked_texture_region<texture_lock_mode::read_write>(
                lock_impl(face, region, level, std::nullopt, texture_lock_mode::read_write));
        }

        /// Acquire a runtime-format lock to inspect pixels without modifying them.
        /// Visitors enforce read-only access; release skips the upload step.
        /// @param face Cube face to lock; invalid faces return a falsy handle.
        /// @param region Sub-rectangle clamped to level bounds; omit for the entire face level.
        /// @param level Mip level (0 = base). Invalid levels/empty regions return a falsy handle.
        [[nodiscard]] any_locked_texture_region<texture_lock_mode::read_only>
        lock_any_read_only(cube_face face, std::optional<rect_i> region = {}, int level = 0) {
            return any_locked_texture_region<texture_lock_mode::read_only>(
                lock_impl(face, region, level, std::nullopt, texture_lock_mode::read_only));
        }

        /// Acquire a runtime-format lock without preserving current contents.
        /// The backend may skip downloading pixels. Write every locked pixel;
        /// reading unwritten pixels or leaving them unwritten gives undefined contents.
        /// @param face Cube face to lock; invalid faces return a falsy handle.
        /// @param region Sub-rectangle clamped to level bounds; omit for the entire face level.
        /// @param level Mip level (0 = base). Invalid levels/empty regions return a falsy handle.
        [[nodiscard]] any_locked_texture_region<texture_lock_mode::write_only>
        lock_any_write_only(cube_face face, std::optional<rect_i> region = {}, int level = 0) {
            return any_locked_texture_region<texture_lock_mode::write_only>(
                lock_impl(face, region, level, std::nullopt, texture_lock_mode::write_only));
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
            const cube_texture_config &config
        )
            : cube_texture(
                device,
                std::span<const bitmap, cube_face_count>(faces),
                config
            ) {}

        cube_texture(
            gfx_device &device,
            const std::array<any_bitmap_view, cube_face_count> &faces,
            const cube_texture_config &config
        )
            : cube_texture(
                device,
                std::span<const any_bitmap_view, cube_face_count>(faces),
                config
            ) {}

        std::unique_ptr<detail::texture_lock_state> lock_impl(
            cube_face face,
            const std::optional<rect_i> &region,
            int level,
            std::optional<pixel_format> expected_fmt,
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
