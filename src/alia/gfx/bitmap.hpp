#ifndef BITMAP_AB97234D_69C8_4EC9_B9FC_3800633E0EEC
#define BITMAP_AB97234D_69C8_4EC9_B9FC_3800633E0EEC

#include "pixel.hpp"
#include <alia/core/vec.hpp>
#include <alia/core/rect.hpp>
#include <memory>
#include <span>
#include <cstddef>
#include <cstring>
#include <stdexcept>

namespace alia {

	/// Sentinel value for @c stride_bytes parameters indicating that pixels are
	/// tightly packed (stride = width * bytes_per_pixel).
	inline constexpr int tight_pixel_packing = 0;

	// forward declarations
	class bitmap;
	template<pixel TPixel> class bitmap_view;

	/**
	 * @brief A non-owning, type-erased view into a 2D pixel buffer.
	 *
	 * Does not own the data; the pointed-to buffer must outlive all views into it.
	 * Copy and move are defaulted.
	 */
	class any_bitmap_view {
	public:
		any_bitmap_view() = default;

		/**
		 * @brief Constructs a view over an existing pixel buffer.
		 * @param fmt          Pixel format of the data.
		 * @param width        Width of the view in pixels.
		 * @param height       Height of the view in pixels.
		 * @param stride_bytes Row stride in bytes (may exceed @c width * bpp).
		 * @param data         Pointer to the first pixel of the top-left corner.
		 */
		any_bitmap_view(pixel_format fmt, int width, int height, int stride_bytes, std::byte* data) noexcept
			: pixel_format_(fmt), width_(width), height_(height), stride_bytes_(stride_bytes), data_(data)
		{}

		/// @returns The pixel format of this view.
		[[nodiscard]] pixel_format format()       const noexcept { return pixel_format_; }
		/// @returns Width of the view in pixels.
		[[nodiscard]] int          width()         const noexcept { return width_; }
		/// @returns Height of the view in pixels.
		[[nodiscard]] int          height()        const noexcept { return height_; }
		/// @returns Row stride in bytes.
		[[nodiscard]] int          stride_bytes()  const noexcept { return stride_bytes_; }

		/**
		 * @brief Returns a pointer to the start of scanline @p y.
		 *
		 * Propagates constness of the view object: a const view returns
		 * @c const @c void*, a mutable view returns @c void*.
		 *
		 * @param y Scanline index (0 = top).
		 * @returns Pointer to the first pixel of the scanline.
		 */
		auto* line(this auto&& self, int y) noexcept {
			using SelfT = std::remove_reference_t<decltype(self)>;
			auto* raw = self.data_ + y * self.stride_bytes_;
			if constexpr (std::is_const_v<SelfT>)
				return static_cast<const void*>(raw);
			else
				return static_cast<void*>(raw);
		}

		/**
		 * @brief Returns a sub-view into @p region, clamped to this view's bounds.
		 *
		 * The sub-view shares the same backing buffer; no data is copied.
		 *
		 * @param region Desired sub-region (clamped to the view's bounds).
		 * @returns A new @c any_bitmap_view into the clamped region.
		 */
		any_bitmap_view subview(this auto&& self, rect_i region) noexcept {
			const rect_i bounds{{0, 0}, {self.width_, self.height_}};
			const rect_i clamped = bounds.intersection_with(region);
			const int bpp = bytes_per_pixel_for_format(self.pixel_format_);
			std::byte* ptr = self.data_
				+ clamped.top()  * self.stride_bytes_
				+ clamped.left() * bpp;
			return any_bitmap_view(
				self.pixel_format_,
				clamped.width(), clamped.height(),
				self.stride_bytes_,
				ptr
			);
		}

	private:
		pixel_format pixel_format_ = pixel_format::rgb888;
		int          width_        = 0;
		int          height_       = 0;
		int          stride_bytes_ = 0;
		std::byte*   data_         = nullptr;
	};

