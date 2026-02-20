#include "alia/util/soo_any.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

// Test struct for small object optimization (fits in 32-byte buffer)
struct SmallObject {
    int x;
    int y;
    
    SmallObject(int x_ = 0, int y_ = 0) : x(x_), y(y_) {}
    
    SmallObject(const SmallObject& other) : x(other.x), y(other.y) {}
    
    SmallObject(SmallObject&& other) noexcept : x(other.x), y(other.y) {}
    
    ~SmallObject() = default;
};

// Test struct that doesn't fit in small buffer (100 bytes)
struct LargeBuffer {
    char data[100];
    
    LargeBuffer(char fill = 'X') {
        std::fill_n(data, 100, fill);
    }
    
    LargeBuffer(const LargeBuffer& other) {
        std::copy_n(other.data, 100, data);
    }
    
    LargeBuffer(LargeBuffer&& other) noexcept {
        std::copy_n(other.data, 100, data);
    }
    
    ~LargeBuffer() = default;
};

// Test struct with non-trivial data structure (vector)
struct ComplexObject {
    std::vector<int> data;
    
    ComplexObject(int size = 10) : data(size, 42) {}
    
    ComplexObject(const ComplexObject& other) : data(other.data) {}
    
    ComplexObject(ComplexObject&& other) noexcept : data(std::move(other.data)) {}
    
    ~ComplexObject() = default;
};

TEST(SooAnyTest, BasicOperations) {
    // Empty any
    soo_any<32> a1;
    EXPECT_FALSE(a1.has_value());
    EXPECT_EQ(a1.type(), typeid(void));
    
    // Store int (small object)
    a1 = 42;
    EXPECT_TRUE(a1.has_value());
    EXPECT_EQ(a1.type(), typeid(int));
    EXPECT_EQ(any_cast<int>(a1), 42);
    
    // Store string
    soo_any<32> a2 = std::string("Hello, World!");
    EXPECT_TRUE(a2.has_value());
    EXPECT_EQ(any_cast<std::string>(a2), "Hello, World!");
    
    // Reset
    a2.reset();
    EXPECT_FALSE(a2.has_value());
}

TEST(SooAnyTest, SmallObjectOptimization) {
    soo_any<32> a;
    
    // SmallObject fits in 32-byte buffer (2 ints = 8 bytes)
    a = SmallObject(10, 20);
    
    auto* ptr = any_cast<SmallObject>(&a);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->x, 10);
    EXPECT_EQ(ptr->y, 20);
}

TEST(SooAnyTest, NonSOOStorage) {
    soo_any<32> a;  // 32-byte buffer, LargeBuffer is 100 bytes
    
    a = LargeBuffer('A');
    
    auto* ptr = any_cast<LargeBuffer>(&a);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->data[0], 'A');
    EXPECT_EQ(ptr->data[99], 'A');
}

TEST(SooAnyTest, ComplexObjectWithVector) {
    soo_any<32> a;
    
    // ComplexObject contains a vector, but the object itself may fit in buffer
    a = ComplexObject(50);
    
    auto* ptr = any_cast<ComplexObject>(&a);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->data.size(), 50);
    EXPECT_EQ(ptr->data[0], 42);
}

TEST(SooAnyTest, CopySemantics) {
    soo_any<32> a1 = SmallObject(1, 2);
    
    soo_any<32> a2 = a1;
    
    EXPECT_TRUE(a1.has_value());
    EXPECT_TRUE(a2.has_value());
    EXPECT_EQ(any_cast<SmallObject>(a1).x, 1);
    EXPECT_EQ(any_cast<SmallObject>(a2).x, 1);
    
    soo_any<32> a3;
    a3 = a1;
    EXPECT_EQ(any_cast<SmallObject>(a3).x, 1);
}

TEST(SooAnyTest, CopySemanticsNonSOO) {
    soo_any<32> a1 = LargeBuffer('B');
    
    soo_any<32> a2 = a1;
    
    EXPECT_TRUE(a1.has_value());
    EXPECT_TRUE(a2.has_value());
    EXPECT_EQ(any_cast<LargeBuffer>(a1).data[0], 'B');
    EXPECT_EQ(any_cast<LargeBuffer>(a2).data[0], 'B');
}

TEST(SooAnyTest, MoveSemantics) {
    soo_any<32> a1 = SmallObject(3, 4);
    
    soo_any<32> a2 = std::move(a1);
    
    EXPECT_FALSE(a1.has_value());
    EXPECT_TRUE(a2.has_value());
    EXPECT_EQ(any_cast<SmallObject>(a2).x, 3);
    
    soo_any<32> a3;
    a3 = std::move(a2);
    EXPECT_FALSE(a2.has_value());
    EXPECT_EQ(any_cast<SmallObject>(a3).x, 3);
}

