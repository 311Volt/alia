#ifndef ALIA_AUDIO_SAMPLE_HPP
#define ALIA_AUDIO_SAMPLE_HPP

#include "../core/vec.hpp"
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace alia {
    template <class T>
    concept sample_point = (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>) || std::floating_point<T>;

    /// Opt-in promise: tuple elements are contiguous, ordered channel points,
    /// with no padding. Frames use native endianness and interleaved channels.
    template <class T>
    inline constexpr bool enable_contiguous_sample_frame = false;
    template <class T>
    inline constexpr bool enable_contiguous_sample_frame<vec2<T>> = true;
    template <class T>
    inline constexpr bool enable_contiguous_sample_frame<vec3<T>> = true;
    template <class T>
    inline constexpr bool enable_contiguous_sample_frame<vec4<T>> = true;
    template <class T, std::size_t N>
    inline constexpr bool enable_contiguous_sample_frame<std::array<T, N>> = true;

    namespace detail {
        template <class T, std::size_t... I>
        consteval bool homogeneous_sample_points(std::index_sequence<I...>) {
            if constexpr ((requires { typename std::tuple_element<I, T>::type; } && ...))
                return sample_point<std::tuple_element_t<0, T>> &&
                       (std::same_as<std::tuple_element_t<0, T>, std::tuple_element_t<I, T>> && ...);
            else
                return false;
        }

        template <class T>
        consteval bool is_tuple_sample_frame() {
            if constexpr (requires { std::tuple_size<T>::value; }) {
                constexpr auto n = std::tuple_size_v<T>;
                if constexpr (n > 0) {
                    if constexpr (homogeneous_sample_points<T>(std::make_index_sequence<n>{}))
                        return sizeof(T) == sizeof(std::tuple_element_t<0, T>) * n && std::is_trivially_copyable_v<T> &&
                               std::is_standard_layout_v<T> && enable_contiguous_sample_frame<T>;
                }
            }
            return false;
        }

        template <class F, bool = sample_point<F>>
        struct sample_frame_traits {
            using point = std::tuple_element_t<0, F>;
            static constexpr std::size_t channels = std::tuple_size_v<F>;
        };

        template <class F>
        struct sample_frame_traits<F, true> {
            using point = F;
            static constexpr std::size_t channels = 1;
        };
    } // namespace detail

    template <class T>
    concept sample_frame = sample_point<T> || detail::is_tuple_sample_frame<std::remove_cv_t<T>>();
    template <sample_frame F>
    using sample_point_t = typename detail::sample_frame_traits<std::remove_cv_t<F>>::point;
    template <sample_frame F>
    inline constexpr std::size_t channel_count_v = detail::sample_frame_traits<std::remove_cv_t<F>>::channels;

    enum class sample_encoding : std::uint8_t {
        unsigned_int,
        signed_int,
        floating_point
    };

    struct sample_point_format {
        sample_encoding encoding = sample_encoding::floating_point;
        std::uint8_t bytes = 4;
        bool operator==(const sample_point_format &) const = default;
    };

    namespace sample_point_formats {
        inline constexpr sample_point_format u8{sample_encoding::unsigned_int, 1}, s8{sample_encoding::signed_int, 1};
        inline constexpr sample_point_format u16{sample_encoding::unsigned_int, 2}, s16{sample_encoding::signed_int, 2};
        inline constexpr sample_point_format u32{sample_encoding::unsigned_int, 4}, s32{sample_encoding::signed_int, 4};
        inline constexpr sample_point_format f32{sample_encoding::floating_point, 4},
            f64{sample_encoding::floating_point, 8};
    } // namespace sample_point_formats

    template <sample_point T>
    inline constexpr sample_point_format sample_point_format_of_v{
        std::floating_point<T> ? sample_encoding::floating_point
        : std::is_signed_v<T>  ? sample_encoding::signed_int
                               : sample_encoding::unsigned_int,
        static_cast<std::uint8_t>(sizeof(T))
    };

    /// Channel order follows WAVEFORMATEXTENSIBLE/SMPTE (FL FR FC LFE BL BR SL SR).
    struct sample_frame_format {
        sample_point_format point{};
        int channels = 1;

        [[nodiscard]] constexpr int bytes_per_frame() const {
            if (point.bytes == 0 || channels <= 0 || channels > (std::numeric_limits<int>::max)() / point.bytes)
                throw std::invalid_argument("invalid audio frame format");
            return channels * point.bytes;
        }

        bool operator==(const sample_frame_format &) const = default;
    };

    template <sample_frame F>
    inline constexpr sample_frame_format sample_frame_format_of_v{
        sample_point_format_of_v<sample_point_t<F>>, static_cast<int>(channel_count_v<F>)
    };

    struct audio_stream_format {
        sample_frame_format frame{};
        int sample_rate = 0;
        bool operator==(const audio_stream_format &) const = default;
    };
} // namespace alia
#endif