	/**
	 * @brief A non-owning, type-safe view into a 2D pixel buffer.
	 *
	 * Does not own the data; the pointed-to buffer must outlive all views into it.
	 * Copy and move are defaulted.
	 *
	 * @tparam TPixel Pixel type. Must satisfy the @c pixel concept.
	 */
	template<pixel TPixel>
	class bitmap_view {
	public:
		bitmap_view() = default;

		/**
		 * @brief Constructs a view over an existing pixel buffer.
		 * @param width        Width of the view in pixels.
		 * @param height       Height of the view in pixels.
		 * @param stride_bytes Row stride in bytes (may exceed @c width * sizeof(TPixel)).
		 * @param data         Pointer to the first pixel of the top-left corner.
		 */
		bitmap_view(int width, int height, int stride_bytes, std::byte* data) noexcept
			: width_(width), height_(height), stride_bytes_(stride_bytes), data_(data)
		{}

		/// @returns Width of the view in pixels.
		[[nodiscard]] int width()        const noexcept { return width_; }
		/// @returns Height of the view in pixels.
		[[nodiscard]] int height()       const noexcept { return height_; }
		/// @returns Row stride in bytes.
		[[nodiscard]] int stride_bytes() const noexcept { return stride_bytes_; }

		/**
		 * @brief Returns a reference to the pixel at (@p x, @p y). No bounds checking.
		 *
		 * Propagates constness of the view object.
		 *
		 * @param x Column index.
		 * @param y Row index.
		 * @returns Reference to the pixel at (x, y).
		 */
		auto&& operator[](this auto&& self, int x, int y) noexcept {
			using SelfT = std::remove_reference_t<decltype(self)>;
			auto* row = self.data_ + y * self.stride_bytes_ + x * static_cast<int>(sizeof(TPixel));
			if constexpr (std::is_const_v<SelfT>)
				return *reinterpret_cast<const TPixel*>(row);
			else
				return *reinterpret_cast<TPixel*>(row);
		}

		/**
		 * @brief Returns a span over scanline @p y.
		 *
		 * Propagates constness: a const view returns @c std::span<const TPixel>,
		 * a mutable view returns @c std::span<TPixel>.
		 *
		 * @param y Scanline index (0 = top).
		 * @returns Span of @c width() pixels starting at the beginning of the scanline.
		 */
		auto line(this auto&& self, int y) noexcept {
			using SelfT = std::remove_reference_t<decltype(self)>;
			if constexpr (std::is_const_v<SelfT>)
				return std::span<const TPixel>(
					reinterpret_cast<const TPixel*>(self.data_ + y * self.stride_bytes_),
					self.width_
				);
			else
				return std::span<TPixel>(
					reinterpret_cast<TPixel*>(self.data_ + y * self.stride_bytes_),
					self.width_
				);
		}

		/**
		 * @brief Bounds-checked pixel access.
		 * @param x Column index.
		 * @param y Row index.
		 * @returns Reference to the pixel at (x, y).
		 * @throws std::out_of_range if (x, y) is outside the view's bounds.
		 */
		auto&& at(this auto&& self, int x, int y) {
			if (x < 0 || x >= self.width_ || y < 0 || y >= self.height_)
				throw std::out_of_range("bitmap_view::at: index out of bounds");
			return self[x, y];
		}

		/**
		 * @brief Returns a sub-view into @p region, clamped to this view's bounds.
		 *
		 * The sub-view shares the same backing buffer; no data is copied.
		 *
		 * @param region Desired sub-region (clamped to the view's bounds).
		 * @returns A new @c bitmap_view<TPixel> into the clamped region.
		 */
		bitmap_view subview(this auto&& self, rect_i region) noexcept {
			const rect_i bounds{{0, 0}, {self.width_, self.height_}};
			const rect_i clamped = bounds.intersection_with(region);
			std::byte* ptr = self.data_
				+ clamped.top()  * self.stride_bytes_
				+ clamped.left() * static_cast<int>(sizeof(TPixel));
			return bitmap_view(clamped.width(), clamped.height(), self.stride_bytes_, ptr);
		}

