
cmake_minimum_required(VERSION 3.10)
project(alia_examples)

add_executable(alia_example_hello hello.cpp)
target_link_libraries(alia_example_hello PRIVATE alia)
add_custom_command(TARGET alia_example_hello POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:alia_example_hello>/resources")

add_executable(alia_example_shader shader.cpp)
target_link_libraries(alia_example_shader PRIVATE alia)

add_executable(alia_example_buffered_mesh buffered_mesh.cpp)
target_link_libraries(alia_example_buffered_mesh PRIVATE alia)

add_executable(alia_example_render_target render_target.cpp)
target_link_libraries(alia_example_render_target PRIVATE alia)

add_executable(alia_example_mdspan mdspan_example.cpp)
target_link_libraries(alia_example_mdspan PRIVATE alia)
