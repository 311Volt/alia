# Test configuration for alia library

# Create test executables
add_executable(cstring_view_tests
    cstring_view_test.cpp
)

target_link_libraries(cstring_view_tests
    PRIVATE
    alia
    GTest::gtest
    GTest::gtest_main
)

add_executable(array_map_tests
    array_map_test.cpp
)

target_link_libraries(array_map_tests
    PRIVATE
    alia
    GTest::gtest
    GTest::gtest_main
)

add_executable(soo_any_tests
    soo_any_test.cpp
)

target_link_libraries(soo_any_tests
    PRIVATE
    alia
    GTest::gtest
    GTest::gtest_main
)

add_executable(any_deque_tests
    any_deque_test.cpp
)

target_link_libraries(any_deque_tests
    PRIVATE
    alia
    GTest::gtest
    GTest::gtest_main
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(cstring_view_tests)
gtest_discover_tests(array_map_tests)
gtest_discover_tests(soo_any_tests)
gtest_discover_tests(any_deque_tests)
