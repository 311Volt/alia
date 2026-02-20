#ifndef RECT_FABC5EAE_D6A2_45E9_9565_1C2ECED7CA91
#define RECT_FABC5EAE_D6A2_45E9_9565_1C2ECED7CA91

#include "vec.hpp"
#include "arithmetic.hpp"
#include <algorithm>

namespace alia {

/**
 * @brief A 2D axis-aligned rectangle defined by its top-left (p1) and bottom-right (p2) corners.
 * @tparam T The numeric type for coordinates.
 */
template <typename T>
struct rect {
    vec2<T> p1; ///< Top-left corner
    vec2<T> p2; ///< Bottom-right corner

    /// Default constructor: initializes a zero-area rectangle at (0, 0).
    constexpr rect() : p1(0, 0), p2(0, 0) {}

    /// Constructor from two corner points.
    constexpr rect(vec2<T> p1, vec2<T> p2) : p1(p1), p2(p2) {}

    /**
     * @brief Creates a rectangle from a top-left position and a size.
     * @param pos Top-left corner position.
     * @param size Dimensions of the rectangle.
     * @return A rectangle starting at pos with the given size.
     */
    static constexpr rect pos_size(vec2<T> pos, vec2<T> size) {
        return rect(pos, pos + size);
    }

    /// Returns the x-coordinate of the left edge.
    [[nodiscard]] constexpr T left() const { return p1.x; }
    /// Returns the y-coordinate of the top edge.
    [[nodiscard]] constexpr T top() const { return p1.y; }
    /// Returns the x-coordinate of the right edge.
    [[nodiscard]] constexpr T right() const { return p2.x; }
    /// Returns the y-coordinate of the bottom edge.
    [[nodiscard]] constexpr T bottom() const { return p2.y; }
    /// Returns the width of the rectangle.
    [[nodiscard]] constexpr T width() const { return p2.x - p1.x; }
    /// Returns the height of the rectangle.
    [[nodiscard]] constexpr T height() const { return p2.y - p1.y; }
    /// Returns the dimensions of the rectangle as a vector.
    [[nodiscard]] constexpr vec2<T> size() const { return p2 - p1; }

    /// Returns the top-left corner point.
    [[nodiscard]] constexpr vec2<T> top_left() const { return p1; }
    /// Shorthand for top_left().
    [[nodiscard]] constexpr vec2<T> tl() const { return p1; }
    /// Returns the top-right corner point.
    [[nodiscard]] constexpr vec2<T> top_right() const { return vec2<T>(p2.x, p1.y); }
    /// Shorthand for top_right().
    [[nodiscard]] constexpr vec2<T> tr() const { return top_right(); }
    /// Returns the bottom-left corner point.
    [[nodiscard]] constexpr vec2<T> bottom_left() const { return vec2<T>(p1.x, p2.y); }
    /// Shorthand for bottom_left().
    [[nodiscard]] constexpr vec2<T> bl() const { return bottom_left(); }
    /// Returns the bottom-right corner point.
    [[nodiscard]] constexpr vec2<T> bottom_right() const { return p2; }
    /// Shorthand for bottom_right().
    [[nodiscard]] constexpr vec2<T> br() const { return p2; }

    /**
     * @brief Converts the rectangle to a different coordinate type.
     * @tparam U The target numeric type.
     * @return A rectangle with coordinates cast to U.
     */
    template <typename U>
    [[nodiscard]] constexpr rect<U> cast() const {
        return rect<U>(p1.template cast<U>(), p2.template cast<U>());
    }

    /// Convenience helpers for casting.
    [[nodiscard]] constexpr rect<float> f32() const { return cast<float>(); }
    [[nodiscard]] constexpr rect<double> f64() const { return cast<double>(); }
    [[nodiscard]] constexpr rect<int32_t> i32() const { return cast<int32_t>(); }
    [[nodiscard]] constexpr rect<uint32_t> u32() const { return cast<uint32_t>(); }
    [[nodiscard]] constexpr rect<int64_t> i64() const { return cast<int64_t>(); }
    [[nodiscard]] constexpr rect<uint64_t> u64() const { return cast<uint64_t>(); }

    /**
     * @brief Calculates the center of the rectangle.
     * Available only for floating-point coordinate types.
     */
    template <typename U = T, typename std::enable_if_t<std::is_floating_point_v<U>, int> = 0>
    [[nodiscard]] constexpr vec2<T> center() const {
        return (p1 + p2) * T(0.5);
    }

    /**
     * @brief Calculates the area of the rectangle.
     * Returns a wider type for integers to prevent overflow.
     */
    template <typename U = T, typename AreaT = typename detail::square_type_trait<U>::type>
    [[nodiscard]] constexpr AreaT area() const {
        return static_cast<AreaT>(width()) * static_cast<AreaT>(height());
    }

