#ifndef TEXTURE_B1D24750_1005_447A_9A4B_D0DF4C2E7AD7
#define TEXTURE_B1D24750_1005_447A_9A4B_D0DF4C2E7AD7

#include "bitmap.hpp"
#include "gfx_device.hpp"
#include <optional>

namespace alia {

/// Sentinel for @c mip_levels constructor arg meaning "full chain down to 1×1".
/// Pass @c 1 for a mip-less texture, or an explicit count for a partial chain.
inline constexpr int full_mip_chain = 0;

// ── Internal lock state (shared by locked_texture_region<TPixel>) ─────

namespace detail {
    struct texture_lock_state {
        texture_impl*    tex  = nullptr;  // non-owning; must outlive this struct
        texture_lock_info info = {};
    };
} // namespace detail

// ── locked_texture_region ─────────────────────────────────────────────

/// @brief RAII handle granting CPU read/write access to a texture mip level.
///
/// Obtained from @c texture::lock<TPixel>(). On destruction (or when
/// @c release() is called) the modified region is committed back to GPU
/// memory (for backends that stage through CPU, e.g. OpenGL).
///
/// Evaluates to @c true if the lock succeeded. Use:
/// @code
///   if (auto reg = tex.lock<px_rgba8888>()) {
///       (*reg)[x, y] = px_rgba8888{255, 0, 0, 255};
///   } // committed on scope exit
/// @endcode
///
/// Format mismatch or an out-of-range region produces a falsy handle;
/// no GPU access occurs.
template<pixel TPixel>
class locked_texture_region {
public:
    locked_texture_region() = default;

    ~locked_texture_region() { release(); }

    locked_texture_region(locked_texture_region&&) noexcept = default;

    locked_texture_region& operator=(locked_texture_region&& o) noexcept {
        if (this != &o) { release(); impl_ = std::move(o.impl_); }
        return *this;
    }

    locked_texture_region(const locked_texture_region&)            = delete;
    locked_texture_region& operator=(const locked_texture_region&) = delete;

    /// @c true if the lock succeeded; @c false on format mismatch or backend error.
    [[nodiscard]] explicit operator bool() const noexcept { return impl_ != nullptr; }

    /// @returns A typed view over the locked region. Undefined if @c !*this.
    [[nodiscard]] bitmap_view<TPixel> view() noexcept {
        return bitmap_view<TPixel>(
            impl_->info.extent.x, impl_->info.extent.y,
            impl_->info.stride_bytes, impl_->info.data);
    }

    /// Shorthand for @c view().
    [[nodiscard]] bitmap_view<TPixel> operator*() noexcept { return view(); }

    /// @returns The top-left pixel coordinate of the locked region within the texture level.
    [[nodiscard]] vec2i origin() const noexcept { return impl_->info.origin; }

    /// Commit changes and release the lock early.
    /// The destructor calls this automatically; safe to call more than once.
    void release() {
        if (impl_) {
            impl_->tex->unlock(impl_->info, true);
            impl_.reset();
        }
    }

private:
    friend class texture;
    std::unique_ptr<detail::texture_lock_state> impl_;
    explicit locked_texture_region(std::unique_ptr<detail::texture_lock_state> s) noexcept
        : impl_(std::move(s)) {}
};

// ── texture ───────────────────────────────────────────────────────────

/// @brief GPU-resident 2D pixel buffer. Hardware counterpart of @c bitmap.
///
/// Owns a backend-specific GPU resource. Move-only; use @c clone() for a
/// GPU-to-GPU deep copy. Created through a @c gfx_device.
class texture {
public:
    texture() = default;
    ~texture() = default;
    texture(texture&&) noexcept = default;
    texture& operator=(texture&&) noexcept = default;
    texture(const texture&)            = delete;
    texture& operator=(const texture&) = delete;

    // ── Construction ─────────────────────────────────────────────────

    /// Create an uninitialised texture.
    /// @param mip_levels  @c 1 = no mipmaps, @c full_mip_chain = compute full
    ///                    chain, or an explicit positive count.
    texture(gfx_device& device, pixel_format fmt, vec2i size, int mip_levels = 1);

    /// Create and initialise level 0 from a type-erased CPU view.
    texture(gfx_device& device, const any_bitmap_view& src, int mip_levels = 1);

    /// Create and initialise level 0 from an owning bitmap.
    texture(gfx_device& device, const bitmap& src, int mip_levels = 1);

    /// Create and initialise level 0 from a typed CPU view.
    template<pixel TPixel>
    texture(gfx_device& device, const bitmap_view<TPixel>& src, int mip_levels = 1)
        : texture(device, bitmap(src), mip_levels)
    {}

    // ── State queries ────────────────────────────────────────────────

    [[nodiscard]] bool         valid()      const noexcept { return impl_ != nullptr; }
    [[nodiscard]] explicit     operator bool() const noexcept { return valid(); }
    [[nodiscard]] pixel_format format()     const noexcept;
    [[nodiscard]] int          width()      const noexcept;
    [[nodiscard]] int          height()     const noexcept;
    [[nodiscard]] vec2i        size()       const noexcept { return {width(), height()}; }
    [[nodiscard]] int          mip_levels() const noexcept;

    // ── Sampler state ────────────────────────────────────────────────

    void                        set_sampler(const sampler_state& s);
    [[nodiscard]] sampler_state sampler()   const noexcept;

    // ── CPU access ───────────────────────────────────────────────────

    /// Acquire a typed lock for reading and writing pixels.
    ///
    /// @tparam TPixel  Must satisfy the @c pixel concept and have
    ///                 @c TPixel::format_id matching this texture's format;
    ///                 otherwise returns a falsy handle.
    /// @param region   Sub-rectangle to lock (clamped to level bounds).
    ///                 Omit to lock the entire level.
    /// @param level    Mip level (0 = base).
    template<pixel TPixel>
    [[nodiscard]] locked_texture_region<TPixel>
    lock(std::optional<rect_i> region = {}, int level = 0) {
        return locked_texture_region<TPixel>(
            lock_impl(region, level, TPixel::format_id));
    }

    /// Regenerate all mip levels from level 0. No-op if @c mip_levels() == 1.
    void generate_mipmaps();

    /// Download the contents of a mip level into a newly-allocated @c bitmap.
    [[nodiscard]] bitmap download(int level = 0) const;

    /// GPU-to-GPU deep copy, including all mip levels and sampler state.
    [[nodiscard]] texture clone() const;

    // ── Backend access (internal) ────────────────────────────────────

    texture_impl*       impl()       noexcept { return impl_.get(); }
    const texture_impl* impl() const noexcept { return impl_.get(); }

private:
    // Non-template core of lock<TPixel>(); validates the format and delegates
    // to the backend. Returns nullptr on format mismatch or backend failure.
    std::unique_ptr<detail::texture_lock_state>
    lock_impl(const std::optional<rect_i>& region, int level, pixel_format expected_fmt);

    explicit texture(std::unique_ptr<texture_impl> impl) noexcept
        : impl_(std::move(impl)) {}

    std::unique_ptr<texture_impl> impl_;
};

} // namespace alia

#endif /* TEXTURE_B1D24750_1005_447A_9A4B_D0DF4C2E7AD7 */
