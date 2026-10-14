#include "alia/core/angle.hpp"
#include "alia/gfx/transform.hpp"

#include <gtest/gtest.h>

#include <initializer_list>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <type_traits>

namespace {

using alia::transform;
using alia::vec2f;
using alia::vec3f;
using alia::vec4f;

constexpr float tolerance = 1e-5f;
constexpr float quarter_turn = std::numbers::pi_v<float> * 0.5f;

void expect_matrix_near(const transform& actual, const transform& expected) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_NEAR(actual.m[i][j], expected.m[i][j], tolerance)
                << "row " << i << ", column " << j;
        }
    }
}

void expect_point_near(vec2f actual, vec2f expected) {
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
}

void expect_point_near(vec3f actual, vec3f expected) {
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

TEST(Transform, RotatesUnitAxesInRightHandedDirections) {
    const vec3f x{1.0f, 0.0f, 0.0f};
    const vec3f y{0.0f, 1.0f, 0.0f};
    const vec3f z{0.0f, 0.0f, 1.0f};

    const auto rx = transform::rotate_x(quarter_turn);
    expect_point_near(rx.apply(x), x);
    expect_point_near(rx.apply(y), z);
    expect_point_near(rx.apply(z), vec3f{0.0f, -1.0f, 0.0f});

    const auto ry = transform::rotate_y(quarter_turn);
    expect_point_near(ry.apply(y), y);
    expect_point_near(ry.apply(x), vec3f{0.0f, 0.0f, -1.0f});
    expect_point_near(ry.apply(z), x);

    const auto rz = transform::rotate_z(quarter_turn);
    expect_point_near(rz.apply(z), z);
    expect_point_near(rz.apply(x), y);
    expect_point_near(rz.apply(y), vec3f{-1.0f, 0.0f, 0.0f});
    expect_matrix_near(rz, transform::rotate(quarter_turn));
}

TEST(Transform, AxisAngleMatchesCardinalRotationsAndNormalizesAxis) {
    for (float angle : {0.0f, 0.37f, -0.83f, quarter_turn}) {
        expect_matrix_near(
            transform::rotate(vec3f{3.0f, 0.0f, 0.0f}, angle),
            transform::rotate_x(angle));
        expect_matrix_near(
            transform::rotate(vec3f{0.0f, 2.0f, 0.0f}, angle),
            transform::rotate_y(angle));
        expect_matrix_near(
            transform::rotate(vec3f{0.0f, 0.0f, 4.0f}, angle),
            transform::rotate_z(angle));
    }
}

TEST(Transform, AxisAngleRotatesAroundAnObliqueAxis) {
    const auto rotation = transform::rotate(
        vec3f{1.0f, 1.0f, 1.0f}, alia::deg_to_rad(120.0f));

    expect_point_near(rotation.apply(vec3f{1.0f, 0.0f, 0.0f}), vec3f{0.0f, 1.0f, 0.0f});
    expect_point_near(rotation.apply(vec3f{0.0f, 1.0f, 0.0f}), vec3f{0.0f, 0.0f, 1.0f});
    expect_point_near(rotation.apply(vec3f{0.0f, 0.0f, 1.0f}), vec3f{1.0f, 0.0f, 0.0f});
    expect_point_near(rotation.apply(vec3f{1.0f, 1.0f, 1.0f}), vec3f{1.0f, 1.0f, 1.0f});
}

TEST(Transform, AxisNormalizationHandlesLargeAndSmallFiniteAxes) {
    const float large = std::numeric_limits<float>::max();
    const float small = std::numeric_limits<float>::min();
    expect_matrix_near(
        transform::rotate(vec3f{large, large, 0.0f}, 0.7f),
        transform::rotate(vec3f{1.0f, 1.0f, 0.0f}, 0.7f));
    expect_matrix_near(
        transform::rotate(vec3f{small, small, 0.0f}, 0.7f),
        transform::rotate(vec3f{1.0f, 1.0f, 0.0f}, 0.7f));
}

#ifndef NDEBUG
TEST(Transform, AxisAngleRejectsZeroAndNonFiniteAxes) {
    EXPECT_THROW((void)transform::rotate(vec3f{}, 0.5f), std::invalid_argument);
    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW((void)transform::rotate(vec3f{inf, 1.0f, 0.0f}, 0.5f), std::invalid_argument);
    EXPECT_THROW((void)transform::rotate(vec3f{0.0f, nan, 1.0f}, 0.5f), std::invalid_argument);
    EXPECT_THROW((void)transform::rotate(vec3f{1.0f, 0.0f, inf}, 0.5f), std::invalid_argument);
}
#endif

TEST(Transform, BracedArgumentsSelectTwoOrThreeDimensionalFactories) {
    const vec3f point{1.0f, 2.0f, 3.0f};
    expect_point_near(transform::translate({4.0f, 5.0f}).apply(point), vec3f{5.0f, 7.0f, 3.0f});
    expect_point_near(transform::translate({4.0f, 5.0f, 6.0f}).apply(point), vec3f{5.0f, 7.0f, 9.0f});
    expect_point_near(transform::scale({4.0f, 5.0f}).apply(point), vec3f{4.0f, 10.0f, 3.0f});
    expect_point_near(transform::scale({4.0f, 5.0f, 6.0f}).apply(point), vec3f{4.0f, 10.0f, 18.0f});
    expect_point_near(transform::scale(2.0f).apply(point), vec3f{2.0f, 4.0f, 3.0f});
}

TEST(Transform, BuildScalesThenRotatesThenTranslates) {
    const vec2f scaling{2.0f, 3.0f};
    const vec2f translation{5.0f, 7.0f};
    const auto built = transform::build(quarter_turn, scaling, translation);

    expect_matrix_near(built,
        transform::scale(scaling) * transform::rotate(quarter_turn) * transform::translate(translation));
    expect_point_near(built.apply(vec2f{1.0f, 2.0f}), vec2f{-1.0f, 9.0f});
}

TEST(Transform, CompositionAppliesLeftThenRight) {
    const auto translate = transform::translate(vec3f{1.0f, 2.0f, 3.0f});
    const auto scale = transform::scale(vec3f{2.0f, 3.0f, 4.0f});
    const vec3f point{4.0f, 5.0f, 6.0f};

    expect_point_near((translate * scale).apply(point), vec3f{10.0f, 21.0f, 36.0f});
    expect_point_near((scale * translate).apply(point), vec3f{9.0f, 17.0f, 27.0f});
}

TEST(Transform, ThenMembersMatchTwoDimensionalComposition) {
    auto chained = transform::translate(vec2f{1.0f, 2.0f});
    auto& result = chained
        .then_scale(2.0f)
        .then_scale({3.0f, 4.0f})
        .then_rotate(0.2f)
        .then_translate({5.0f, -6.0f})
        .then_horizontal_shear(0.1f)
        .then_vertical_shear(-0.3f)
        .then(transform::rotate(-0.4f));
    const auto composed = transform::translate(vec2f{1.0f, 2.0f}) *
        transform::scale(2.0f) * transform::scale(vec2f{3.0f, 4.0f}) *
        transform::rotate(0.2f) * transform::translate(vec2f{5.0f, -6.0f}) *
        transform::horizontal_shear(0.1f) * transform::vertical_shear(-0.3f) *
        transform::rotate(-0.4f);

    EXPECT_EQ(&result, &chained);
    expect_matrix_near(chained, composed);
}

TEST(Transform, ThenMembersMatchThreeDimensionalComposition) {
    const vec3f axis{1.0f, 2.0f, 3.0f};
    auto chained = transform::identity();
    auto& result = chained
        .then_scale({2.0f, 3.0f, 4.0f})
        .then_rotate_x(0.1f)
        .then_rotate_y(-0.2f)
        .then_rotate_z(0.3f)
        .then_rotate(axis, -0.4f)
        .then_translate({5.0f, 6.0f, 7.0f});
    const auto composed = transform::scale(vec3f{2.0f, 3.0f, 4.0f}) *
        transform::rotate_x(0.1f) * transform::rotate_y(-0.2f) *
        transform::rotate_z(0.3f) * transform::rotate(axis, -0.4f) *
        transform::translate(vec3f{5.0f, 6.0f, 7.0f});

    EXPECT_EQ(&result, &chained);
    expect_matrix_near(chained, composed);
}

TEST(Transform, InvertsAffineTransformsAndRestoresPoints) {
    const auto matrix = transform::scale(vec3f{2.0f, -3.0f, 4.0f}) *
        transform::rotate(vec3f{1.0f, 2.0f, 3.0f}, 0.7f) *
        transform::translate(vec3f{5.0f, 6.0f, 7.0f});
    const auto inverse = matrix.inverted();

    EXPECT_TRUE(matrix.is_invertible());
    EXPECT_NEAR(matrix.determinant(), -24.0f, 1e-4f);
    expect_matrix_near(inverse * matrix, transform::identity());
    expect_matrix_near(matrix * inverse, transform::identity());
    const vec3f point{1.0f, 2.0f, 3.0f};
    expect_point_near(inverse.apply(matrix.apply(point)), point);

    auto in_place = matrix;
    EXPECT_EQ(&in_place.invert(), &in_place);
    expect_matrix_near(in_place, inverse);
}

TEST(Transform, InvertsGeneralFourByFourMatrices) {
    const transform matrix{{
        {1.0f, 2.0f, 3.0f, 4.0f},
        {2.0f, 3.0f, 4.0f, 5.0f},
        {3.0f, 4.0f, 6.0f, 7.0f},
        {4.0f, 5.0f, 7.0f, 9.0f},
    }};
    const auto inverse = matrix.inverted();

    EXPECT_FLOAT_EQ(matrix.determinant(), -1.0f);
    expect_matrix_near(inverse * matrix, transform::identity());
    expect_matrix_near(matrix * inverse, transform::identity());
}

TEST(Transform, SingularInverseThrowsAndLeavesOriginalUnchanged) {
    transform singular{{
        {1.0f, 2.0f, 3.0f, 4.0f},
        {1.0f, 2.0f, 3.0f, 4.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    }};
    const auto original = singular;

    EXPECT_FLOAT_EQ(singular.determinant(), 0.0f);
    EXPECT_FALSE(singular.is_invertible());
    EXPECT_FALSE(singular.is_invertible(0.0f));
    EXPECT_THROW((void)singular.inverted(), std::invalid_argument);
    EXPECT_THROW(singular.invert(), std::invalid_argument);
    expect_matrix_near(singular, original);
}

TEST(Transform, InvertibilityToleranceDoesNotRejectNonzeroInverse) {
    const auto matrix = transform::scale(vec3f{1e-8f, 1.0f, 1.0f});

    EXPECT_FALSE(matrix.is_invertible());
    EXPECT_FALSE(matrix.is_invertible(1e-8f));
    EXPECT_TRUE(matrix.is_invertible(1e-9f));
    EXPECT_TRUE(matrix.is_invertible(0.0f));
    expect_matrix_near(matrix.inverted() * matrix, transform::identity());
}

TEST(Transform, NonFiniteDeterminantsAreNotInvertible) {
    auto matrix = transform::identity();
    matrix.m[0][0] = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(matrix.is_invertible());
    matrix.m[0][0] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(matrix.is_invertible());
}

TEST(Transform, TransposeExchangesRowsAndColumnsAndIsItsOwnInverse) {
    const transform matrix{{
        {1.0f, 2.0f, 3.0f, 4.0f},
        {5.0f, 6.0f, 7.0f, 8.0f},
        {9.0f, 10.0f, 11.0f, 12.0f},
        {13.0f, 14.0f, 15.0f, 16.0f},
    }};
    const auto transposed = matrix.transposed();
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            EXPECT_FLOAT_EQ(transposed.m[i][j], matrix.m[j][i]);
    expect_matrix_near(transposed.transposed(), matrix);

    auto in_place = matrix;
    EXPECT_EQ(&in_place.transpose(), &in_place);
    expect_matrix_near(in_place, transposed);
}

TEST(Transform, AppliesAffineHomogeneousAndProjectivePoints) {
    const transform matrix{{
        {2.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 3.0f, 1.0f, 2.0f},
        {1.0f, 0.0f, 4.0f, 3.0f},
        {5.0f, 6.0f, 7.0f, 2.0f},
    }};

    expect_point_near(matrix.apply(vec2f{1.0f, 2.0f}), vec2f{7.0f, 13.0f});
    expect_point_near(matrix.apply(vec3f{1.0f, 2.0f, 3.0f}), vec3f{10.0f, 13.0f, 21.0f});
    EXPECT_EQ(matrix.apply(vec4f{1.0f, 2.0f, 3.0f, 2.0f}), (vec4f{15.0f, 19.0f, 28.0f, 18.0f}));
    EXPECT_EQ(matrix.apply(vec4f{1.0f, 2.0f, 3.0f, 0.0f}), (vec4f{5.0f, 7.0f, 14.0f, 14.0f}));
    expect_point_near(matrix.apply_projective(vec3f{1.0f, 2.0f, 3.0f}),
        vec3f{10.0f / 16.0f, 13.0f / 16.0f, 21.0f / 16.0f});

    EXPECT_EQ(matrix(vec2f{1.0f, 2.0f}), matrix.apply(vec2f{1.0f, 2.0f}));
    EXPECT_EQ(matrix(vec3f{1.0f, 2.0f, 3.0f}), matrix.apply(vec3f{1.0f, 2.0f, 3.0f}));
    EXPECT_EQ(matrix(vec4f{1.0f, 2.0f, 3.0f, 2.0f}), matrix.apply(vec4f{1.0f, 2.0f, 3.0f, 2.0f}));
}

TEST(Transform, ShearsByTangentOfAngle) {
    const float angle = std::numbers::pi_v<float> * 0.25f;
    const vec3f point{2.0f, 3.0f, 4.0f};

    expect_point_near(transform::horizontal_shear(angle).apply(point), vec3f{5.0f, 3.0f, 4.0f});
    expect_point_near(transform::vertical_shear(angle).apply(point), vec3f{2.0f, 5.0f, 4.0f});
    expect_point_near(transform::horizontal_shear(-angle).apply(point), vec3f{-1.0f, 3.0f, 4.0f});
    expect_point_near(transform::vertical_shear(-angle).apply(point), vec3f{2.0f, 1.0f, 4.0f});
}

TEST(Angle, ConversionsAreConstexprAndPreserveType) {
    static_assert(std::is_same_v<decltype(alia::deg_to_rad(1.0f)), float>);
    static_assert(std::is_same_v<decltype(alia::deg_to_rad(1.0)), double>);
    static_assert(std::is_same_v<decltype(alia::deg_to_rad(1.0L)), long double>);
    static_assert(std::is_same_v<decltype(alia::rad_to_deg(1.0f)), float>);
    static_assert(std::is_same_v<decltype(alia::rad_to_deg(1.0)), double>);
    static_assert(std::is_same_v<decltype(alia::rad_to_deg(1.0L)), long double>);
    constexpr double radians = alia::deg_to_rad(180.0);
    constexpr double degrees = alia::rad_to_deg(std::numbers::pi_v<double>);
    EXPECT_DOUBLE_EQ(radians, std::numbers::pi_v<double>);
    EXPECT_DOUBLE_EQ(degrees, 180.0);
}

TEST(Angle, ConvertsKnownAnglesAndRoundTrips) {
    EXPECT_NEAR(alia::deg_to_rad(90.0f), quarter_turn, tolerance);
    EXPECT_NEAR(alia::rad_to_deg(quarter_turn), 90.0f, tolerance);
    for (float degrees : {0.0f, 30.0f, -45.0f, 90.0f, 180.0f, 360.0f})
        EXPECT_NEAR(alia::rad_to_deg(alia::deg_to_rad(degrees)), degrees, 1e-4f);
    for (double degrees : {0.0, 30.0, -45.0, 90.0, 180.0, 360.0})
        EXPECT_NEAR(alia::rad_to_deg(alia::deg_to_rad(degrees)), degrees, 1e-12);
}

} // namespace
