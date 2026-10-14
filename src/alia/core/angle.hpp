#ifndef ALIA_CORE_ANGLE_HPP
#define ALIA_CORE_ANGLE_HPP

#include <concepts>
#include <numbers>

namespace alia {

/// Converts degrees to radians, preserving the floating-point type.
template <std::floating_point T>
[[nodiscard]] constexpr T deg_to_rad(T degrees) {
    return degrees * (std::numbers::pi_v<T> / T{180});
}

/// Converts radians to degrees, preserving the floating-point type.
template <std::floating_point T>
[[nodiscard]] constexpr T rad_to_deg(T radians) {
    return radians * (T{180} / std::numbers::pi_v<T>);
}

} // namespace alia

#endif // ALIA_CORE_ANGLE_HPP
