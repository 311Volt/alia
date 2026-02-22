#include "alia/util/cstring_view.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace literals::cstring_view_literals;

// Basic Usage Tests
TEST(CStringViewTest, DefaultConstruction) {
    cstring_view csv;
    EXPECT_EQ(csv.size(), 0);
    EXPECT_TRUE(csv.empty());
    EXPECT_EQ(csv.data(), nullptr);
}

TEST(CStringViewTest, ConstructionFromCString) {
    cstring_view csv("Hello, World!");
    EXPECT_EQ(csv.size(), 13);
    EXPECT_FALSE(csv.empty());
    EXPECT_STREQ(csv.c_str(), "Hello, World!");
}

TEST(CStringViewTest, ConstructionWithLength) {
    cstring_view csv("Hello", 5);
    EXPECT_EQ(csv.size(), 5);
    EXPECT_STREQ(csv.c_str(), "Hello");
}

TEST(CStringViewTest, LiteralOperator) {
    auto csv = "Hello from literal"_csv;
    EXPECT_EQ(csv.size(), 18);
    EXPECT_STREQ(csv.c_str(), "Hello from literal");
}

TEST(CStringViewTest, NullPointerConstruction) {
    cstring_view csv(nullptr);
    EXPECT_EQ(csv.size(), 0);
    EXPECT_TRUE(csv.empty());
    EXPECT_EQ(csv.data(), nullptr);
}

TEST(CStringViewTest, InvalidNullTerminationThrows) {
    char buffer[5] = {'H', 'e', 'l', 'l', 'o'};
    EXPECT_THROW(cstring_view(buffer, 5), std::invalid_argument);
}

// Iterator Tests
TEST(CStringViewTest, BeginEnd) {
    cstring_view csv("Test");
    EXPECT_EQ(*csv.begin(), 'T');
    EXPECT_EQ(*(csv.end() - 1), 't');
}

TEST(CStringViewTest, ConstBeginEnd) {
    const cstring_view csv("Test");
    EXPECT_EQ(*csv.cbegin(), 'T');
    EXPECT_EQ(*(csv.cend() - 1), 't');
}

TEST(CStringViewTest, ReverseIteration) {
    cstring_view csv("Test");
    auto it = csv.rbegin();
    EXPECT_EQ(*it, 't');
    ++it;
    EXPECT_EQ(*it, 's');
    ++it;
    EXPECT_EQ(*it, 'e');
    ++it;
    EXPECT_EQ(*it, 'T');
    EXPECT_EQ(it + 1, csv.rend());
}

TEST(CStringViewTest, ConstReverseIteration) {
    const cstring_view csv("Test");
    auto it = csv.crbegin();
    EXPECT_EQ(*it, 't');
    EXPECT_EQ(it + 4, csv.crend());
}

// Element Access Tests
TEST(CStringViewTest, SubscriptOperator) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv[0], 'H');
    EXPECT_EQ(csv[4], 'o');
}

TEST(CStringViewTest, AtMethod) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.at(0), 'H');
    EXPECT_EQ(csv.at(4), 'o');
}

TEST(CStringViewTest, AtMethodOutOfBounds) {
    cstring_view csv("Hello");
    EXPECT_THROW(csv.at(5), std::out_of_range);
}

TEST(CStringViewTest, FrontBack) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.front(), 'H');
    EXPECT_EQ(csv.back(), 'o');
}

TEST(CStringViewTest, DataAndCStr) {
    cstring_view csv("Test");
    EXPECT_STREQ(csv.data(), "Test");
    EXPECT_STREQ(csv.c_str(), "Test");
}

// Capacity Tests
TEST(CStringViewTest, SizeAndLength) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.size(), 5);
    EXPECT_EQ(csv.length(), 5);
}

TEST(CStringViewTest, MaxSize) {
    cstring_view csv;
    EXPECT_GT(csv.max_size(), 0);
}

TEST(CStringViewTest, Empty) {
    cstring_view csv1;
    cstring_view csv2("Not empty");
    EXPECT_TRUE(csv1.empty());
    EXPECT_FALSE(csv2.empty());
}

