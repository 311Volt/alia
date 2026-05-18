#ifndef TEXTURE_D1DD8DA1_4AF1_4438_BF28_BC459807D50F
#define TEXTURE_D1DD8DA1_4AF1_4438_BF28_BC459807D50F

#include "bitmap/bitmap.hpp"
#include "gfx_device.hpp"
#include <optional>
#include <utility>

namespace alia {

    /// Sentinel for @c mip_levels constructor arg meaning "full chain down to 1×1".
    /// Pass @c 1 for a mip-less texture, or an explicit count for a partial chain.
    inline constexpr int full_mip_chain = 0;

    // ── Internal lock state (shared by locked_texture_region<TPixel>) ─────

    namespace detail {
        struct texture_lock_state {
            texture_handle *handle = nullptr;
            const graphics_backend_interface *backend = nullptr;
            texture_lock_info info = {};
        };

        struct texture_render_target_state {
            render_target_scope_handle *handle = nullptr;
            device_handle *device = nullptr;
            const graphics_backend_interface *backend = nullptr;
            vec2i previous_render_target_size = {};
        };
    } // namespace detail

    // ── locked_texture_region ─────────────────────────────────────────────

    /// @brief RAII handle granting CPU access to a texture mip level.
    ///
    /// Obtained from @c texture::lock<TPixel>() (read-write),
    /// @c texture::lock_read_only<TPixel>(), or @c texture::lock_write_only<TPixel>().
    /// On destruction (or when @c release() is called) the modified region is
    /// committed back to GPU memory, unless this was a read-only lock.
    ///
    /// Evaluates to @c true if the lock succeeded. Use:
    /// @code
    ///   if (auto reg = tex.lock<px_rgba8888>()) {
    ///       (*reg)[x, y] = px_rgba8888{255, 0, 0, 255};
    ///   } // committed on scope exit
    /// @endcode
    ///
    /// For a read-only lock, @c view() returns a @c const reference, so writes
    /// through the view are a compile error.
    ///
    /// Format mismatch or an out-of-range region produces a falsy handle;
    /// no GPU access occurs.
    template <pixel TPixel, texture_lock_mode Mode = texture_lock_mode::read_write>
    class locked_texture_region {
    public:
        locked_texture_region() = default;

        ~locked_texture_region() {
            release();
        }

        locked_texture_region(locked_texture_region &&) noexcept = default;

        locked_texture_region &operator=(locked_texture_region &&o) noexcept {
            if (this != &o) {
                release();
                impl_ = std::move(o.impl_);
                view_ = o.view_;
                o.view_ = {};
            }
            return *this;
        }

        locked_texture_region(const locked_texture_region &) = delete;
        locked_texture_region &operator=(const locked_texture_region &) = delete;

        /// @c true if the lock succeeded; @c false on format mismatch or backend error.
        [[nodiscard]] explicit operator bool() const noexcept {
            return impl_ != nullptr;
        }

        /// @returns Reference to the typed view over the locked region.
        /// For a read-only lock returns @c const, so subscript writes are a
        /// compile error. Undefined if @c !*this.
        [[nodiscard]] auto &view() noexcept {
            if constexpr (Mode == texture_lock_mode::read_only)
                return std::as_const(view_);
            else
                return view_;
        }

        /// Shorthand for @c view().
        [[nodiscard]] auto &operator*() noexcept {
            return view();
        }

        /// @returns The top-left pixel coordinate of the locked region within the texture level.
        [[nodiscard]] vec2i origin() const noexcept {
            return impl_->info.origin;
        }

        /// Commit changes (unless this was a read-only lock) and release the lock early.
        /// The destructor calls this automatically; safe to call more than once.
        void release() {
            if (impl_) {
                constexpr bool wrote = (Mode != texture_lock_mode::read_only);
                impl_->backend->texture_unlock.get_or_throw()(impl_->handle, impl_->info, wrote);
                impl_.reset();
                view_ = {};
            }
        }

    private:
        friend class texture;
        std::unique_ptr<detail::texture_lock_state> impl_;
        bitmap_view<TPixel> view_{};