    /**
     * @brief Result of a point-rectangle test.
     */
    struct point_test_result {
        bool x_low;      ///< Point is to the left of p1.x
        bool x_in_range; ///< Point is between p1.x (inclusive) and p2.x (exclusive)
        bool x_high;     ///< Point is to the right of p2.x (inclusive)
        bool y_low;      ///< Point is above p1.y
        bool y_in_range; ///< Point is between p1.y (inclusive) and p2.y (exclusive)
        bool y_high;     ///< Point is below p2.y (inclusive)
    };

    /**
     * @brief Tests the position of a point relative to the rectangle's bounds.
     * @param point The point to test.
     * @return A bitmask-like struct indicating the point's relation to each axis.
     */
    [[nodiscard]] constexpr point_test_result test_point(vec2<T> point) const {
        return {
            point.x < p1.x,
            point.x >= p1.x && point.x < p2.x,
            point.x >= p2.x,
            point.y < p1.y,
            point.y >= p1.y && point.y < p2.y,
            point.y >= p2.y
        };
    }

    /**
     * @brief Checks if a point is inside the rectangle.
     * Includes left/top edges, excludes right/bottom edges.
     */
    [[nodiscard]] constexpr bool contains(vec2<T> point) const {
        return point.x >= p1.x && point.x < p2.x &&
               point.y >= p1.y && point.y < p2.y;
    }

    /**
     * @brief Checks if this rectangle intersects another.
     */
    [[nodiscard]] constexpr bool intersects(const rect& other) const {
        return p1.x < other.p2.x && p2.x > other.p1.x &&
               p1.y < other.p2.y && p2.y > other.p1.y;
    }

    /**
     * @brief Checks if this rectangle completely contains another.
     */
    [[nodiscard]] constexpr bool contains(const rect& other) const {
        return other.p1.x >= p1.x && other.p2.x <= p2.x &&
               other.p1.y >= p1.y && other.p2.y <= p2.y;
    }

    /**
     * @brief Returns the smallest rectangle that contains both this and another rectangle.
     */
    [[nodiscard]] constexpr rect union_with(const rect& other) const {
        return rect(
            vec2<T>((std::min)(p1.x, other.p1.x), (std::min)(p1.y, other.p1.y)),
            vec2<T>((std::max)(p2.x, other.p2.x), (std::max)(p2.y, other.p2.y)));
    }

    /**
     * @brief Returns the intersection of this rectangle and another.
     * Note: If they don't intersect, the result might have negative size.
     */
    [[nodiscard]] constexpr rect intersection_with(const rect& other) const {
        return rect(
            vec2<T>((std::max)(p1.x, other.p1.x), (std::max)(p1.y, other.p1.y)),
            vec2<T>((std::min)(p2.x, other.p2.x), (std::min)(p2.y, other.p2.y)));
    }

    /**
     * @brief Translates the rectangle by a given offset.
     * @param offset The vector to add to the rectangle's position.
     */
    constexpr void translate_inplace(vec2<T> offset) {
        p1 += offset;
        p2 += offset;
    }

    /**
     * @brief Returns a copy of the rectangle translated by an offset.
     */
    [[nodiscard]] constexpr rect translated(vec2<T> offset) const {
        rect r = *this;
        r.translate_inplace(offset);
        return r;
    }

    /**
     * @brief Scales the rectangle relative to an anchor point.
     * @param factor Scale factors for x and y axes.
     * @param anchor The point that remains stationary during scaling.
     */
    constexpr void scale_inplace(vec2<T> factor, vec2<T> anchor) {
        p1 = anchor + (p1 - anchor) * factor;
        p2 = anchor + (p2 - anchor) * factor;
    }

    /**
     * @brief Scales the rectangle relative to its top-left corner (p1).
     * @param factor Scale factors for x and y axes.
     */
    constexpr void scale_inplace(vec2<T> factor) {
        scale_inplace(factor, p1);
    }

    /**
     * @brief Returns a copy of the rectangle scaled relative to an anchor point.
     */
    [[nodiscard]] constexpr rect scaled(vec2<T> factor, vec2<T> anchor) const {
        rect r = *this;
        r.scale_inplace(factor, anchor);
        return r;
    }

    /**
     * @brief Returns a copy of the rectangle scaled relative to its top-left corner.
     */
    [[nodiscard]] constexpr rect scaled(vec2<T> factor) const {
        return scaled(factor, p1);
    }

    constexpr bool operator==(const rect& other) const = default;
};

using rect_i = rect<int>;
using rect_f = rect<float>;
using rect_d = rect<double>;

} // namespace alia

#endif /* RECT_FABC5EAE_D6A2_45E9_9565_1C2ECED7CA91 */
