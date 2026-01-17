
add_executable(alia_example_hello hello.cpp)
target_link_libraries(alia_example_hello PUBLIC alia)

target_compile_options(alia_example_hello PRIVATE "-ffast-math")




add_executable(alia_example_gfx_basic gfx_basic.cpp)
target_link_libraries(alia_example_gfx_basic PUBLIC alia)