        explicit locked_texture_region(std::unique_ptr<detail::texture_lock_state> s) noexcept
            : impl_(std::move(s))
            , view_(impl_
                ? bitmap_view<TPixel>(impl_->info.extent.x, impl_->info.extent.y,
                                      impl_->info.stride_bytes, impl_->info.data)
                : bitmap_view<TPixel>{}) {}
    };

    // ── texture ───────────────────────────────────────────────────────────

    class scoped_texture_render_target {
    public:
        ~scoped_texture_render_target();
        scoped_texture_render_target(scoped_texture_render_target &&) noexcept;
        scoped_texture_render_target &operator=(scoped_texture_render_target &&) noexcept;
        scoped_texture_render_target(const scoped_texture_render_target &) = delete;
        scoped_texture_render_target &operator=(const scoped_texture_render_target &) = delete;

        void release();

    private:
        friend class texture;

        std::unique_ptr<detail::texture_render_target_state> impl_;

        explicit scoped_texture_render_target(std::unique_ptr<detail::texture_render_target_state> state) noexcept
            : impl_(std::move(state)) {}
    };

    /// @brief GPU-resident 2D pixel buffer. Hardware counterpart of @c bitmap.
    ///
    /// Owns a backend-specific GPU resource. Move-only; use @c clone() for a
    /// GPU-to-GPU deep copy. Created through a @c gfx_device; construction
    /// throws if the backend cannot create a valid texture.
    class texture {
    public:
        ~texture();
        texture(texture &&) noexcept;
        texture &operator=(texture &&) noexcept;
        texture(const texture &) = delete;
        texture &operator=(const texture &) = delete;

        // ── Construction ─────────────────────────────────────────────────

        /// Create an uninitialised texture.
        texture(
            gfx_device &device,
            pixel_format fmt,
            vec2i size,
            int mip_levels = 1,
            texture_role role = texture_role::color,
            texture_usage usage = texture_usage::sampling_only
        );

        /// Create and initialise level 0 from a type-erased CPU view.
        texture(
            gfx_device &device,
            const any_bitmap_view &src,
            int mip_levels = 1,
            texture_role role = texture_role::color,
            texture_usage usage = texture_usage::sampling_only
        );

        /// Create and initialise level 0 from an owning bitmap.
        texture(
            gfx_device &device,
            const bitmap &src,
            int mip_levels = 1,
            texture_role role = texture_role::color,
            texture_usage usage = texture_usage::sampling_only
        );

        /// Create and initialise level 0 from a typed CPU view.
        template <pixel TPixel>
        texture(
            gfx_device &device,
            const bitmap_view<TPixel> &src,
            int mip_levels = 1,
            texture_role role = texture_role::color,
            texture_usage usage = texture_usage::sampling_only
        )
            : texture(device, bitmap(src), mip_levels, role, usage) {}

        // ── State queries ────────────────────────────────────────────────

        [[nodiscard]] pixel_format format() const noexcept;
        [[nodiscard]] texture_role role() const noexcept {
            return role_;
        }
        [[nodiscard]] texture_usage usage() const noexcept {
            return usage_;
        }
        [[nodiscard]] int width() const noexcept;
        [[nodiscard]] int height() const noexcept;
        [[nodiscard]] vec2i size() const noexcept {
            return {width(), height()};
        }
        [[nodiscard]] int mip_levels() const noexcept;

        // ── Sampler state ────────────────────────────────────────────────

        void set_sampler(const sampler_state &s);
        [[nodiscard]] sampler_state sampler() const noexcept;

        // ── CPU access ───────────────────────────────────────────────────