// Modifier Tests
TEST(CStringViewTest, RemovePrefix) {
    cstring_view csv("Hello, World!");
    csv.remove_prefix(7);
    EXPECT_EQ(csv.size(), 6);
    EXPECT_STREQ(csv.c_str(), "World!");
}

TEST(CStringViewTest, RemoveSuffix) {
    cstring_view csv("Hello, World!");
    csv.remove_suffix(1);
    EXPECT_EQ(csv.size(), 12);
    EXPECT_STREQ(csv.c_str(), "Hello, World");
}

TEST(CStringViewTest, Swap) {
    cstring_view csv1("First");
    cstring_view csv2("Second");
    csv1.swap(csv2);
    EXPECT_STREQ(csv1.c_str(), "Second");
    EXPECT_STREQ(csv2.c_str(), "First");
}

// Substring Tests
TEST(CStringViewTest, SubstrSv) {
    cstring_view csv("Hello, World!");
    std::string_view sv = csv.substr_sv(0, 5);
    EXPECT_EQ(sv.size(), 5);
    EXPECT_EQ(sv, "Hello");
}

TEST(CStringViewTest, SubstrSvDefaultCount) {
    cstring_view csv("Hello, World!");
    std::string_view sv = csv.substr_sv(7);
    EXPECT_EQ(sv.size(), 6);
    EXPECT_EQ(sv, "World!");
}

TEST(CStringViewTest, SubstrSvOutOfBounds) {
    cstring_view csv("Hello");
    EXPECT_THROW(csv.substr_sv(10), std::out_of_range);
}

TEST(CStringViewTest, Suffix) {
    cstring_view csv("Hello, World!");
    auto suf = csv.suffix(7);
    EXPECT_EQ(suf.size(), 6);
    EXPECT_STREQ(suf.c_str(), "World!");
}

TEST(CStringViewTest, SuffixFromZero) {
    cstring_view csv("Hello");
    auto suf = csv.suffix(0);
    EXPECT_EQ(suf.size(), 5);
    EXPECT_STREQ(suf.c_str(), "Hello");
}

TEST(CStringViewTest, SuffixAtEnd) {
    cstring_view csv("Hello");
    auto suf = csv.suffix(5);
    EXPECT_TRUE(suf.empty());
    EXPECT_STREQ(suf.c_str(), "");
}

TEST(CStringViewTest, SuffixOutOfBounds) {
    cstring_view csv("Hello");
    EXPECT_THROW(csv.suffix(10), std::out_of_range);
}

TEST(CStringViewTest, SuffixNullTermination) {
    cstring_view csv("Hello, World!");
    auto suf = csv.suffix(7);
    EXPECT_EQ(suf.c_str()[suf.size()], '\0');
}

TEST(CStringViewTest, CreateSubstr) {
    cstring_view csv("Hello, World!");
    auto str = csv.create_substr(0, 5);
    EXPECT_EQ(str, "Hello");
    EXPECT_EQ(typeid(str), typeid(std::string));
}

TEST(CStringViewTest, CreateSubstrOutOfBounds) {
    cstring_view csv("Hello");
    EXPECT_THROW(csv.create_substr(0, 10), std::out_of_range);
}

// Search Tests
TEST(CStringViewTest, FindString) {
    cstring_view csv("The quick brown fox");
    EXPECT_EQ(csv.find("quick"), 4);
    EXPECT_EQ(csv.find("cat"), cstring_view::npos);
}

TEST(CStringViewTest, FindChar) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find('l'), 2);
    EXPECT_EQ(csv.find('z'), cstring_view::npos);
}

TEST(CStringViewTest, FindWithPosition) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.find("Hello", 1), 6);
}

TEST(CStringViewTest, RFind) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.rfind("Hello"), 6);
    EXPECT_EQ(csv.rfind('l'), 9);
}

TEST(CStringViewTest, Contains) {
    cstring_view csv("The quick brown fox");
    EXPECT_TRUE(csv.contains("fox"));
    EXPECT_FALSE(csv.contains("cat"));
    EXPECT_TRUE(csv.contains('q'));
    EXPECT_FALSE(csv.contains('z'));
}

