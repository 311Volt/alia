#include "alia/util/array_map.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

TEST(ArrayMapTest, BasicOperations) {
    array_map<int, std::string> map;
    
    // Test empty
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0);
    
    // Test insert
    auto [it1, inserted1] = map.insert({5, "five"});
    EXPECT_TRUE(inserted1);
    EXPECT_EQ(it1->first, 5);
    EXPECT_EQ(it1->second, "five");
    EXPECT_EQ(map.size(), 1);
    
    // Test duplicate insert
    auto [it2, inserted2] = map.insert({5, "FIVE"});
    EXPECT_FALSE(inserted2);
    EXPECT_EQ(map.size(), 1);
    EXPECT_EQ(map[5], "five"); // Original value preserved
    
    // Test operator[]
    map[10] = "ten";
    EXPECT_EQ(map.size(), 2);
    EXPECT_EQ(map[10], "ten");
    
    // Test at()
    EXPECT_EQ(map.at(5), "five");
    EXPECT_THROW(map.at(999), std::out_of_range);
}

TEST(ArrayMapTest, Iteration) {
    array_map<int, int> map;
    map[5] = 50;
    map[10] = 100;
    map[2] = 20;
    map[7] = 70;
    
    // Test forward iteration (should be in key order)
    std::vector<std::pair<int, int>> expected = {{2, 20}, {5, 50}, {7, 70}, {10, 100}};
    size_t idx = 0;
    for (const auto& [key, value] : map) {
        ASSERT_LT(idx, expected.size());
        EXPECT_EQ(key, expected[idx].first);
        EXPECT_EQ(value, expected[idx].second);
        ++idx;
    }
    EXPECT_EQ(idx, expected.size());
    
    // Test const iteration
    const auto& const_map = map;
    idx = 0;
    for (auto it = const_map.begin(); it != const_map.end(); ++it) {
        ASSERT_LT(idx, expected.size());
        EXPECT_EQ(it->first, expected[idx].first);
        EXPECT_EQ(it->second, expected[idx].second);
        ++idx;
    }
}

TEST(ArrayMapTest, Lookup) {
    array_map<int, std::string> map;
    map[5] = "five";
    map[10] = "ten";
    
    // Test find
    auto it = map.find(5);
    ASSERT_NE(it, map.end());
    EXPECT_EQ(it->second, "five");
    
    auto it2 = map.find(999);
    EXPECT_EQ(it2, map.end());
    
    // Test count
    EXPECT_EQ(map.count(5), 1);
    EXPECT_EQ(map.count(999), 0);
    
    // Test contains
    EXPECT_TRUE(map.contains(5));
    EXPECT_FALSE(map.contains(999));
}

TEST(ArrayMapTest, Erase) {
    array_map<int, std::string> map;
    map[5] = "five";
    map[10] = "ten";
    map[15] = "fifteen";
    
    // Test erase by key
    EXPECT_EQ(map.size(), 3);
    size_t erased = map.erase(10);
    EXPECT_EQ(erased, 1);
    EXPECT_EQ(map.size(), 2);
    EXPECT_FALSE(map.contains(10));
    
    erased = map.erase(999);
    EXPECT_EQ(erased, 0);
    EXPECT_EQ(map.size(), 2);
    
    // Test erase by iterator
    auto it = map.find(5);
    auto next_it = map.erase(it);
    EXPECT_EQ(map.size(), 1);
    EXPECT_FALSE(map.contains(5));
    EXPECT_EQ(next_it->first, 15);
}