        /// Acquire a typed lock for reading and writing pixels.
        ///
        /// @tparam TPixel  Must satisfy the @c pixel concept and have
        ///                 @c TPixel::format_id matching this texture's format;
        ///                 otherwise returns a falsy handle.
        /// @param region   Sub-rectangle to lock (clamped to level bounds).
        ///                 Omit to lock the entire level.
        /// @param level    Mip level (0 = base).
        template <pixel TPixel>
        [[nodiscard]] locked_texture_region<TPixel, texture_lock_mode::read_write>
        lock(std::optional<rect_i> region = {}, int level = 0) {
            return locked_texture_region<TPixel, texture_lock_mode::read_write>(
                lock_impl(region, level, TPixel::format_id, texture_lock_mode::read_write));
        }

        /// Acquire a typed lock to inspect pixels without modifying them.
        ///
        /// Equivalent to @c lock() but pixel writes through the returned view
        /// are silently discarded on release, allowing the backend to skip the
        /// upload step.
        ///
        /// @tparam TPixel  Must match this texture's format; otherwise falsy.
        /// @param region   Sub-rectangle to lock (clamped to level bounds).
        /// @param level    Mip level (0 = base).
        template <pixel TPixel>
        [[nodiscard]] locked_texture_region<TPixel, texture_lock_mode::read_only>
        lock_read_only(std::optional<rect_i> region = {}, int level = 0) {
            return locked_texture_region<TPixel, texture_lock_mode::read_only>(
                lock_impl(region, level, TPixel::format_id, texture_lock_mode::read_only));
        }

        /// Acquire a typed lock for writing pixels without preserving current contents.
        ///
        /// The backend may skip downloading the existing pixel data, so the
        /// returned view is guaranteed only to be writable. Reading pixels before
        /// writing them — or leaving any pixel in the locked region unwritten —
        /// produces undefined contents on commit.
        ///
        /// @tparam TPixel  Must match this texture's format; otherwise falsy.
        /// @param region   Sub-rectangle to lock (clamped to level bounds).
        /// @param level    Mip level (0 = base).
        template <pixel TPixel>
        [[nodiscard]] locked_texture_region<TPixel, texture_lock_mode::write_only>
        lock_write_only(std::optional<rect_i> region = {}, int level = 0) {
            return locked_texture_region<TPixel, texture_lock_mode::write_only>(
                lock_impl(region, level, TPixel::format_id, texture_lock_mode::write_only));
        }

        /// Regenerate all mip levels from level 0. No-op if @c mip_levels() == 1.
        void generate_mipmaps();

        /// Download the contents of a mip level into a newly-allocated @c bitmap.
        [[nodiscard]] bitmap download(int level = 0) const;

        /// GPU-to-GPU deep copy, including all mip levels and sampler state.
        [[nodiscard]] texture clone() const;

        /// Temporarily bind a mip level as the active render target.
        [[nodiscard]] scoped_texture_render_target as_render_target(int level = 0);

        // ── Backend access (internal) ────────────────────────────────────

        texture_handle *impl() noexcept {
            return handle_;
        }
        const texture_handle *impl() const noexcept {
            return handle_;
        }
        const graphics_backend_interface *backend() const noexcept {
            return backend_;
        }
        device_handle *device() const noexcept {
            return device_;
        }

    private:
        friend void copy_render_target_to_texture(texture &dst, rect_i src_rect, vec2i dst_pos, int dst_level);

        std::unique_ptr<detail::texture_lock_state> lock_impl(
            const std::optional<rect_i> &region, int level, pixel_format expected_fmt, texture_lock_mode mode
        );

        explicit texture(
            texture_handle *handle,
            const graphics_backend_interface *backend,
            device_handle *device,
            texture_role role,
            texture_usage usage
        ) noexcept
            : handle_(handle), backend_(backend), device_(device), role_(role), usage_(usage) {}

        texture_handle *handle_ = nullptr;
        const graphics_backend_interface *backend_ = nullptr;
        device_handle *device_ = nullptr;
        texture_role role_ = texture_role::color;
        texture_usage usage_ = texture_usage::sampling_only;
    };

    void copy_render_target_to_texture(texture &dst, rect_i src_rect, vec2i dst_pos = {}, int dst_level = 0);

} // namespace alia

#endif /* TEXTURE_D1DD8DA1_4AF1_4438_BF28_BC459807D50F */