TEST(CStringViewTest, StartsWith) {
    cstring_view csv("Hello, World!");
    EXPECT_TRUE(csv.starts_with("Hello"));
    EXPECT_TRUE(csv.starts_with('H'));
    EXPECT_FALSE(csv.starts_with("World"));
}

TEST(CStringViewTest, EndsWith) {
    cstring_view csv("Hello, World!");
    EXPECT_TRUE(csv.ends_with("World!"));
    EXPECT_TRUE(csv.ends_with('!'));
    EXPECT_FALSE(csv.ends_with("Hello"));
}

// Find First/Last Of Tests
TEST(CStringViewTest, FindFirstOf) {
    cstring_view csv("Hello, World!");
    EXPECT_EQ(csv.find_first_of("aeiou"), 1); // 'e'
    EXPECT_EQ(csv.find_first_of("xyz"), cstring_view::npos);
}

TEST(CStringViewTest, FindLastOf) {
    cstring_view csv("Hello, World!");
    EXPECT_EQ(csv.find_last_of("aeiou"), 8); // 'o' in World
}

TEST(CStringViewTest, FindFirstNotOf) {
    cstring_view csv("Hello, World!");
    EXPECT_EQ(csv.find_first_not_of("Helo, "), 6); // 'W'
}

TEST(CStringViewTest, FindLastNotOf) {
    cstring_view csv("Hello, World!");
    EXPECT_EQ(csv.find_last_not_of("!"), 11); // 'd'
}

// Comparison Tests
TEST(CStringViewTest, Equality) {
    cstring_view csv1("apple");
    cstring_view csv2("apple");
    cstring_view csv3("banana");
    EXPECT_TRUE(csv1 == csv2);
    EXPECT_FALSE(csv1 == csv3);
}

TEST(CStringViewTest, LessThan) {
    cstring_view csv1("apple");
    cstring_view csv2("banana");
    EXPECT_TRUE(csv1 < csv2);
    EXPECT_FALSE(csv2 < csv1);
}

TEST(CStringViewTest, ThreeWayComparison) {
    cstring_view csv1("apple");
    cstring_view csv2("banana");
    cstring_view csv3("apple");
    
    auto cmp1 = csv1 <=> csv2;
    EXPECT_TRUE(std::is_lt(cmp1));
    
    auto cmp2 = csv2 <=> csv1;
    EXPECT_TRUE(std::is_gt(cmp2));
    
    auto cmp3 = csv1 <=> csv3;
    EXPECT_TRUE(std::is_eq(cmp3));
}

TEST(CStringViewTest, CompareMethod) {
    cstring_view csv1("apple");
    cstring_view csv2("banana");
    EXPECT_LT(csv1.compare(csv2), 0);
    EXPECT_GT(csv2.compare(csv1), 0);
    EXPECT_EQ(csv1.compare("apple"), 0);
}

// Conversion Tests
TEST(CStringViewTest, ConversionToStringView) {
    cstring_view csv("Convert me");
    std::string_view sv = csv;
    EXPECT_EQ(sv, "Convert me");
}

TEST(CStringViewTest, PassToStringViewFunction) {
    cstring_view csv("Test");
    auto process = [](std::string_view s) { return s.length(); };
    EXPECT_EQ(process(csv), 4);
}

// C Interoperability Tests
TEST(CStringViewTest, CStringInterop) {
    cstring_view csv("C String");
    const char* c_str = csv.c_str();
    EXPECT_EQ(strlen(c_str), csv.size());
    EXPECT_STREQ(c_str, "C String");
}

// Copy Tests
TEST(CStringViewTest, Copy) {
    cstring_view csv("Hello");
    char buffer[10];
    auto copied = csv.copy(buffer, 3, 1);
    EXPECT_EQ(copied, 3);
    buffer[copied] = '\0';
    EXPECT_STREQ(buffer, "ell");
}

