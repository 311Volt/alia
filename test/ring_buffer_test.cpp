#include "alia/audio/ring_buffer.hpp"
#include <gtest/gtest.h>

static_assert(std::ranges::random_access_range<alia::ring_buffer<float>>);
static_assert(std::ranges::random_access_range<const alia::ring_buffer<float>>);

TEST(RingBuffer, PartialOperationsAcrossWrap) {
    alia::ring_buffer<float> buffer(4);
    const std::array input{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    EXPECT_EQ(buffer.push(input), 4);
    EXPECT_TRUE(buffer.full());
    std::array<float, 3> first;
    EXPECT_EQ(buffer.pop(first), 3);
    EXPECT_EQ(first, (std::array{1.0f, 2.0f, 3.0f}));
    EXPECT_EQ(buffer.push(std::span(input).first(3)), 3);
    std::array<float, 5> result{};
    EXPECT_EQ(buffer.pop(result), 4);
    EXPECT_EQ(result, (std::array{4.0f, 1.0f, 2.0f, 3.0f, 0.0f}));
    EXPECT_TRUE(buffer.empty());
}
TEST(RingBuffer, AllOrNothingAndZeroCapacity) {
    alia::ring_buffer<int> buffer(2);
    const std::array input{1, 2, 3};
    EXPECT_FALSE(buffer.try_push(input));
    EXPECT_TRUE(buffer.empty());
    EXPECT_TRUE(buffer.try_push(std::span(input).first(1)));
    std::array<int, 2> output{99, 99};
    EXPECT_FALSE(buffer.try_pop(output));
    EXPECT_EQ(output[0], 99);
    EXPECT_EQ(buffer.size(), 1);
    EXPECT_TRUE(buffer.try_pop(std::span(output).first(1)));
    alia::ring_buffer<int> zero(0);
    EXPECT_EQ(zero.push(input), 0);
    EXPECT_EQ(zero.pop(output), 0);
    EXPECT_TRUE(zero.readable_spans()[0].empty());
    EXPECT_TRUE(zero.writable_spans()[1].empty());
}
TEST(RingBuffer, SpansIteratorsAndReserveAfterWrap) {
    alia::ring_buffer<float> buffer(4);
    const std::array initial{1.0f, 2.0f, 3.0f};
    buffer.push(initial);
    buffer.discard(2);
    auto writable = buffer.writable_spans();
    ASSERT_EQ(writable[0].size(), 1);
    ASSERT_EQ(writable[1].size(), 2);
    writable[0][0] = 4;
    writable[1][0] = 5;
    writable[1][1] = 6;
    buffer.commit_push(3);
    const auto &constant = buffer;
    EXPECT_EQ(constant.front(), 3);
    EXPECT_EQ(constant.back(), 6);
    EXPECT_EQ(constant.end() - constant.begin(), 4);
    EXPECT_EQ(*(2 + buffer.begin()), 5);
    EXPECT_EQ(buffer.begin()[3], 6);
    EXPECT_EQ((buffer.end() - 1)[0], 6);
    EXPECT_TRUE(buffer.begin() < buffer.end());
    EXPECT_EQ(constant.readable_spans()[0].size(), 2);
    EXPECT_EQ(constant.readable_spans()[1].size(), 2);
    buffer.reserve(9);
    EXPECT_EQ(buffer.capacity(), 9);
    EXPECT_EQ(buffer.readable_spans()[0].size(), 4);
    EXPECT_TRUE(buffer.readable_spans()[1].empty());
    EXPECT_EQ(buffer.at(0), 3);
    EXPECT_EQ(buffer.at(3), 6);
    EXPECT_THROW(buffer.at(4), std::out_of_range);
    EXPECT_THROW(buffer.discard(5), std::out_of_range);
    EXPECT_THROW(buffer.commit_push(6), std::out_of_range);
    std::ranges::reverse(buffer);
    EXPECT_EQ(buffer.front(), 6);
    EXPECT_EQ(buffer.back(), 3);
    buffer.clear();
    EXPECT_EQ(buffer.free_space(), 9);
}
