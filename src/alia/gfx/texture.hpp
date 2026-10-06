#ifndef TEXTURE_D1DD8DA1_4AF1_4438_BF28_BC459807D50F
#define TEXTURE_D1DD8DA1_4AF1_4438_BF28_BC459807D50F

#include "bitmap/bitmap.hpp"
#include "gfx_device.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace alia {

    /// Sentinel for @c mip_levels in texture configs meaning "full chain down to 1×1".
    /// Pass @c 1 for a mip-less texture, or an explicit count for a partial chain.
    inline constexpr int full_mip_chain = 0;

    /// Every backend supports bgra8888 color textures and gray_u8 alpha masks.
    struct texture_config {
        /// Set: stored exactly; source pixels are converted (lossy allowed).
        /// Throws if the device cannot store the requested format.
        /// Empty: uploads use the source format or the closest supported one;
        /// uninitialised textures use bgra8888 (color) or gray_u8 (alpha_mask).
        std::optional<pixel_format> format;
        int mip_levels = 1;
        texture_role role = texture_role::color;
        texture_usage usage = texture_usage::sampling_only;
    };

    // ── Internal lock state (shared by typed and untyped regions) ─────────

    namespace detail {
        struct texture_lock_state {
            texture_handle *handle = nullptr;
            const graphics_backend_interface *backend = nullptr;
            texture_lock_info info = {};
            pixel_format format = pixel_format::bgra8888;
        };

        template <texture_lock_mode Mode>
        class texture_lock_holder {
        public:
            texture_lock_holder() = default;
            explicit texture_lock_holder(std::unique_ptr<texture_lock_state> state) noexcept
                : state_(std::move(state)) {}
            ~texture_lock_holder() {
                release();
            }
            texture_lock_holder(texture_lock_holder &&) noexcept = default;
            texture_lock_holder &operator=(texture_lock_holder &&other) noexcept {
                if (this != &other) {
                    release();
                    state_ = std::move(other.state_);
                }
                return *this;
            }
            texture_lock_holder(const texture_lock_holder &) = delete;
            texture_lock_holder &operator=(const texture_lock_holder &) = delete;

            [[nodiscard]] explicit operator bool() const noexcept {
                return state_ != nullptr;
            }
            [[nodiscard]] const texture_lock_state *get() const noexcept {
                return state_.get();
            }
            void release() {
                if (state_) {
                    constexpr bool wrote = Mode != texture_lock_mode::read_only;
                    state_->backend->texture_unlock.get_or_throw()(state_->handle, state_->info, wrote);
                    state_.reset();
                }
            }

        private:
            std::unique_ptr<texture_lock_state> state_;
        };

        bool upload_bitmap_view(
            texture_handle *,
            const graphics_backend_interface *,
            const any_bitmap_view &,
            std::optional<cube_face> face = {}
        );
        void validate_texture_desc(
            pixel_format, vec2i, int mip_levels, const char *operation
        );

        [[nodiscard]] std::span<const pixel_format> texture_format_fallbacks(pixel_format, texture_role);

        template <class F>
        [[nodiscard]] std::optional<pixel_format> closest_texture_format(
            pixel_format requested, texture_role role, F &&supported
        ) {
            if (std::invoke(supported, requested))
                return requested;
            for (const auto fallback : texture_format_fallbacks(requested, role)) {
                if (std::invoke(supported, fallback))
                    return fallback;
            }
            return std::nullopt;
        }

    } // namespace detail

    // ── locked_texture_region ─────────────────────────────────────────────

    /// @brief RAII handle granting CPU access to a texture mip level.
    ///
    /// Obtained from the corresponding @c texture or @c cube_texture lock
    /// method (read-write, read-only, or write-only).
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

        locked_texture_region(locked_texture_region &&other) noexcept
            : holder_(std::move(other.holder_)), view_(std::exchange(other.view_, {})) {}

        locked_texture_region &operator=(locked_texture_region &&o) noexcept {
            if (this != &o) {
                holder_ = std::move(o.holder_);
                view_ = std::exchange(o.view_, {});
            }
            return *this;
        }

        locked_texture_region(const locked_texture_region &) = delete;
        locked_texture_region &operator=(const locked_texture_region &) = delete;

        /// @c true if the lock succeeded; @c false on format mismatch or backend error.
        [[nodiscard]] explicit operator bool() const noexcept {
            return static_cast<bool>(holder_);
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
            return holder_.get()->info.origin;
        }

        /// Commit changes (unless this was a read-only lock) and release the lock early.
        /// The destructor calls this automatically; safe to call more than once.
        void release() {
            holder_.release();
            view_ = {};
        }

    private:
        friend class texture;
        friend class cube_texture;
        detail::texture_lock_holder<Mode> holder_;
        bitmap_view<TPixel> view_{};

        explicit locked_texture_region(std::unique_ptr<detail::texture_lock_state> s) noexcept
            : holder_(std::move(s))
            , view_(holder_
                ? bitmap_view<TPixel>(holder_.get()->info.extent.x, holder_.get()->info.extent.y,
                                      holder_.get()->info.stride_bytes, holder_.get()->info.data)
                : bitmap_view<TPixel>{}) {}
    };

    /// RAII CPU access to a texture region in its runtime pixel format.
    /// Destruction or release() commits writes unless Mode is read_only.
    /// Views and row spans are valid only while the region remains locked.
    /// Except for operator bool(), release(), and the visitors (which return
    /// false), accessing a failed or released lock is undefined.
    template <texture_lock_mode Mode = texture_lock_mode::read_write>
    class any_locked_texture_region {
    public:
        any_locked_texture_region() = default;
        any_locked_texture_region(any_locked_texture_region &&) noexcept = default;
        any_locked_texture_region &operator=(any_locked_texture_region &&) noexcept = default;
        any_locked_texture_region(const any_locked_texture_region &) = delete;
        any_locked_texture_region &operator=(const any_locked_texture_region &) = delete;

        [[nodiscard]] explicit operator bool() const noexcept {
            return static_cast<bool>(holder_);
        }
        [[nodiscard]] pixel_format format() const noexcept {
            return holder_.get()->format;
        }
        [[nodiscard]] vec2i origin() const noexcept {
            return holder_.get()->info.origin;
        }
        [[nodiscard]] vec2i size() const noexcept {
            return holder_.get()->info.extent;
        }

        /// Native byte view, e.g. for converting_blit_lossy(view(), src).
        /// Read-only constness is not enforced through any_bitmap_view.
        [[nodiscard]] any_bitmap_view view() noexcept {
            const auto &state = *holder_.get();
            return any_bitmap_view(state.format, state.info.extent.x, state.info.extent.y,
                                   state.info.stride_bytes, state.info.data);
        }

        /// Calls f(span<THub>, y) for each row, or span<const THub> for read-only.
        /// Matching formats share locked memory; other formats convert through
        /// one reusable row whose span is valid only during the callback.
        /// Returns false for palette/unknown formats, or for read-write float
        /// storage through px_rgba8888. Use px_rgba_f32 for float storage.
        template <rgba_hub_pixel THub, class F>
        bool visit_rows(F &&f) {
            if (!holder_)
                return false;
            const auto &state = *holder_.get();
            return detail::visit_pixel_rows<THub, Mode != texture_lock_mode::write_only,
                                           Mode != texture_lock_mode::read_only>(
                state.format, state.info.data, state.info.stride_bytes, state.info.extent, std::forward<F>(f));
        }

        /// Calls f with a native bitmap_view<P>&, or const bitmap_view<P>& for
        /// read-only. No copy; returns false if f does not accept this format.
        template <class F>
        bool visit(F &&f) {
            if (!holder_)
                return false;
            const auto &state = *holder_.get();
            return detail::visit_pixel_view<Mode == texture_lock_mode::read_only>(
                state.format, state.info.data, state.info.stride_bytes, state.info.extent, std::forward<F>(f));
        }

        /// Commit writes and release early. Safe to call more than once.
        void release() {
            holder_.release();
        }

    private:
        friend class texture;
        friend class cube_texture;
        explicit any_locked_texture_region(std::unique_ptr<detail::texture_lock_state> state) noexcept
            : holder_(std::move(state)) {}
        detail::texture_lock_holder<Mode> holder_;
    };

    // ── texture ───────────────────────────────────────────────────────────

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
            vec2i size,
            const texture_config &config = {}
        );

        /// Create and initialise level 0 from a type-erased CPU view.
        texture(
            gfx_device &device,
            const any_bitmap_view &src,
            const texture_config &config = {}
        );

        /// Create and initialise level 0 from an owning bitmap.
        texture(
            gfx_device &device,
            const bitmap &src,
            const texture_config &config = {}
        );

        /// Create and initialise level 0 from a typed CPU view.
        template <pixel TPixel>
        texture(
            gfx_device &device,
            const bitmap_view<TPixel> &src,
            const texture_config &config = {}
        )
            : texture(device, bitmap(src), config) {}

        /// Query the exact configured format, or the uninitialised default if
        /// omitted. Checks format/mip/role/usage support without allocating;
        /// dimensions and allocation success are not covered by this query.
        [[nodiscard]] static bool can_create(const gfx_device &, const texture_config &);

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
        /// @param level    Mip level (0 = base). Invalid levels/empty regions return a falsy handle.
        template <pixel TPixel>
        [[nodiscard]] locked_texture_region<TPixel, texture_lock_mode::read_write>
        lock(std::optional<rect_i> region = {}, int level = 0) {
            return locked_texture_region<TPixel, texture_lock_mode::read_write>(
                lock_impl(region, level, TPixel::format_id, texture_lock_mode::read_write));
        }

        /// Acquire a typed lock to inspect pixels without modifying them.
        ///
        /// The returned view is const and release skips the upload step.
        ///
        /// @tparam TPixel  Must match this texture's format; otherwise falsy.
        /// @param region   Sub-rectangle to lock (clamped to level bounds).
        /// @param level    Mip level (0 = base). Invalid levels/empty regions return a falsy handle.
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
        /// @param level    Mip level (0 = base). Invalid levels/empty regions return a falsy handle.
        template <pixel TPixel>
        [[nodiscard]] locked_texture_region<TPixel, texture_lock_mode::write_only>
        lock_write_only(std::optional<rect_i> region = {}, int level = 0) {
            return locked_texture_region<TPixel, texture_lock_mode::write_only>(
                lock_impl(region, level, TPixel::format_id, texture_lock_mode::write_only));
        }

        /// Acquire a runtime-format lock for reading and writing pixels.
        /// @param region Sub-rectangle clamped to level bounds; omit for the entire level.
        /// @param level Mip level (0 = base). Invalid levels/empty regions return a falsy handle.
        [[nodiscard]] any_locked_texture_region<texture_lock_mode::read_write>
        lock_any(std::optional<rect_i> region = {}, int level = 0) {
            return any_locked_texture_region<texture_lock_mode::read_write>(
                lock_impl(region, level, std::nullopt, texture_lock_mode::read_write));
        }

        /// Acquire a runtime-format lock to inspect pixels without modifying them.
        /// Visitors enforce read-only access; release skips the upload step.
        /// @param region Sub-rectangle clamped to level bounds; omit for the entire level.
        /// @param level Mip level (0 = base). Invalid levels/empty regions return a falsy handle.
        [[nodiscard]] any_locked_texture_region<texture_lock_mode::read_only>
        lock_any_read_only(std::optional<rect_i> region = {}, int level = 0) {
            return any_locked_texture_region<texture_lock_mode::read_only>(
                lock_impl(region, level, std::nullopt, texture_lock_mode::read_only));
        }

        /// Acquire a runtime-format lock without preserving current contents.
        /// The backend may skip downloading pixels. Write every locked pixel;
        /// reading unwritten pixels or leaving them unwritten gives undefined contents.
        /// @param region Sub-rectangle clamped to level bounds; omit for the entire level.
        /// @param level Mip level (0 = base). Invalid levels/empty regions return a falsy handle.
        [[nodiscard]] any_locked_texture_region<texture_lock_mode::write_only>
        lock_any_write_only(std::optional<rect_i> region = {}, int level = 0) {
            return any_locked_texture_region<texture_lock_mode::write_only>(
                lock_impl(region, level, std::nullopt, texture_lock_mode::write_only));
        }

        /// Regenerate all mip levels from level 0. No-op if @c mip_levels() == 1.
        void generate_mipmaps();

        /// Download the contents of a mip level into a newly-allocated @c bitmap.
        [[nodiscard]] bitmap download(int level = 0) const;

        /// GPU-to-GPU deep copy, including all mip levels and sampler state.
        [[nodiscard]] texture clone() const;

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
        std::unique_ptr<detail::texture_lock_state> lock_impl(
            const std::optional<rect_i> &region, int level, std::optional<pixel_format> expected_fmt, texture_lock_mode mode
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

} // namespace alia

#endif /* TEXTURE_D1DD8DA1_4AF1_4438_BF28_BC459807D50F */
