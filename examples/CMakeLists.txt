
cmake_minimum_required(VERSION 3.10)
project(alia_examples)

add_executable(alia_example_hello hello.cpp)
target_link_libraries(alia_example_hello PRIVATE alia)

add_executable(alia_example_mdspan mdspan_example.cpp)
target_link_libraries(alia_example_mdspan PRIVATE alia)
