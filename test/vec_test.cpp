#include "alia/core/vec.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <concepts>
#include <type_traits>
#include <utility>

static_assert(std::tuple_size_v<alia::vec2f> == 2);
static_assert(std::tuple_size_v<alia::vec3f> == 3);
static_assert(std::tuple_size_v<alia::vec4f> == 4);
static_assert(std::same_as<std::tuple_element_t<1, alia::vec2f>, float>);
static_assert(std::same_as<std::tuple_element_t<2, const alia::vec3i>, const int>);
static_assert(alia::vec4i{1, 2, 3, 4}.get<3>() == 4);
static_assert(std::same_as<decltype(std::declval<const alia::vec2f &>().get<0>()), const float &>);
static_assert(std::same_as<decltype(std::declval<alia::vec3f &&>().get<2>()), float &&>);

namespace {

TEST(VecTuple, StructuredBindingsPreserveReferences) {
    alia::vec2i a{1, 2};
    auto &[x, y] = a;
    x = 9;
    EXPECT_EQ(a.x, 9);
    EXPECT_EQ(&y, &a.y);
    const alia::vec3i b{3, 4, 5};
    const auto &[p, q, r] = b;
    EXPECT_EQ(&p, &b.x);
    EXPECT_EQ(&q, &b.y);
    EXPECT_EQ(&r, &b.z);
}

TEST(Vec3, ComputesDotProduct) {
    constexpr alia::vec3f a{1.0f, 2.0f, 3.0f};
    constexpr alia::vec3f b{4.0f, -5.0f, 6.0f};

    static_assert(a.dot(b) == 12.0f);
    EXPECT_FLOAT_EQ(a.dot(b), 12.0f);
}

TEST(Vec3, ComputesCrossProduct) {
    constexpr alia::vec3f a{1.0f, 2.0f, 3.0f};
    constexpr alia::vec3f b{4.0f, -5.0f, 6.0f};
    constexpr alia::vec3f expected{27.0f, 6.0f, -13.0f};

    static_assert(a.cross(b) == expected);
    EXPECT_EQ(a.cross(b), expected);
}

TEST(Vec3, WidensIntegralDotProduct) {
    constexpr alia::vec3i value{50'000, 50'000, 50'000};

    static_assert(std::is_same_v<decltype(value.dot(value)), std::int64_t>);
    static_assert(value.dot(value) == 7'500'000'000LL);
    EXPECT_EQ(value.dot(value), 7'500'000'000LL);
}

} // namespace