TEST(SooAnyTest, MoveSemanticsNonSOO) {
    soo_any<32> a1 = LargeBuffer('C');
    
    soo_any<32> a2 = std::move(a1);
    
    EXPECT_FALSE(a1.has_value());
    EXPECT_TRUE(a2.has_value());
    EXPECT_EQ(any_cast<LargeBuffer>(a2).data[0], 'C');
    
    soo_any<32> a3;
    a3 = std::move(a2);
    EXPECT_FALSE(a2.has_value());
    EXPECT_EQ(any_cast<LargeBuffer>(a3).data[0], 'C');
}

TEST(SooAnyTest, Emplace) {
    soo_any<32> a;
    
    auto& obj = a.emplace<SmallObject>(5, 6);
    
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(obj.x, 5);
    EXPECT_EQ(obj.y, 6);
    EXPECT_EQ(any_cast<SmallObject>(a).x, 5);
}

TEST(SooAnyTest, EmplaceNonSOO) {
    soo_any<32> a;
    
    auto& obj = a.emplace<LargeBuffer>('D');
    
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(obj.data[0], 'D');
    EXPECT_EQ(any_cast<LargeBuffer>(a).data[0], 'D');
}

TEST(SooAnyTest, AnyCast) {
    soo_any<32> a = 123;
    
    // Pointer cast
    int* ptr = any_cast<int>(&a);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 123);
    
    // Wrong type pointer cast
    double* dptr = any_cast<double>(&a);
    EXPECT_EQ(dptr, nullptr);
    
    // Reference cast
    int& ref = any_cast<int&>(a);
    EXPECT_EQ(ref, 123);
    ref = 456;
    EXPECT_EQ(any_cast<int>(a), 456);
    
    // Value cast (creates copy)
    int val = any_cast<int>(a);
    EXPECT_EQ(val, 456);
    
    // Exception on wrong type
    EXPECT_THROW(any_cast<double>(a), bad_soo_any_cast);
}

TEST(SooAnyTest, DifferentBufferSizes) {
    // Very small buffer
    soo_any<8> a1 = 42;  // int fits
    EXPECT_EQ(any_cast<int>(a1), 42);
    
    // Large buffer
    soo_any<128> a2 = std::string("This is a longer string that might fit in the larger buffer");
    EXPECT_TRUE(a2.has_value());
    
    // Custom buffer size for specific use case
    struct Medium { 
        long long data[3]; 
        Medium() : data{1, 2, 3} {}
    };
    
    soo_any<32> a3 = Medium();  // 24 bytes, fits in 32-byte buffer
    EXPECT_TRUE(a3.has_value());
}

TEST(SooAnyTest, Swap) {
    soo_any<32> a1 = 42;
    soo_any<32> a2 = std::string("hello");
    
    a1.swap(a2);
    
    EXPECT_EQ(any_cast<std::string>(a1), "hello");
    EXPECT_EQ(any_cast<int>(a2), 42);
}

TEST(SooAnyTest, SwapNonSOO) {
    soo_any<32> a1 = LargeBuffer('E');
    soo_any<32> a2 = SmallObject(1, 2);
    
    a1.swap(a2);
    
    EXPECT_EQ(any_cast<SmallObject>(a1).x, 1);
    EXPECT_EQ(any_cast<LargeBuffer>(a2).data[0], 'E');
}

TEST(SooAnyTest, TypeInfo) {
    soo_any<32> a;
    EXPECT_EQ(a.type(), typeid(void));
    
    a = 42;
    EXPECT_EQ(a.type(), typeid(int));
    
    a = std::string("test");
    EXPECT_EQ(a.type(), typeid(std::string));
}

TEST(SooAnyTest, SelfAssignment) {
    soo_any<32> a = SmallObject(7, 8);
    
    a = a;  // Self-assignment should work
    
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(any_cast<SmallObject>(a).x, 7);
    EXPECT_EQ(any_cast<SmallObject>(a).y, 8);
}

TEST(SooAnyTest, SelfMoveAssignment) {
    soo_any<32> a = SmallObject(9, 10);
    
    a = std::move(a);  // Self move-assignment should work
    
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(any_cast<SmallObject>(a).x, 9);
    EXPECT_EQ(any_cast<SmallObject>(a).y, 10);
}

TEST(SooAnyTest, MakeAny) {
    auto a = make_any<SmallObject, 32>(11, 12);
    
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(any_cast<SmallObject>(a).x, 11);
    EXPECT_EQ(any_cast<SmallObject>(a).y, 12);
}

TEST(SooAnyTest, MakeAnyNonSOO) {
    auto a = make_any<LargeBuffer, 32>('F');
    
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(any_cast<LargeBuffer>(a).data[0], 'F');
}