TEST(CStringViewTest, CopyOutOfBounds) {
    cstring_view csv("Hello");
    char buffer[10];
    EXPECT_THROW(csv.copy(buffer, 10, 10), std::out_of_range);
}

// Wide String Tests
TEST(CStringViewTest, WideStringConstruction) {
    wcstring_view wcsv(L"Hello");
    EXPECT_EQ(wcsv.size(), 5);
    EXPECT_EQ(wcsv[0], L'H');
}

TEST(CStringViewTest, WideStringLiteral) {
    auto wcsv = L"Test"_csv;
    EXPECT_EQ(wcsv.size(), 4);
    EXPECT_EQ(wcsv[0], L'T');
}

// UTF-8 String Tests
TEST(CStringViewTest, UTF8StringConstruction) {
    u8cstring_view u8csv(u8"Hello");
    EXPECT_EQ(u8csv.size(), 5);
    EXPECT_EQ(u8csv[0], u8'H');
}

TEST(CStringViewTest, UTF8StringLiteral) {
    auto u8csv = u8"Test"_csv;
    EXPECT_EQ(u8csv.size(), 4);
    EXPECT_EQ(u8csv[0], u8'T');
}

// UTF-16 String Tests
TEST(CStringViewTest, UTF16StringConstruction) {
    u16cstring_view u16csv(u"Hello");
    EXPECT_EQ(u16csv.size(), 5);
    EXPECT_EQ(u16csv[0], u'H');
}

TEST(CStringViewTest, UTF16StringLiteral) {
    auto u16csv = u"Test"_csv;
    EXPECT_EQ(u16csv.size(), 4);
    EXPECT_EQ(u16csv[0], u'T');
}

// UTF-32 String Tests
TEST(CStringViewTest, UTF32StringConstruction) {
    u32cstring_view u32csv(U"Hello");
    EXPECT_EQ(u32csv.size(), 5);
    EXPECT_EQ(u32csv[0], U'H');
}

TEST(CStringViewTest, UTF32StringLiteral) {
    auto u32csv = U"Test"_csv;
    EXPECT_EQ(u32csv.size(), 4);
    EXPECT_EQ(u32csv[0], U'T');
}

// Stream Output Tests
TEST(CStringViewTest, StreamOutput) {
    cstring_view csv("Test");
    std::ostringstream oss;
    oss << csv;
    EXPECT_EQ(oss.str(), "Test");
}

// Empty String Edge Cases
TEST(CStringViewTest, EmptyStringOperations) {
    cstring_view csv("");
    EXPECT_TRUE(csv.empty());
    EXPECT_EQ(csv.size(), 0);
    EXPECT_FALSE(csv.contains("a"));
    EXPECT_FALSE(csv.starts_with("a"));
    EXPECT_FALSE(csv.ends_with("a"));
}

// Single Character Tests
TEST(CStringViewTest, SingleCharacter) {
    cstring_view csv("A");
    EXPECT_EQ(csv.size(), 1);
    EXPECT_EQ(csv.front(), 'A');
    EXPECT_EQ(csv.back(), 'A');
    EXPECT_TRUE(csv.starts_with('A'));
    EXPECT_TRUE(csv.ends_with('A'));
}

// Npos Tests
TEST(CStringViewTest, NposValue) {
    EXPECT_EQ(cstring_view::npos, static_cast<cstring_view::size_type>(-1));
}

TEST(CStringViewTest, FindReturnsNpos) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find("z"), cstring_view::npos);
    EXPECT_EQ(csv.rfind("z"), cstring_view::npos);
    EXPECT_EQ(csv.find_first_of("xyz"), cstring_view::npos);
    EXPECT_EQ(csv.find_last_of("xyz"), cstring_view::npos);
}

// Copy Construction and Assignment Tests
TEST(CStringViewTest, CopyConstruction) {
    cstring_view csv1("Original");
    cstring_view csv2(csv1);
    EXPECT_STREQ(csv2.c_str(), "Original");
}

TEST(CStringViewTest, CopyAssignment) {
    cstring_view csv1("Original");
    cstring_view csv2;
    csv2 = csv1;
    EXPECT_STREQ(csv2.c_str(), "Original");
}

