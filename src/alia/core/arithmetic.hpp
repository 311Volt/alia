#ifndef ARITHMETIC_CD2274DC_DEC9_4D7D_9A97_A9988BF7C30A
#define ARITHMETIC_CD2274DC_DEC9_4D7D_9A97_A9988BF7C30A

#include <cstdint>
#include <type_traits>

namespace alia {
namespace detail {

template <typename T, size_t Size = sizeof(T), bool IsSigned = std::is_signed_v<T>>
struct double_width_helper;

// 1-byte
template <typename T> struct double_width_helper<T, 1, true>  { using type = int16_t; };
template <typename T> struct double_width_helper<T, 1, false> { using type = uint16_t; };
// 2-byte
template <typename T> struct double_width_helper<T, 2, true>  { using type = int32_t; };
template <typename T> struct double_width_helper<T, 2, false> { using type = uint32_t; };
// 4-byte
template <typename T> struct double_width_helper<T, 4, true>  { using type = int64_t; };
template <typename T> struct double_width_helper<T, 4, false> { using type = uint64_t; };
// 8-byte (stay at 64-bit)
template <typename T> struct double_width_helper<T, 8, true>  { using type = int64_t; };
template <typename T> struct double_width_helper<T, 8, false> { using type = uint64_t; };

template <typename T>
struct double_width : double_width_helper<T> {};

template <typename T, typename = void>
struct square_type_trait {};

template <typename T>
struct square_type_trait<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    using type = T;
};

template <typename T>
struct square_type_trait<T, std::enable_if_t<std::is_integral_v<T>>> {
    using type = typename double_width<T>::type;
};

} // namespace detail
} // namespace alia

#endif /* ARITHMETIC_CD2274DC_DEC9_4D7D_9A97_A9988BF7C30A */
