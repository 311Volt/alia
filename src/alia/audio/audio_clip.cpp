#include "sample_conversions.hpp"

namespace alia {
    audio_clip::audio_clip(sample_frame_format format, int rate, std::size_t count)
        : format_(format), rate_(rate), count_(count) {
        if (rate <= 0) throw std::invalid_argument("audio sample rate must be positive");
        const auto size = detail::audio_byte_count(format, count);
        if (size) data_ = std::make_unique<std::byte[]>(size);
    }
    audio_clip::audio_clip(sample_frame_format format, int rate, std::span<const std::byte> bytes)
        : audio_clip(format, rate, bytes.size() / format.bytes_per_frame()) {
        if (bytes.size() % format.bytes_per_frame()) throw std::invalid_argument("incomplete audio frame");
        if (!bytes.empty()) std::memcpy(data_.get(), bytes.data(), bytes.size());
    }
    audio_clip::audio_clip(const_audio_clip_view view) {
        if (!view.sample_rate() && !view.frame_count()) { format_ = view.format(); return; }
        *this = audio_clip(view.format(), view.sample_rate(), view.bytes());
    }
    audio_clip::audio_clip(audio_clip &&other) noexcept
        : format_(other.format_), rate_(std::exchange(other.rate_, 0)), count_(std::exchange(other.count_, 0)), data_(std::move(other.data_)) {}
    audio_clip &audio_clip::operator=(audio_clip &&other) noexcept {
        if (this != &other) {
            format_ = other.format_;
            rate_ = std::exchange(other.rate_, 0);
            count_ = std::exchange(other.count_, 0);
            data_ = std::move(other.data_);
        }
        return *this;
    }
    audio_clip audio_clip::clone() const { return audio_clip(view()); }

    namespace detail {
        template <class D, class S>
        void convert_bytes(std::byte *dst, const std::byte *src, std::size_t count, int dc, int sc) noexcept {
            for (std::size_t f = 0; f < count; ++f) {
                double sum = 0;
                if (dc == 1 && sc != 1) {
                    for (int c = 0; c < sc; ++c) {
                        S point;
                        std::memcpy(&point, src + (f * sc + c) * sizeof(S), sizeof(S));
                        sum += convert_sample_point<double>(point);
                    }
                }
                for (int c = 0; c < dc; ++c) {
                    D point;
                    if (dc == 1 && sc != 1) point = convert_sample_point<D>(sum / sc);
                    else {
                        S in;
                        std::memcpy(&in, src + (f * sc + (sc == 1 ? 0 : c)) * sizeof(S), sizeof(S));
                        point = convert_sample_point<D>(in);
                    }
                    std::memcpy(dst + (f * dc + c) * sizeof(D), &point, sizeof(D));
                }
            }
        }
        template <class F> decltype(auto) dispatch_point(sample_point_format format, F &&fn) {
            using namespace sample_point_formats;
            if (format == u8) return fn.template operator()<std::uint8_t>();
            if (format == s8) return fn.template operator()<std::int8_t>();
            if (format == u16) return fn.template operator()<std::uint16_t>();
            if (format == s16) return fn.template operator()<std::int16_t>();
            if (format == u32) return fn.template operator()<std::uint32_t>();
            if (format == s32) return fn.template operator()<std::int32_t>();
            if (format == f32) return fn.template operator()<float>();
            if (format == f64) return fn.template operator()<double>();
            throw std::invalid_argument("unsupported erased audio point format");
        }
        void validate_erased_audio_format(sample_frame_format format) {
            (void)format.bytes_per_frame();
            dispatch_point(format.point, []<class P> {});
        }
        audio_conversion_fn select_audio_conversion(sample_frame_format dst, sample_frame_format src) {
            validate_erased_audio_format(dst);
            validate_erased_audio_format(src);
            if (dst.channels != src.channels && dst.channels != 1 && src.channels != 1)
                throw std::invalid_argument("unsupported audio channel mapping");
            return dispatch_point(dst.point, [&]<class D> {
                return dispatch_point(src.point, []<class S>() -> audio_conversion_fn { return &convert_bytes<D, S>; });
            });
        }
        void silence_audio_bytes(std::byte *dst, std::size_t count, sample_frame_format format) noexcept {
            const auto points = count * format.channels;
            if (!points) return;
            std::memset(dst, 0, points * format.point.bytes);
            if (format.point.encoding == sample_encoding::unsigned_int) {
                const auto high = std::endian::native == std::endian::little ? format.point.bytes - 1 : 0;
                for (std::size_t i = 0; i < points; ++i) dst[i * format.point.bytes + high] = std::byte{0x80};
            }
        }
    }
    void fill_silence(any_audio_clip_view view) {
        detail::validate_erased_audio_format(view.format());
        detail::silence_audio_bytes(view.bytes().data(), view.frame_count(), view.format());
    }
    void converting_copy(any_audio_clip_view dst, const_audio_clip_view src) {
        if (dst.frame_count() != src.frame_count()) throw std::invalid_argument("audio frame counts do not match");
        auto convert = detail::select_audio_conversion(dst.format(), src.format());
        convert(dst.bytes().data(), src.bytes().data(), dst.frame_count(), dst.format().channels, src.format().channels);
    }
}
