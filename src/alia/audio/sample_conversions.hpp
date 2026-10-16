#ifndef ALIA_AUDIO_SAMPLE_CONVERSIONS_HPP
#define ALIA_AUDIO_SAMPLE_CONVERSIONS_HPP

#include "audio_clip.hpp"
#include <cmath>
#include <cstring>
#include <functional>

namespace alia {
    /// Normalized PCM conversion, without dither. NaN becomes silence for integer output.
    template <sample_point D, sample_point S>
    D convert_sample_point(S value) noexcept {
        if constexpr (std::same_as<D, S>)
            return value;
        else if constexpr (std::integral<D> && std::integral<S>) {
            using SU = std::make_unsigned_t<S>;
            using DU = std::make_unsigned_t<D>;
            SU bits = static_cast<SU>(value);
            if constexpr (std::is_signed_v<S>)
                bits ^= SU(1) << (sizeof(S) * 8 - 1);
            DU result;
            if constexpr (sizeof(D) > sizeof(S))
                result = static_cast<DU>(bits) << ((sizeof(D) - sizeof(S)) * 8);
            else
                result = static_cast<DU>(bits >> ((sizeof(S) - sizeof(D)) * 8));
            if constexpr (std::is_signed_v<D>)
                result ^= DU(1) << (sizeof(D) * 8 - 1);
            return std::bit_cast<D>(result);
        } else if constexpr (std::floating_point<D> && std::integral<S>) {
            const long double scale = std::ldexp(1.0L, sizeof(S) * 8 - 1);
            long double normalized;
            if constexpr (std::unsigned_integral<S>) {
                // Recenter before conversion, preserving tiny offsets from the
                // midpoint even when long double has only double precision.
                const S midpoint = S(1) << (sizeof(S) * 8 - 1);
                normalized = value >= midpoint ? static_cast<long double>(value - midpoint) / scale
                                               : -static_cast<long double>(midpoint - value) / scale;
            } else
                normalized = static_cast<long double>(value) / scale;
            return static_cast<D>(normalized);
        } else if constexpr (std::integral<D>) {
            if (std::isnan(value))
                return silence_of<D>();
            if (value <= S(-1))
                return (std::numeric_limits<D>::min)();
            if (value >= S(1))
                return (std::numeric_limits<D>::max)();
            const long double scale = std::ldexp(1.0L, sizeof(D) * 8 - 1);
            const long double scaled = std::round(static_cast<long double>(value) * scale);
            if constexpr (std::unsigned_integral<D>) {
                if (scaled >= scale)
                    return (std::numeric_limits<D>::max)();
                if (scaled <= -scale)
                    return 0;
                const D midpoint = D(1) << (sizeof(D) * 8 - 1);
                return scaled >= 0 ? midpoint + static_cast<D>(scaled) : midpoint - static_cast<D>(-scaled);
            } else {
                if (scaled <= static_cast<long double>((std::numeric_limits<D>::min)()))
                    return (std::numeric_limits<D>::min)();
                if (scaled >= static_cast<long double>((std::numeric_limits<D>::max)()))
                    return (std::numeric_limits<D>::max)();
                return static_cast<D>(scaled);
            }
        } else
            return static_cast<D>(value);
    }

    template <sample_frame F>
        requires(!std::is_const_v<F>)
    void fill_silence(audio_clip_view<F> view) noexcept {
        auto frames = view.frames();
        std::fill(frames.begin(), frames.end(), detail::silent_frame<F>());
    }

    void fill_silence(any_audio_clip_view);

    template <sample_frame D, sample_frame S, class Converter>
        requires(!std::is_const_v<D>)
    void converting_copy(audio_clip_view<D> dst, audio_clip_view<S> src, Converter &&convert) {
        if (dst.frame_count() != src.frame_count())
            throw std::invalid_argument("audio frame counts do not match");
        for (std::size_t i = 0; i < dst.frame_count(); ++i)
            dst[i] = std::invoke(convert, src[i]);
    }

    template <sample_frame D, sample_frame S>
        requires(!std::is_const_v<D>)
    void converting_copy(audio_clip_view<D> dst, audio_clip_view<S> src) {
        constexpr auto dc = channel_count_v<D>, sc = channel_count_v<S>;
        if constexpr (dc != sc && dc != 1 && sc != 1)
            throw std::invalid_argument("unsupported audio channel mapping");
        else
            converting_copy(dst, src, [](const auto &frame) {
                const auto in = std::bit_cast<std::array<sample_point_t<S>, sc>>(frame);
                std::array<sample_point_t<D>, dc> out;
                if constexpr (dc == 1 && sc != 1) {
                    double sum = 0;
                    for (auto point : in)
                        sum += convert_sample_point<double>(point);
                    out[0] = convert_sample_point<sample_point_t<D>>(sum / sc);
                } else {
                    for (std::size_t c = 0; c < dc; ++c)
                        out[c] = convert_sample_point<sample_point_t<D>>(in[sc == 1 ? 0 : c]);
                }
                return std::bit_cast<D>(out);
            });
    }

    /// Validates once, then dispatches supported runtime point formats. Rates
    /// are metadata: this operation never resamples. Buffers must not overlap.
    void converting_copy(any_audio_clip_view dst, const_audio_clip_view src);

    namespace detail {
        using audio_conversion_fn = void (*)(std::byte *, const std::byte *, std::size_t, int, int) noexcept;
        audio_conversion_fn select_audio_conversion(sample_frame_format dst, sample_frame_format src);
        /// Validates runtime encoding support and writes offset-binary silence.
        void validate_erased_audio_format(sample_frame_format);
        void silence_audio_bytes(std::byte *, std::size_t frame_count, sample_frame_format) noexcept;
    } // namespace detail
} // namespace alia
#endif
