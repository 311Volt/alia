#include "alia/core/vec.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

namespace {

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