// Deduction Guide Tests
TEST(CStringViewTest, DeductionGuideCString) {
    const char* str = "Test";
    basic_cstring_view csv = str;
    EXPECT_EQ(csv.size(), 4);
}

TEST(CStringViewTest, DeductionGuideCStringWithLength) {
    const char* str = "Test";
    basic_cstring_view csv = basic_cstring_view(str, 2);
    EXPECT_EQ(csv.size(), 2);
}

// Compare Overload Tests
TEST(CStringViewTest, CompareWithPosition) {
    cstring_view csv("Hello World");
    EXPECT_EQ(csv.compare(0, 5, "Hello"), 0);
    EXPECT_NE(csv.compare(6, 5, "Hello"), 0);
}

TEST(CStringViewTest, CompareWithBothPositions) {
    cstring_view csv1("Hello World");
    cstring_view csv2("Hello Universe");
    EXPECT_EQ(csv1.compare(0, 5, csv2, 0, 5), 0);
}

// Find With Count Tests
TEST(CStringViewTest, FindWithCount) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.find("Hello", 0, 5), 0);
    EXPECT_EQ(csv.find("Hello", 1, 5), 6);
}

TEST(CStringViewTest, RFindWithCount) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.rfind("Hello", cstring_view::npos, 5), 6);
}

// Find First/Last Of With Count Tests
TEST(CStringViewTest, FindFirstOfWithCount) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find_first_of("lo", 0, 2), 3);
}

TEST(CStringViewTest, FindLastOfWithCount) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find_last_of("He", cstring_view::npos, 2), 1);
}

TEST(CStringViewTest, FindFirstNotOfWithCount) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find_first_not_of("He", 0, 2), 2);
}

TEST(CStringViewTest, FindLastNotOfWithCount) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find_last_not_of("lo", cstring_view::npos, 2), 1);
}

// Const Correctness Tests
TEST(CStringViewTest, ConstViewIteration) {
    const cstring_view csv("Const");
    auto it = csv.begin();
    EXPECT_EQ(*it, 'C');
    EXPECT_EQ(csv[0], 'C');
    EXPECT_EQ(csv.front(), 'C');
    EXPECT_EQ(csv.back(), 't');
}

TEST(CStringViewTest, NonConstViewIteration) {
    cstring_view csv("NonConst");
    auto it = csv.begin();
    EXPECT_EQ(*it, 'N');
    EXPECT_EQ(csv[0], 'N');
    EXPECT_EQ(csv.front(), 'N');
    EXPECT_EQ(csv.back(), 't');
}

// Multiple Remove Operations Test
TEST(CStringViewTest, MultipleRemoveOperations) {
    cstring_view csv("Hello, World!");
    csv.remove_prefix(7);
    EXPECT_EQ(csv.size(), 6);
    csv.remove_suffix(1);
    EXPECT_EQ(csv.size(), 5);
    EXPECT_STREQ(csv.c_str(), "World");
}

// Empty String Literal Test
TEST(CStringViewTest, EmptyStringLiteral) {
    auto csv = ""_csv;
    EXPECT_TRUE(csv.empty());
    EXPECT_EQ(csv.size(), 0);
}

// Long String Test
TEST(CStringViewTest, LongString) {
    std::string long_str(1000, 'a');
    cstring_view csv(long_str.c_str());
    EXPECT_EQ(csv.size(), 1000);
    EXPECT_TRUE(csv.contains("a"));
    EXPECT_FALSE(csv.contains("b"));
}

// Special Characters Test
TEST(CStringViewTest, SpecialCharacters) {
    cstring_view csv("Hello\n\tWorld!");
    EXPECT_EQ(csv.size(), 12);
    EXPECT_TRUE(csv.contains("\n"));
    EXPECT_TRUE(csv.contains("\t"));
}

// Unicode Characters Test (UTF-8)
TEST(CStringViewTest, UnicodeUTF8) {
    u8cstring_view csv(u8"Hello 世界");
    EXPECT_GT(csv.size(), 5);
    EXPECT_TRUE(csv.contains(u8"世"));
}