TEST(ArrayMapTest, ShrinkToFit) {
    array_map<int, int> map;
    
    // Insert sparse data
    map[0] = 0;
    map[100] = 100;
    map[50] = 50;
    
    auto [min1, max1] = map.key_range();
    EXPECT_EQ(min1, 0);
    EXPECT_EQ(max1, 100);
    
    // Remove extremes
    map.erase(0);
    map.erase(100);
    
    // Before shrink, range is still wide
    auto [min2, max2] = map.key_range();
    EXPECT_EQ(min2, 0);
    EXPECT_EQ(max2, 100);
    
    // After shrink, range should be tight
    map.shrink_to_fit();
    auto [min3, max3] = map.key_range();
    EXPECT_EQ(min3, 50);
    EXPECT_EQ(max3, 50);
    EXPECT_EQ(map.size(), 1);
    EXPECT_EQ(map[50], 50);
}

TEST(ArrayMapTest, NegativeKeys) {
    array_map<int, std::string> map;
    map[-10] = "minus ten";
    map[-5] = "minus five";
    map[0] = "zero";
    map[5] = "five";
    
    EXPECT_EQ(map.size(), 4);
    
    // Test iteration order
    std::vector<int> expected_keys = {-10, -5, 0, 5};
    size_t idx = 0;
    for (const auto& [key, value] : map) {
        ASSERT_LT(idx, expected_keys.size());
        EXPECT_EQ(key, expected_keys[idx]);
        ++idx;
    }
}

TEST(ArrayMapTest, Emplace) {
    array_map<int, std::string> map;
    
    auto [it1, inserted1] = map.emplace(5, "five");
    EXPECT_TRUE(inserted1);
    EXPECT_EQ(it1->second, "five");
    
    auto [it2, inserted2] = map.emplace(5, "FIVE");
    EXPECT_FALSE(inserted2);
    EXPECT_EQ(map[5], "five");
}

TEST(ArrayMapTest, ClearAndSwap) {
    array_map<int, int> map1;
    map1[1] = 10;
    map1[2] = 20;
    
    array_map<int, int> map2;
    map2[3] = 30;
    
    // Test swap
    map1.swap(map2);
    EXPECT_EQ(map1.size(), 1);
    EXPECT_EQ(map1[3], 30);
    EXPECT_EQ(map2.size(), 2);
    EXPECT_EQ(map2[1], 10);
    
    // Test clear
    map1.clear();
    EXPECT_TRUE(map1.empty());
    EXPECT_EQ(map1.size(), 0);
}

TEST(ArrayMapTest, UnsignedKeys) {
    array_map<unsigned int, std::string> map;
    map[0u] = "zero";
    map[100u] = "hundred";
    map[50u] = "fifty";
    
    EXPECT_EQ(map.size(), 3);
    EXPECT_EQ(map[50u], "fifty");
}

TEST(ArrayMapTest, InitializerList) {
    array_map<int, std::string> map = {
        {1, "one"},
        {2, "two"},
        {3, "three"}
    };
    
    EXPECT_EQ(map.size(), 3);
    EXPECT_EQ(map[1], "one");
    EXPECT_EQ(map[2], "two");
    EXPECT_EQ(map[3], "three");
}

TEST(ArrayMapTest, Comparison) {
    array_map<int, int> map1 = {{1, 10}, {2, 20}};
    array_map<int, int> map2 = {{1, 10}, {2, 20}};
    array_map<int, int> map3 = {{1, 10}, {2, 30}};
    
    EXPECT_TRUE(map1 == map2);
    EXPECT_FALSE(map1 != map2);
    EXPECT_TRUE(map1 != map3);
    EXPECT_FALSE(map1 == map3);
}

TEST(ArrayMapTest, PerformanceDemo) {
    array_map<int, int> map;
    
    // Insert 1000 elements
    for (int i = 0; i < 1000; ++i) {
        map[i * 10] = i;
    }
    
    EXPECT_EQ(map.size(), 1000);
    auto [min_k, max_k] = map.key_range();
    EXPECT_EQ(min_k, 0);
    EXPECT_EQ(max_k, 9990);
    
    // O(1) lookup demonstration
    EXPECT_EQ(map[5000], 500);
    EXPECT_TRUE(map.contains(7500));
    EXPECT_FALSE(map.contains(7501));
}
