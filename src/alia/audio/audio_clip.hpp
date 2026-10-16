#ifndef ALIA_AUDIO_AUDIO_CLIP_HPP
#define ALIA_AUDIO_AUDIO_CLIP_HPP

#include "sample.hpp"
#include <algorithm>
#include <bit>
#include <memory>
#include <span>
#include <utility>

namespace alia {
    /// Zero for signed/floating points, midpoint for offset-binary unsigned PCM.
    template <sample_point T>
    constexpr T silence_of() noexcept {
        if constexpr (std::unsigned_integral<T>)
            return T(1) << (sizeof(T) * 8 - 1);
        else
            return T(0);
    }

    namespace detail {
        template <sample_frame F>
        constexpr std::remove_cv_t<F> silent_frame() noexcept {
            std::array<sample_point_t<F>, channel_count_v<F>> points;
            points.fill(silence_of<sample_point_t<F>>());
            return std::bit_cast<std::remove_cv_t<F>>(points);
        }

        inline std::size_t audio_byte_count(sample_frame_format format, std::size_t count) {
            const auto stride = static_cast<std::size_t>(format.bytes_per_frame());
            if (count > (std::numeric_limits<std::size_t>::max)() / stride)
                throw std::length_error("audio frame count is too large");
            return count * stride;
        }
    } // namespace detail

    template <sample_frame F>
    class audio_clip_view;
    template <bool Const>
    class basic_any_audio_clip_view;
    using any_audio_clip_view = basic_any_audio_clip_view<false>;
    /// Read-only erased storage, used by capture callbacks and const clips.
    using const_audio_clip_view = basic_any_audio_clip_view<true>;

    template <bool Const>
    class basic_any_audio_clip_view {
    public:
        using byte_type = std::conditional_t<Const, const std::byte, std::byte>;
        basic_any_audio_clip_view() = default;

        basic_any_audio_clip_view(sample_frame_format format, int rate, std::size_t count, byte_type *data)
            : format_(format), rate_(rate), count_(count), data_(data) {
            detail::audio_byte_count(format, count);
            if (rate < 0 || (count && (!data || rate == 0)))
                throw std::invalid_argument("invalid audio clip view");
        }

        template <bool C>
            requires(Const && !C)
        basic_any_audio_clip_view(const basic_any_audio_clip_view<C> &other)
            : format_(other.format()), rate_(other.sample_rate()), count_(other.frame_count()),
              data_(other.bytes().data()) {
        }

        [[nodiscard]] sample_frame_format format() const noexcept {
            return format_;
        }

        [[nodiscard]] int sample_rate() const noexcept {
            return rate_;
        }

        [[nodiscard]] std::size_t frame_count() const noexcept {
            return count_;
        }

        [[nodiscard]] double duration() const noexcept {
            return rate_ > 0 ? double(count_) / rate_ : 0;
        }

        [[nodiscard]] auto bytes(this auto &&self) noexcept {
            constexpr bool read_only = Const || std::is_const_v<std::remove_reference_t<decltype(self)>>;
            using B = std::conditional_t<read_only, const std::byte, std::byte>;
            return std::span<B>(self.data_, self.count_ * self.format_.point.bytes * self.format_.channels);
        }

        [[nodiscard]] auto subview(this auto &&self, std::size_t first, std::size_t count) {
            constexpr bool read_only = Const || std::is_const_v<std::remove_reference_t<decltype(self)>>;
            first = (std::min)(first, self.count_);
            count = (std::min)(count, self.count_ - first);
            auto *ptr = self.data_;
            if (first)
                ptr += first * self.format_.bytes_per_frame();
            return basic_any_audio_clip_view<read_only>(self.format_, self.rate_, count, ptr);
        }

        /// Throws on a format or alignment mismatch; propagates read-only storage.
        template <sample_frame F>
        [[nodiscard]] auto as(this auto &&self) {
            constexpr bool read_only =
                Const || std::is_const_v<F> || std::is_const_v<std::remove_reference_t<decltype(self)>>;
            using E = std::conditional_t<read_only, const std::remove_cv_t<F>, F>;
            if (self.format_ != sample_frame_format_of_v<F>)
                throw std::invalid_argument("audio clip frame format mismatch");
            if (reinterpret_cast<std::uintptr_t>(self.data_) % alignof(F))
                throw std::invalid_argument("unaligned audio frame storage");
            return audio_clip_view<E>(self.rate_, std::span<E>(reinterpret_cast<E *>(self.data_), self.count_));
        }

    private:
        sample_frame_format format_{};
        int rate_ = 0;
        std::size_t count_ = 0;
        byte_type *data_ = nullptr;
    };

    /// A tightly packed, non-owning frame span. Constness propagates to subviews
    /// and erased views; the storage must outlive every view.
    template <sample_frame F>
    class audio_clip_view {
    public:
        audio_clip_view() = default;

        audio_clip_view(int rate, std::span<F> frames) : rate_(rate), frames_(frames) {
            if (rate < 0 || (rate == 0 && !frames.empty()))
                throw std::invalid_argument("audio sample rate must be positive");
        }

