#include <gtest/gtest.h>
#include <alia/util/any_deque.hpp>
#include <string>
#include <vector>

struct EventA {
    int x;
    EventA(int x) : x(x) {}
    bool operator==(const EventA& o) const { return x == o.x; }
};

struct EventB {
    std::string s;
    double d;
    EventB(std::string s, double d) : s(std::move(s)), d(d) {}
    bool operator==(const EventB& o) const { return s == o.s && d == o.d; }
};

TEST(AnyDequeTest, BasicPushPop) {
    any_deque q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0);

    q.push_back(EventA{42});
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 1);
    EXPECT_EQ(q.back_as<EventA>().x, 42);
    EXPECT_EQ(q.front_as<EventA>().x, 42);

    q.push_front(EventB{"hello", 3.14});
    EXPECT_EQ(q.size(), 2);
    EXPECT_EQ(q.front_as<EventB>().s, "hello");
    EXPECT_EQ(q.back_as<EventA>().x, 42);

    q.pop_back();
    EXPECT_EQ(q.size(), 1);
    EXPECT_EQ(q.front_as<EventB>().s, "hello");

    q.pop_front();
    EXPECT_TRUE(q.empty());
}

TEST(AnyDequeTest, Iteration) {
    any_deque q;
    q.push_back(EventA{1});
    q.push_back(EventB{"two", 2.0});
    q.push_back(EventA{3});

    auto it = q.begin();
    ASSERT_NE(it, q.end());
    EXPECT_TRUE(any_deque::holds_type<EventA>(it));
    EXPECT_EQ(it.as<EventA>().x, 1);

    ++it;
    ASSERT_NE(it, q.end());
    EXPECT_TRUE(any_deque::holds_type<EventB>(it));
    EXPECT_EQ(it.as<EventB>().s, "two");

    ++it;
    ASSERT_NE(it, q.end());
    EXPECT_TRUE(any_deque::holds_type<EventA>(it));
    EXPECT_EQ(it.as<EventA>().x, 3);

    ++it;
    EXPECT_EQ(it, q.end());
}

TEST(AnyDequeTest, CopyAndMove) {
    any_deque q1;
    q1.push_back(EventA{10});
    q1.push_back(EventB{"test", 1.5});

    any_deque q2 = q1;
    EXPECT_EQ(q2.size(), 2);
    EXPECT_EQ(q2.front_as<EventA>().x, 10);
    EXPECT_EQ(q2.back_as<EventB>().s, "test");

    any_deque q3 = std::move(q1);
    EXPECT_EQ(q3.size(), 2);
    EXPECT_TRUE(q1.empty());
    EXPECT_EQ(q3.front_as<EventA>().x, 10);
}

TEST(AnyDequeTest, InsertAndErase) {
    any_deque q;
    q.push_back(EventA{1});
    q.push_back(EventA{3});

    auto it = q.begin();
    ++it;
    q.insert(it, EventB{"two", 2.0});

    EXPECT_EQ(q.size(), 3);
    it = q.begin();
    EXPECT_EQ(it.as<EventA>().x, 1);
    ++it;
    EXPECT_EQ(it.as<EventB>().s, "two");
    ++it;
    EXPECT_EQ(it.as<EventA>().x, 3);

    it = q.begin();
    ++it;
    q.erase(it);

    EXPECT_EQ(q.size(), 2);
    EXPECT_EQ(q.front_as<EventA>().x, 1);
    EXPECT_EQ(q.back_as<EventA>().x, 3);
}

TEST(AnyDequeTest, LargeVolumeContiguityBug) {
    // Push many elements to force std::deque chunk boundaries
    any_deque q;
    for (int i = 0; i < 10000; ++i) {
        if (i % 2 == 0) {
            q.push_back(EventA{i});
        } else {
            q.push_back(EventB{"str" + std::to_string(i), (double)i});
        }
    }

    EXPECT_EQ(q.size(), 10000);

    int i = 0;
    for (auto it = q.begin(); it != q.end(); ++it, ++i) {
        if (i % 2 == 0) {
            EXPECT_EQ(it.as<EventA>().x, i);
        } else {
            EXPECT_EQ(it.as<EventB>().s, "str" + std::to_string(i));
        }
    }
}