// Comparison With Different Types Test
TEST(CStringViewTest, CompareWithCString) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.compare("Hello"), 0);
    EXPECT_NE(csv.compare("World"), 0);
}

TEST(CStringViewTest, CompareWithPositionAndCString) {
    cstring_view csv("Hello World");
    EXPECT_EQ(csv.compare(0, 5, "Hello"), 0);
}

TEST(CStringViewTest, CompareWithPositionCStringAndCount) {
    cstring_view csv("Hello World");
    EXPECT_EQ(csv.compare(0, 5, "Hello", 5), 0);
}

// Starts/Ends With CString Test
TEST(CStringViewTest, StartsWithCString) {
    cstring_view csv("Hello, World!");
    EXPECT_TRUE(csv.starts_with("Hello"));
    EXPECT_FALSE(csv.starts_with("World"));
}

TEST(CStringViewTest, EndsWithCString) {
    cstring_view csv("Hello, World!");
    EXPECT_TRUE(csv.ends_with("World!"));
    EXPECT_FALSE(csv.ends_with("Hello"));
}

// Contains CString Test
TEST(CStringViewTest, ContainsCString) {
    cstring_view csv("The quick brown fox");
    EXPECT_TRUE(csv.contains("quick"));
    EXPECT_FALSE(csv.contains("cat"));
}

// Find CString Test
TEST(CStringViewTest, FindCString) {
    cstring_view csv("Hello World");
    EXPECT_EQ(csv.find("World"), 6);
    EXPECT_EQ(csv.find("xyz"), cstring_view::npos);
}

TEST(CStringViewTest, FindCStringWithPosition) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.find("Hello", 1), 6);
}

TEST(CStringViewTest, FindCStringWithPositionAndCount) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.find("Hello", 0, 5), 0);
    EXPECT_EQ(csv.find("Hello", 1, 5), 6);
}

// RFind CString Test
TEST(CStringViewTest, RFindCString) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.rfind("Hello"), 6);
}

TEST(CStringViewTest, RFindCStringWithPosition) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.rfind("Hello", 5), 0);
}

TEST(CStringViewTest, RFindCStringWithPositionAndCount) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.rfind("Hello", cstring_view::npos, 5), 6);
}

// Find First/Last Of CString Test
TEST(CStringViewTest, FindFirstOfCString) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find_first_of("lo"), 3);
}

TEST(CStringViewTest, FindFirstOfCStringWithPosition) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.find_first_of("H", 1), 6);
}

TEST(CStringViewTest, FindFirstOfCStringWithPositionAndCount) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find_first_of("lo", 0, 2), 3);
}

TEST(CStringViewTest, FindLastOfCString) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find_last_of("l"), 3);
}

TEST(CStringViewTest, FindLastOfCStringWithPosition) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.find_last_of("H", 5), 0);
}

TEST(CStringViewTest, FindLastOfCStringWithPositionAndCount) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find_last_of("He", cstring_view::npos, 2), 1);
}

// Find First/Last Not Of CString Test
TEST(CStringViewTest, FindFirstNotOfCString) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find_first_not_of("He"), 2);
}

TEST(CStringViewTest, FindFirstNotOfCStringWithPosition) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.find_first_not_of("H", 1), 1);
}

TEST(CStringViewTest, FindFirstNotOfCStringWithPositionAndCount) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find_first_not_of("He", 0, 2), 2);
}

TEST(CStringViewTest, FindLastNotOfCString) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find_last_not_of("lo"), 1);
}

TEST(CStringViewTest, FindLastNotOfCStringWithPosition) {
    cstring_view csv("Hello Hello");
    EXPECT_EQ(csv.find_last_not_of("H", 5), 5);
}

TEST(CStringViewTest, FindLastNotOfCStringWithPositionAndCount) {
    cstring_view csv("Hello");
    EXPECT_EQ(csv.find_last_not_of("lo", cstring_view::npos, 2), 1);
}
