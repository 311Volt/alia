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

add_executable(vec_tests
    vec_test.cpp
)

target_link_libraries(vec_tests
    PRIVATE
    alia
    GTest::gtest
    GTest::gtest_main
)

add_executable(primitive_renderer_tests
    primitive_renderer_test.cpp
)

target_link_libraries(primitive_renderer_tests
    PRIVATE
    alia
    GTest::gtest
    GTest::gtest_main
)

add_executable(text_layout_tests
    text_layout_test.cpp
)

target_link_libraries(text_layout_tests
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

add_executable(pixel_bitmap_tests
    pixel_bitmap_test.cpp
)

target_link_libraries(pixel_bitmap_tests
    PRIVATE
    alia
    GTest::gtest
    GTest::gtest_main
)

add_executable(vertex_definition_registry_tests
    vertex_definition_registry_test.cpp
    vertex_definition_registry_test_helper.cpp
)

add_executable(display_mode_tests display_mode_test.cpp)
target_link_libraries(display_mode_tests PRIVATE alia GTest::gtest GTest::gtest_main)

add_executable(framebuffer_config_tests framebuffer_config_test.cpp)
target_link_libraries(framebuffer_config_tests PRIVATE alia GTest::gtest GTest::gtest_main)

add_executable(mouse_tests mouse_test.cpp)
target_link_libraries(mouse_tests PRIVATE alia GTest::gtest GTest::gtest_main)

target_link_libraries(vertex_definition_registry_tests
    PRIVATE
    alia
    GTest::gtest
    GTest::gtest_main
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(cstring_view_tests)
gtest_discover_tests(array_map_tests)
gtest_discover_tests(vec_tests)
gtest_discover_tests(primitive_renderer_tests)
gtest_discover_tests(text_layout_tests)
gtest_discover_tests(soo_any_tests)
gtest_discover_tests(any_deque_tests)
gtest_discover_tests(pixel_bitmap_tests)
gtest_discover_tests(vertex_definition_registry_tests)
gtest_discover_tests(display_mode_tests)
gtest_discover_tests(framebuffer_config_tests)
gtest_discover_tests(mouse_tests)