	private:
		int        width_        = 0;
		int        height_       = 0;
		int        stride_bytes_ = 0;
		std::byte* data_         = nullptr;
	};

	/**
	 * @brief Owns a 2D pixel buffer stored top-to-bottom, left-to-right.
	 *
	 * Pixels within each row are tightly packed unless an explicit stride was
	 * supplied at construction. Copy construction is deleted; use @c clone() for
	 * explicit deep copies. Move construction and assignment are available.
	 */
	class bitmap {
	public:
		/**
		 * @brief Constructs a bitmap by copying raw bytes.
		 * @param fmt          Pixel format of the data.
		 * @param size         Dimensions of the bitmap in pixels.
		 * @param data         Source bytes (copied into an internally-owned buffer).
		 * @param stride_bytes Row stride of the source data. Pass @c tight_pixel_packing
		 *                     (the default) for tightly packed source data.
		 */
		bitmap(pixel_format fmt, vec2i size, std::span<const std::byte> data, int stride_bytes = tight_pixel_packing)
			: pixel_format_(fmt)
			, width_(size.x)
			, height_(size.y)
			, stride_bytes_(stride_bytes > 0 ? stride_bytes : size.x * bytes_per_pixel_for_format(fmt))
			, data_(std::make_unique<std::byte[]>(static_cast<std::size_t>(stride_bytes_) * size.y))
		{
			std::memcpy(data_.get(), data.data(), static_cast<std::size_t>(stride_bytes_) * height_);
		}

		/**
		 * @brief Constructs a bitmap by copying a typed pixel span.
		 *
		 * The span is treated as a flat, tightly packed row-major array of pixels.
		 *
		 * @param size   Dimensions of the bitmap in pixels.
		 * @param pixels Source pixels; must contain exactly @c size.x * size.y elements.
		 */
		template<pixel TPixel>
		bitmap(vec2i size, std::span<const TPixel> pixels)
			: pixel_format_(TPixel::format_id)
			, width_(size.x)
			, height_(size.y)
			, stride_bytes_(size.x * static_cast<int>(sizeof(TPixel)))
			, data_(std::make_unique<std::byte[]>(static_cast<std::size_t>(stride_bytes_) * size.y))
		{
			std::memcpy(data_.get(), pixels.data(), static_cast<std::size_t>(stride_bytes_) * height_);
		}

		/**
		 * @brief Constructs a bitmap from an @c any_bitmap_view.
		 *
		 * Data is copied row-by-row with tight packing, regardless of the source stride.
		 *
		 * @param view Source view to copy from.
		 */
		explicit bitmap(const any_bitmap_view& view)
			: pixel_format_(view.format())
			, width_(view.width())
			, height_(view.height())
			, stride_bytes_(view.width() * bytes_per_pixel_for_format(view.format()))
			, data_(std::make_unique<std::byte[]>(static_cast<std::size_t>(stride_bytes_) * height_))
		{
			for (int y = 0; y < height_; ++y)
				std::memcpy(data_.get() + y * stride_bytes_, view.line(y), stride_bytes_);
		}

		/**
		 * @brief Constructs a bitmap from a @c bitmap_view<TPixel>.
		 *
		 * Data is copied row-by-row with tight packing, regardless of the source stride.
		 *
		 * @tparam TPixel Pixel type of the source view.
		 * @param  view   Source view to copy from.
		 */
		template<pixel TPixel>
		explicit bitmap(const bitmap_view<TPixel>& view)
			: pixel_format_(TPixel::format_id)
			, width_(view.width())
			, height_(view.height())
			, stride_bytes_(view.width() * static_cast<int>(sizeof(TPixel)))
			, data_(std::make_unique<std::byte[]>(static_cast<std::size_t>(stride_bytes_) * height_))
		{
			for (int y = 0; y < height_; ++y)
				std::memcpy(data_.get() + y * stride_bytes_, view.line(y).data(), stride_bytes_);
		}