        template <sample_frame S>
            requires std::is_convertible_v<S (*)[], F (*)[]>
        audio_clip_view(const audio_clip_view<S> &other) : rate_(other.sample_rate()), frames_(other.frames_) {
        }

        [[nodiscard]] static constexpr sample_frame_format format() noexcept {
            return sample_frame_format_of_v<F>;
        }

        [[nodiscard]] int sample_rate() const noexcept {
            return rate_;
        }

        [[nodiscard]] std::size_t frame_count() const noexcept {
            return frames_.size();
        }

        [[nodiscard]] double duration() const noexcept {
            return rate_ > 0 ? double(frame_count()) / rate_ : 0;
        }

        [[nodiscard]] auto frames(this auto &&self) noexcept {
            using E = std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const F, F>;
            return std::span<E>(self.frames_.data(), self.frames_.size());
        }

        [[nodiscard]] auto bytes(this auto &&self) noexcept {
            if constexpr (std::is_const_v<F> || std::is_const_v<std::remove_reference_t<decltype(self)>>)
                return std::as_bytes(self.frames());
            else
                return std::as_writable_bytes(self.frames());
        }

        auto &&operator[](this auto &&self, std::size_t i) noexcept {
            return self.frames()[i];
        }

        auto &&at(this auto &&self, std::size_t i) {
            if (i >= self.frame_count())
                throw std::out_of_range("audio_clip_view::at");
            return self[i];
        }

        [[nodiscard]] auto subview(this auto &&self, std::size_t first, std::size_t count) {
            first = (std::min)(first, self.frame_count());
            count = (std::min)(count, self.frame_count() - first);
            using E = typename decltype(self.frames())::element_type;
            return audio_clip_view<E>(self.rate_, self.frames().subspan(first, count));
        }

        operator any_audio_clip_view()
            requires(!std::is_const_v<F>)
        {
            return {format(), rate_, frame_count(), bytes().data()};
        }

        operator const_audio_clip_view() const {
            return {format(), rate_, frame_count(), bytes().data()};
        }

    private:
        template <sample_frame>
        friend class audio_clip_view;
        int rate_ = 0;
        std::span<F> frames_;
    };
    template <class F, std::size_t N>
    audio_clip_view(int, std::span<F, N>) -> audio_clip_view<F>;

    /// Move-only owned interleaved audio. clone() explicitly copies the frames.
    /// Raw-byte constructors reject incomplete frames and nonpositive rates.
    /// Frame-count construction fills with the supplied frame (silence by default).
    class audio_clip {
    public:
        audio_clip() = default;
        audio_clip(sample_frame_format, int rate, std::span<const std::byte>);
        explicit audio_clip(const_audio_clip_view);

        explicit audio_clip(any_audio_clip_view view) : audio_clip(const_audio_clip_view(view)) {
        }

        template <sample_frame F, std::size_t N>
        audio_clip(int rate, std::span<F, N> frames)
            : audio_clip(sample_frame_format_of_v<F>, rate, std::as_bytes(frames)) {
        }

        template <sample_frame F>
        explicit audio_clip(const audio_clip_view<F> &view) : audio_clip(static_cast<const_audio_clip_view>(view)) {
        }

        template <sample_frame F = float>
        audio_clip(std::size_t count, int rate, F fill = detail::silent_frame<F>())
            : audio_clip(sample_frame_format_of_v<F>, rate, count) {
            auto frames = view_as<F>().frames();
            std::fill(frames.begin(), frames.end(), fill);
        }

        audio_clip(const audio_clip &) = delete;
        audio_clip &operator=(const audio_clip &) = delete;
        audio_clip(audio_clip &&) noexcept;
        audio_clip &operator=(audio_clip &&) noexcept;
        [[nodiscard]] audio_clip clone() const;

        [[nodiscard]] sample_frame_format format() const noexcept {
            return format_;
        }

        [[nodiscard]] int sample_rate() const noexcept {
            return rate_;
        }

        [[nodiscard]] std::size_t frame_count() const noexcept {
            return count_;
        }

        [[nodiscard]] double duration() const noexcept {
            return rate_ > 0 ? double(count_) / rate_ : 0;
        }

        [[nodiscard]] auto view(this auto &&self) {
            constexpr bool read_only = std::is_const_v<std::remove_reference_t<decltype(self)>>;
            return basic_any_audio_clip_view<read_only>(self.format_, self.rate_, self.count_, self.data_.get());
        }

        template <sample_frame F>
        [[nodiscard]] auto view_as(this auto &&self) {
            return self.view().template as<F>();
        }

        [[nodiscard]] auto bytes(this auto &&self) noexcept {
            using B = std::
                conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const std::byte, std::byte>;
            return std::span<B>(self.data_.get(), self.count_ * self.format_.point.bytes * self.format_.channels);
        }

        [[nodiscard]] auto subview(this auto &&self, std::size_t first, std::size_t count) {
            return self.view().subview(first, count);
        }

    private:
        audio_clip(sample_frame_format, int rate, std::size_t count);
        sample_frame_format format_{};
        int rate_ = 0;
        std::size_t count_ = 0;
        std::unique_ptr<std::byte[]> data_;
    };
} // namespace alia
#endif
