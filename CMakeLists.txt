cmake_minimum_required(VERSION 3.10.0)
project(alia VERSION 0.1.0)

if (NOT DEFINED ALIA_MASTER_PROJECT)
	set(ALIA_MASTER_PROJECT OFF)
	if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
		set(ALIA_MASTER_PROJECT ON)
	endif ()
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)


include(CheckCXXSourceCompiles)
check_cxx_source_compiles([[
  #if __cpp_lib_mdspan < 202207L
  #error "__cpp_lib_mdspan is too old"
  #endif
  #if __cpp_lib_submdspan < 202403L
  #error "__cpp_lib_submdspan is too old"
  #endif
  int main() { return 0; }
]] ALIA_MDSPAN_FEATURE_TEST_OK)

file(GLOB_RECURSE alia_headers "src/*.hpp" "src/*.h")
file(GLOB_RECURSE alia_sources "src/*.cpp" "src/*.c")

add_library(alia ${alia_headers} ${alia_sources})

# include("backends.cmake")

option(ALIA_USE_KOKKOS_MDSPAN "Use Kokkos MDSPAN implementation instead of std::mdspan" ON)
option(ALIA_BUILD_EXAMPLES "Build example programs" ON)

if (NOT ALIA_MDSPAN_FEATURE_TEST_OK)
  set(ALIA_USE_KOKKOS_MDSPAN ON)
endif()

if (ALIA_USE_KOKKOS_MDSPAN)
  find_package(mdspan CONFIG REQUIRED)
  if(TARGET std::mdspan)
    target_link_libraries(alia PUBLIC std::mdspan)
  else()
    target_link_libraries(alia PUBLIC mdspan::mdspan)
  endif()
  target_compile_definitions(alia PUBLIC ALIA_USE_KOKKOS_MDSPAN)
endif()

target_include_directories(alia PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>)

include(GNUInstallDirs)
install(TARGETS alia
    EXPORT alia-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

foreach(header ${alia_headers})
    file(RELATIVE_PATH relative_header "${CMAKE_CURRENT_SOURCE_DIR}/src" "${header}")
    get_filename_component(header_dir "${relative_header}" DIRECTORY)
    install(FILES "${header}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${header_dir}")
endforeach()

install(EXPORT alia-targets
    FILE alia-config.cmake
    NAMESPACE alia::
    DESTINATION lib/cmake/alia
)

if(ALIA_BUILD_EXAMPLES)
  add_subdirectory("examples")
endif()

# Testing
option(ALIA_BUILD_TESTS "Build tests" ON)
if(ALIA_BUILD_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  add_subdirectory("test")
endif()