		/**
		 * @brief Constructs a solid-color bitmap filled with the given pixel value.
		 * @param size  Dimensions of the bitmap in pixels.
		 * @param color Fill color (default-constructed if omitted).
		 */
		template<pixel TPixel>
		bitmap(vec2i size, TPixel color = {})
			: pixel_format_(TPixel::format_id)
			, width_(size.x)
			, height_(size.y)
			, stride_bytes_(size.x * static_cast<int>(sizeof(TPixel)))
			, data_(std::make_unique<std::byte[]>(static_cast<std::size_t>(stride_bytes_) * size.y))
		{
			auto* pixels = reinterpret_cast<TPixel*>(data_.get());
			const std::size_t n = static_cast<std::size_t>(width_) * height_;
			for (std::size_t i = 0; i < n; ++i)
				pixels[i] = color;
		}

		bitmap(const bitmap&)            = delete;
		bitmap& operator=(const bitmap&) = delete;
		bitmap(bitmap&&)                 = default;
		bitmap& operator=(bitmap&&)      = default;

		/// @returns The pixel format of this bitmap.
		[[nodiscard]] pixel_format format()       const noexcept { return pixel_format_; }
		/// @returns Width in pixels.
		[[nodiscard]] int          width()         const noexcept { return width_; }
		/// @returns Height in pixels.
		[[nodiscard]] int          height()        const noexcept { return height_; }
		/// @returns Row stride in bytes.
		[[nodiscard]] int          stride_bytes()  const noexcept { return stride_bytes_; }

		/// @returns A deep copy of this bitmap.
		[[nodiscard]] bitmap clone() const {
			const std::size_t total = static_cast<std::size_t>(stride_bytes_) * height_;
			auto data = std::make_unique<std::byte[]>(total);
			std::memcpy(data.get(), data_.get(), total);
			return bitmap(pixel_format_, width_, height_, stride_bytes_, std::move(data));
		}

		/**
		 * @brief Returns a raw pointer to the start of scanline @p y.
		 *
		 * Propagates constness: a const bitmap returns @c const @c void*,
		 * a mutable bitmap returns @c void*.
		 *
		 * @param y Scanline index (0 = top).
		 * @returns Pointer to the first pixel of the scanline.
		 */
		auto* line(this auto&& self, int y) noexcept {
			using SelfT = std::remove_reference_t<decltype(self)>;
			auto* raw = self.data_.get() + y * self.stride_bytes_;
			if constexpr (std::is_const_v<SelfT>)
				return static_cast<const void*>(raw);
			else
				return static_cast<void*>(raw);
		}

		/**
		 * @brief Returns a type-erased view into this bitmap.
		 * @returns An @c any_bitmap_view pointing to this bitmap's data.
		 */
		any_bitmap_view view(this auto&& self) noexcept {
			return any_bitmap_view(
				self.pixel_format_,
				self.width_, self.height_, self.stride_bytes_,
				self.data_.get()
			);
		}

		/**
		 * @brief Returns a typed view into this bitmap.
		 * @tparam TPixel Desired pixel type.
		 * @returns A @c bitmap_view<TPixel> pointing to this bitmap's data.
		 * @throws std::invalid_argument if @c TPixel::format_id doesn't match this bitmap's format.
		 */
		template<pixel TPixel>
		bitmap_view<TPixel> view_as(this auto&& self) {
			if (self.pixel_format_ != TPixel::format_id)
				throw std::invalid_argument("bitmap::view_as: pixel format mismatch");
			return bitmap_view<TPixel>(self.width_, self.height_, self.stride_bytes_, self.data_.get());
		}

	private:
		bitmap(pixel_format fmt, int w, int h, int stride, std::unique_ptr<std::byte[]> data) noexcept
			: pixel_format_(fmt), width_(w), height_(h), stride_bytes_(stride), data_(std::move(data))
		{}

		pixel_format                 pixel_format_ = pixel_format::rgb888;
		int                          width_        = 0;
		int                          height_       = 0;
		int                          stride_bytes_ = 0;
		std::unique_ptr<std::byte[]> data_;
	};

} // namespace alia

#endif /* BITMAP_AB97234D_69C8_4EC9_B9FC_3800633E0EEC */
